/*
 * server.js
 *
 * - ESP32 gửi tốc độ + áp suất + GPS qua WebSocket /ws
 * - Server:
 *    + Hỏi OSM/Overpass để lấy giới hạn tốc độ
 *    + Trả kết quả so sánh cho ESP32 (overMax / underMin)
 *    + Lưu mẫu vào MongoDB (collection "telemetry")
 *    + Broadcast mẫu mới cho web dashboard (client WebSocket)
 *
 * - Web (trình duyệt):
 *    + Kết nối WebSocket /ws, gửi { type: "dashboard_subscribe" }
 *    + Nhận "telemetry_update" để update realtime
 *    + Gọi REST /history để lấy lịch sử từ MongoDB
 */

import express from "express";
import fetch from "node-fetch";
import http from "http";
import { WebSocketServer } from "ws";
import path from "path";
import { fileURLToPath } from "url";
import { MongoClient } from "mongodb";   // <= MONGO

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// ====== Config chung ======
const OVERPASS_URL = process.env.OVERPASS_URL || "https://overpass-api.de/api/interpreter";
const PORT = process.env.PORT || 3000;
const CACHE_TTL_MS = 60 * 60 * 1000; // 1h
const DEFAULT_MARGIN = 5;             // km/h

// ====== Config MongoDB ======
const MONGO_URL = process.env.MONGO_URL || "mongodb://127.0.0.1:27017";
const MONGO_DB_NAME = process.env.MONGO_DB_NAME || "vehicle_monitor";

let mongoClient;
let telemetryCol; // collection lưu dữ liệu

async function initMongo() {
  try {
    mongoClient = new MongoClient(MONGO_URL);
    await mongoClient.connect();
    const db = mongoClient.db(MONGO_DB_NAME);
    telemetryCol = db.collection("telemetry");
    console.log(`[Mongo] Connected to ${MONGO_URL}, db=${MONGO_DB_NAME}, collection=telemetry`);
  } catch (err) {
    console.error("[Mongo] Connect error:", err);
  }
}

// ====== Express + HTTP server ======
const app = express();
app.use(express.json());
const server = http.createServer(app);

// Phục vụ file tĩnh (HTML, CSS, JS) từ thư mục public
app.use(express.static(path.join(__dirname, "public")));

// Nếu người dùng truy cập "/", gửi file main.html
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "main.html"));
});

// ==== Tiny cache cho Overpass ====
const cache = new Map(); // key -> { value, expires }
const coordKey = (lat, lng) => `${lat.toFixed(5)},${lng.toFixed(5)}`;
const setCache = (k, v, ttl = CACHE_TTL_MS) => cache.set(k, { v, e: Date.now() + ttl });
const getCache = (k) => {
  const c = cache.get(k);
  if (!c) return null;
  if (c.e < Date.now()) { cache.delete(k); return null; }
  return c.v;
};

// Logging helpers
const ts = () => new Date().toLocaleString("vi-VN", { hour12: false });
const log = (...args) => console.log(`[${ts()}]`, ...args);
const round = (x, d = 3) => (x == null ? null : Math.round(x * 10 ** d) / 10 ** d);
const shortCoord = (lat, lng) => `${round(lat, 5)},${round(lng, 5)}`;

// Helpers
function parseSpeed(raw) {
  if (!raw) return null;
  const s = String(raw).trim().toLowerCase();
  if (s.includes("mph")) {
    const m = s.match(/(\d+(\.\d+)?)/);
    if (!m) return null;
    const mph = parseFloat(m[1]);
    return { value: Math.round(mph * 1.60934), unit: "km/h", raw };
  }
  const m = s.match(/(\d+(\.\d+)?)/);
  if (m) return { value: parseFloat(m[1]), unit: "km/h", raw };
  return null;
}
const roughDist2 = (a, b, c, d) => (a - c) * (a - c) + (b - d) * (b - d);

// Fallback MAX theo loại đường
const FALLBACK_MAX_BY_HIGHWAY = {
  motorway: 100,
  trunk: 80,
  primary: 60,
  secondary: 60,
  tertiary: 50,
  residential: 40,
  service: 20,
};

// Core: tìm giới hạn tốc độ gần (lat,lng) từ OSM
async function queryOSMSpeeds(lat, lng) {
  const k = coordKey(lat, lng);
  const cached = getCache(k);
  if (cached) return cached;

  const buildQuery = (r) => `
    [out:json][timeout:25];
    (
      way(around:${r},${lat},${lng})["highway"];
    );
    out tags center 50;
  `;

  for (const r of [50, 150, 300]) {
    const q = buildQuery(r);
    const resp = await fetch(OVERPASS_URL, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({ data: q }).toString(),
    });
    if (!resp.ok) continue;

    const data = await resp.json();
    const els = Array.isArray(data.elements) ? data.elements : [];
    if (!els.length) continue;

    let best = null;
    let bestScore = Infinity;
    for (const e of els) {
      const t = e.tags || {};
      const center = e.center;
      const maxRaw = t.maxspeed || t["maxspeed:forward"] || t["maxspeed:backward"] || null;
      const minRaw = t.minspeed || t["minspeed:forward"] || t["minspeed:backward"] || null;
      const hasAny = maxRaw || minRaw;

      let dist2 = 1e12;
      if (center) dist2 = roughDist2(lat, lng, center.lat, center.lon);
      const score = (hasAny ? 0 : 1e6) + dist2;
      if (score < bestScore) {
        bestScore = score;
        best = { wayId: e.id, highway: t.highway || null, maxRaw, minRaw };
      }
    }

    if (best) {
      const maxParsed = parseSpeed(best.maxRaw);
      const minParsed = parseSpeed(best.minRaw);

      let out = {
        source: "osm-overpass",
        wayId: best.wayId,
        highway: best.highway,
        max: { value: maxParsed?.value ?? null, unit: "km/h", raw: best.maxRaw ?? null, fallbackUsed: false },
        min: { value: minParsed?.value ?? null, unit: "km/h", raw: best.minRaw ?? null, fallbackUsed: false },
      };

      if (out.max.value == null) {
        const fb = FALLBACK_MAX_BY_HIGHWAY[out.highway] ?? null;
        if (fb != null) {
          out.max = { value: fb, unit: "km/h", raw: null, fallbackUsed: true };
        }
      }
      setCache(k, out);
      return out;
    }
  }
  return null;
}

// ===== REST: GET /limit & POST /compare =====
app.get("/limit", async (req, res) => {
  const lat = parseFloat(req.query.lat), lng = parseFloat(req.query.lng);
  if (!Number.isFinite(lat) || !Number.isFinite(lng)) return res.status(400).json({ error: "invalid_lat_lng" });
  try {
    const r = await queryOSMSpeeds(lat, lng);
    if (!r) return res.status(404).json({ error: "no_highway_or_no_data" });
    return res.json({
      source: r.source, wayId: r.wayId, highway: r.highway,
      maxspeed: r.max.value, maxRaw: r.max.raw, maxFallbackUsed: r.max.fallbackUsed,
      minspeed: r.min.value, minRaw: r.min.raw, minFallbackUsed: r.min.fallbackUsed,
      unit: "km/h",
    });
  } catch (e) {
    console.error(e);
    return res.status(500).json({ error: "server_error" });
  }
});

app.post("/compare", async (req, res) => {
  const { lat, lng, speedKmh, margin } = req.body || {};
  const peer = req.socket?.remoteAddress || "http";
  if (!Number.isFinite(lat) || !Number.isFinite(lng) || !Number.isFinite(speedKmh)) {
    log("REST  IN ", peer, "invalid_payload:", req.body);
    return res.status(400).json({ error: "invalid_payload" });
  }
  const marginKmh = Number.isFinite(margin) ? margin : DEFAULT_MARGIN;
  log("REST  IN ", peer, { pos: shortCoord(lat, lng), speedKmh: round(speedKmh), margin: marginKmh });

  try {
    const r = await queryOSMSpeeds(lat, lng);
    if (!r) {
      log("REST OUT", peer, "no_highway_or_no_data");
      return res.status(404).json({ error: "no_highway_or_no_data" });
    }

    const limitMax = r.max.value;    // km/h
    const limitMin = r.min.value;    // km/h (thường null)
    const overMax  = limitMax != null ? (speedKmh > limitMax + marginKmh) : false;
    const underMin = limitMin != null ? (speedKmh < Math.max(0, limitMin - marginKmh)) : false;

    const payload = {
      limitKmh: limitMax,
      minKmh: limitMin,
      overMax,
      underMin,
      highway: r.highway,
      unit: "km/h",
      note:
        (r.min.value == null ? "Không có minspeed trong OSM. " : "")
    };
    log("REST OUT", peer, { limitKmh: payload.limitKmh, minKmh: payload.minKmh, overMax, underMin, highway: payload.highway, fbMax: r.max.fallbackUsed, fbMin: r.min.fallbackUsed });
    return res.json(payload);
  } catch (e) {
    console.error(e);
    return res.status(500).json({ error: "server_error" });
  }
});

// ===== REST: GET /history – lấy dữ liệu từ MongoDB =====
app.get("/history", async (req, res) => {
  if (!telemetryCol) return res.status(503).json({ error: "mongo_not_ready" });

  const limit = Math.min(parseInt(req.query.limit) || 100, 1000);

  let query = {};
  const from = req.query.from ? new Date(req.query.from) : null;
  const to   = req.query.to   ? new Date(req.query.to)   : null;
  const plate = req.query.plate ? String(req.query.plate).trim().toUpperCase() : null;
if (plate) query.licensePlate = plate;

  if (from || to) {
    query.timestamp = {};
    if (from) query.timestamp.$gte = from;
    if (to)   query.timestamp.$lte = to;
  }

  try {
    const docs = await telemetryCol.find(query)
      .sort({ timestamp: -1 })
      .limit(limit)
      .toArray();
    res.json(docs);
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "server_error" });
  }
});

// ===== REST: GET /plates – danh sách biển số đang có trong DB =====
app.get("/plates", async (req, res) => {
  if (!telemetryCol) return res.status(503).json({ error: "mongo_not_ready" });

  try {
    const plates = await telemetryCol.distinct("licensePlate", {
      licensePlate: { $ne: null },
    });

    // lọc null / rỗng + sort
    const clean = plates
      .map((p) => String(p || "").trim())
      .filter((p) => p.length > 0)
      .sort();

    res.json(clean);
  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "server_error" });
  }
});



// ====== WebSocket ======
const wss = new WebSocketServer({ noServer: true });

// Rate-limit xử lý mỗi client (ms)
const MIN_HANDLE_INTERVAL = 300; // 0.3s
let WS_ID_SEQ = 1;

wss.on("connection", (ws, req) => {
  ws.isAlive = true;
  ws.lastHandledAt = 0;
  ws.id = WS_ID_SEQ++;
  ws.peer = req.socket?.remoteAddress || "ws";
  ws.clientType = "unknown"; // "device" (ESP32) | "dashboard"

  log("WS  OPEN", `#${ws.id}`, ws.peer);

  ws.on("pong", () => (ws.isAlive = true));

  ws.on("message", async (msg) => {
    let payload;
    try {
      payload = JSON.parse(msg.toString());
    } catch {
      log("WS  IN  ", `#${ws.id}`, ws.peer, "invalid_json:", msg.toString());
      return ws.send(JSON.stringify({ error: "invalid_json" }));
    }

    // ---- Dashboard đăng ký nhận realtime ----
    if (payload.type === "dashboard_subscribe") {
      ws.clientType = "dashboard";
      log("WS  IN  ", `#${ws.id}`, ws.peer, "dashboard_subscribed");
      ws.send(JSON.stringify({ type: "dashboard_ack" }));
      return;
    }

    // ---- Mặc định: client là thiết bị (ESP32) ----
    if (ws.clientType === "unknown") ws.clientType = "device";

    const { lat, lng, speedKmh, margin, pressureBar } = payload || {};
const plateRaw = (payload.licensePlate || payload.plate || "").toString().trim();
const licensePlate = plateRaw ? plateRaw.toUpperCase() : null;

    if (!Number.isFinite(lat) || !Number.isFinite(lng) || !Number.isFinite(speedKmh)) {
      log("WS  IN  ", `#${ws.id}`, ws.peer, "invalid_payload:", payload);
      return ws.send(JSON.stringify({ error: "invalid_payload" }));
    }

    const now = Date.now();
    if (now - ws.lastHandledAt < MIN_HANDLE_INTERVAL) return; // simple rate-limit
    ws.lastHandledAt = now;

    const marginKmh = Number.isFinite(margin) ? margin : DEFAULT_MARGIN;
    log("WS  IN  ", `#${ws.id}`, ws.peer, { pos: shortCoord(lat, lng), speedKmh: round(speedKmh), margin: marginKmh });

    try {
      const r = await queryOSMSpeeds(lat, lng);
      if (!r) {
        log("WS  OUT ", `#${ws.id}`, ws.peer, "no_highway_or_no_data");
        return ws.send(JSON.stringify({ error: "no_highway_or_no_data" }));
      }

      const limitMax = r.max.value;
      const limitMin = r.min.value;
      const overMax  = limitMax != null ? (speedKmh > limitMax + marginKmh) : false;
      const underMin = limitMin != null ? (speedKmh < Math.max(0, limitMin - marginKmh)) : false;

      const note =
        (r.min.value == null ? "Không có minspeed. " : "")

      const out = {
        type: "compare_result",
        limitKmh: limitMax,
        minKmh: limitMin,
        overMax,
        underMin,
        highway: r.highway,
        unit: "km/h",
        note
      };

      // ==== Lưu vào MongoDB ====
      if (telemetryCol) {
        const doc = {
          timestamp: new Date(),
          lat,
          lng,
          speedKmh,
          pressureBar: typeof pressureBar === "number" ? pressureBar : null,
          limitKmh: limitMax,
          minKmh: limitMin,
          overMax,
          underMin,
          highway: r.highway || null,
          note,
          licensePlate
        };
        try {
          await telemetryCol.insertOne(doc);
        } catch (err) {
          console.error("[Mongo] insert error:", err);
        }

        // ==== Broadcast cho tất cả dashboard đang mở ====
        const broadcast = {
          type: "telemetry_update",
          ...doc,
          timestamp: doc.timestamp.toISOString(),
        };

        wss.clients.forEach((client) => {
          if (
            client !== ws &&
            client.readyState === 1 && // WebSocket.OPEN
            client.clientType === "dashboard"
          ) {
            client.send(JSON.stringify(broadcast));
          }
        });
      }

      log("WS  OUT ", `#${ws.id}`, ws.peer, {
        limitKmh: out.limitKmh,
        minKmh: out.minKmh,
        overMax,
        underMin,
        highway: out.highway,
        fbMax: r.max.fallbackUsed,
        fbMin: r.min.fallbackUsed
      });

      // Gửi lại cho ESP32
      ws.send(JSON.stringify(out));
    } catch (e) {
      console.error(e);
      ws.send(JSON.stringify({ error: "server_error" }));
    }
  });

  ws.on("close", (code, reason) => {
    log("WS CLOSE", `#${ws.id}`, ws.peer, "code:", code, "reason:", reason?.toString?.() || "");
  });
});

// Heartbeat: dọn kết nối chết
const interval = setInterval(() => {
  wss.clients.forEach((ws) => {
    if (ws.isAlive === false) return ws.terminate();
    ws.isAlive = false; ws.ping();
  });
}, 30000);

server.on("upgrade", (req, socket, head) => {
  if (req.url === "/ws") {
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit("connection", ws, req));
  } else {
    socket.destroy();
  }
});

server.listen(PORT, () => {
  log(`HTTP: http://localhost:${PORT}   WS: ws://localhost:${PORT}/ws`);
  initMongo();   // Khởi động MongoDB sau khi server listen
});
