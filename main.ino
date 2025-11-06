//demo test giả lập để cảnh báo
#/*******************************************************************************
 * main.ino
 *
 * Mục đích:
 *   - Mô phỏng/thu thập dữ liệu tốc độ + áp suất từ input Serial hoặc cảm biến.
 *   - Hiển thị trạng thái trên OLED.
 *   - Gửi dữ liệu vị trí/tốc độ tới một WebSocket server để so sánh giới hạn tốc độ (server chạy ở PC).
 *
 * Phần cứng:
 *   - ESP32 (thư viện WiFi.h)
 *   - OLED SSD1306 (I2C)
 *
 * Lệnh Serial (ví dụ để nhập thủ công khi phát triển):
 *   - pos <lat>,<lng> <speed> <pressure>
 *       (ví dụ: pos 21.0546,105.8684 72.5 1.23)  -> cập nhật toạ độ + speed + pressure, gửi ngay 1 gói WS
 *   - pos <lat>,<lng>,<speed>,<pressure>
 *   - gps <lat>,<lng>                           -> cập nhật toạ độ
 *   - <speed> <pressure>                        -> ví dụ: "72.5 1.23"
 *   - limit pmin=<bar> | limit pmax=<bar>      -> điều chỉnh ngưỡng áp suất
 *
 * Cấu hình nhanh trong file:
 *   - WIFI_SSID / WIFI_PASS: cấu hình WiFi của bạn
 *   - WS_HOST / WS_PORT / WS_PATH: thông tin server WebSocket (ví dụ ws://192.168.1.8:3000/ws)
 *
 * Ghi chú:
 *   - File đã có các hàm phụ trợ để parse nhiều định dạng đầu vào. Các hàm hiển thị và cảnh báo
 *     đã được tách ra thành các hàm nhỏ (showNormal, showAlert, updateLED).
 *
 * Ngày: 2025-11-05
 ******************************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* WIFI_SSID = "P4";
const char* WIFI_PASS = "p45024ltm";

// ws://<PC-IP>:3000/ws
const char* WS_HOST = "192.168.1.8";
const uint16_t WS_PORT = 3000;
const char* WS_PATH = "/ws";

// OLED / LED
#define LED_PIN 19
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Trạng thái
WebSocketsClient ws;
bool wsConnected = false;

float speed_kmh    = 60.0f;   // nhập Serial
float pressure_bar = 1.50f;   // nhập Serial hoặc từ cảm biến

// Ngưỡng áp suất (cục bộ)
float PRESSURE_LIMIT_MIN = 1.00f;  // bar
float PRESSURE_LIMIT_MAX = 2.00f;  // bar

// Toạ độ (Serial)
bool  hasCoord = false;
float gps_lat = 0.0f;
float gps_lng = 0.0f;

// Phản hồi server (tốc độ)
bool  srv_overMax = false;
bool  srv_underMin = false;
int   srv_limit_kmh = -1;
int   srv_min_kmh   = -1;
String srv_note;

const uint32_t SEND_INTERVAL_MS = 400;
uint32_t lastSendMs = 0;

const uint32_t BLINK_INTERVAL_MS = 350;
uint32_t lastBlinkMs = 0;
bool blinkOn = false;

String lineBuffer;

// Helpers
void drawCenteredText(const String &text, int16_t y, uint8_t textSize = 1) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - (int16_t)w) / 2;
  display.setCursor(x, y);
  display.print(text);
}
void drawCenteredLines(const String lines[], uint8_t count, int16_t startY, uint8_t textSize = 1, uint8_t spacing = 10) {
  for (uint8_t i = 0; i < count; i++) drawCenteredText(lines[i], startY + i * spacing, textSize);
}
void updateBlink() {
  uint32_t now = millis();
  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) { lastBlinkMs = now; blinkOn = !blinkOn; }
}

// Hiển thị
void showNormal() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText(F("GIAM SAT (WS)"), 0, 1);

  // tốc độ
  display.setTextSize(1);
  display.setCursor(4, 20); display.print(F("Toc do: "));
  display.setTextSize(2);   display.print(speed_kmh, 0);
  display.setTextSize(1);   display.setCursor(100, 28); display.print(F("km/h"));

  // áp suất
  display.setTextSize(1);
  display.setCursor(4, 42); display.print(F("Ap suat: "));
  display.setTextSize(2);   display.print(pressure_bar, 1);
  display.setTextSize(1);   display.setCursor(100, 50); display.print(F("bar"));

  display.display();
}

void showAlert() {
  // Xác định nguồn cảnh báo
  bool pressHigh = (pressure_bar > PRESSURE_LIMIT_MAX);
  bool pressLow  = (pressure_bar < PRESSURE_LIMIT_MIN);
  bool spdAlert  = (srv_overMax || srv_underMin);
  bool prsAlert  = (pressHigh || pressLow);

  display.clearDisplay();

  // Tiêu đề nhấp nháy
  if (blinkOn) {
    display.setTextColor(SSD1306_WHITE);
    drawCenteredText(F("CANH BAO!"), 6, 2);
  }

  String lines[6]; 
  uint8_t n = 0;

  // 1) Chỉ áp suất -> chỉ hiện áp suất
  if (prsAlert && !spdAlert) {
    if (pressHigh) lines[n++] = "Ap suat cao!";
    if (pressLow)  lines[n++] = "Ap suat thap!";
    lines[n++] = String(PRESSURE_LIMIT_MIN,2) + "-" + String(PRESSURE_LIMIT_MAX,2) + " bar";
  }
  // 2) Chỉ tốc độ -> chỉ hiện tốc độ
  else if (spdAlert && !prsAlert) {
    if (srv_overMax)  lines[n++] = "Qua toc do!";
    if (srv_underMin) lines[n++] = "Toc do thap!";
    if (srv_limit_kmh >= 0) lines[n++] = String("Max: ") + srv_limit_kmh + " km/h";
  }
  // 3) Cả hai -> hiện cả hai nhóm
  else if (spdAlert && prsAlert) {
    if (srv_overMax)  lines[n++] = "Qua toc do!";
    if (srv_underMin) lines[n++] = "Toc do thap!";
    if (srv_limit_kmh >= 0) lines[n++] = String("Max: ") + srv_limit_kmh + " km/h";
    if (pressHigh) lines[n++] = "Ap suat cao!";
    if (pressLow)  lines[n++] = "Ap suat thap!";
    lines[n++] = String(PRESSURE_LIMIT_MIN,2) + "-" + String(PRESSURE_LIMIT_MAX,2) + " bar";
  }
  else {
    lines[n++] = "Dang xu ly...";
  }

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  drawCenteredLines(lines, n, 26, 1, 10);
  display.display();
}

// LED
void updateLED() {
  bool pressHigh = (pressure_bar > PRESSURE_LIMIT_MAX);
  bool pressLow  = (pressure_bar < PRESSURE_LIMIT_MIN);
  bool alert = srv_overMax || srv_underMin || pressHigh || pressLow;
  digitalWrite(LED_PIN, (alert && blinkOn) ? HIGH : LOW);
}

// WebSocket
void handleWSMessage(const String& msg) {
  StaticJsonDocument<512> d;
  DeserializationError err = deserializeJson(d, msg);
  if (err) { Serial.print(F("[WS] JSON err: ")); Serial.println(err.c_str()); return; }
  if (d.containsKey("error")) { Serial.print(F("[WS] Error: ")); Serial.println((const char*)d["error"]); return; }
  if (d["type"] && String((const char*)d["type"]) != "compare_result") return;

  srv_limit_kmh = d["limitKmh"].isNull() ? -1 : (int)d["limitKmh"].as<int>();
  srv_min_kmh   = d["minKmh"].isNull() ? -1 : (int)d["minKmh"].as<int>();
  srv_overMax   = d["overMax"]  | false;
  srv_underMin  = d["underMin"] | false;
  srv_note      = d["note"]     | "";

  Serial.printf("[WS<-] limit=%d min=%d over=%d under=%d note=%s\n",
    srv_limit_kmh, srv_min_kmh, srv_overMax, srv_underMin, srv_note.c_str());
}

void onWSEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:   wsConnected = true;  Serial.println(F("[WS] Connected")); break;
    case WStype_DISCONNECTED:wsConnected = false; Serial.println(F("[WS] Disconnected")); break;
    case WStype_TEXT: {
      String msg((char*)payload, length);
      handleWSMessage(msg);
      break;
    }
    case WStype_PING:  Serial.println(F("[WS] PING"));  break;
    case WStype_PONG:  Serial.println(F("[WS] PONG"));  break;
    default: break;
  }
}

void sendWS() {
  if (!wsConnected || !hasCoord) return;
  uint32_t now = millis();
  if (now - lastSendMs < SEND_INTERVAL_MS) return;
  lastSendMs = now;

  StaticJsonDocument<256> doc;
  doc["lat"] = gps_lat;
  doc["lng"] = gps_lng;
  doc["speedKmh"] = speed_kmh;
  doc["margin"] = 5;

  String out; serializeJson(doc, out);
  ws.sendTXT(out);
}

// Serial parsing
bool tryParseTwoFloats(const String& s, float& a, float& b) {
  float ta, tb;
  int matched = sscanf(s.c_str(), "%f %f", &ta, &tb);
  if (matched == 2) { a = ta; b = tb; return true; }
  matched = sscanf(s.c_str(), "v=%f p=%f", &ta, &tb);
  if (matched == 2) { a = ta; b = tb; return true; }
  matched = sscanf(s.c_str(), "%f,%f", &ta, &tb);
  if (matched == 2) { a = ta; b = tb; return true; }
  return false;
}

bool tryParseGps(const String& s) {
  float la, lo;
  if (sscanf(s.c_str(), "gps %f,%f", &la, &lo) == 2 ||
      sscanf(s.c_str(), "gps=%f,%f", &la, &lo) == 2 ||
      sscanf(s.c_str(), "gps %f %f", &la, &lo) == 2) {
    gps_lat = la; gps_lng = lo; hasCoord = true;
    Serial.printf("[GPS] lat=%.6f lng=%.6f\n", gps_lat, gps_lng);
    return true;
  }
  return false;
}

// Parse lệnh pos: pos <lat>,<lng> <speed> <pressure>  hoặc pos <lat>,<lng>,<speed>,<pressure>
bool tryParsePosAll(const String& s) {
  float la, lo, v, p;
  if (sscanf(s.c_str(), "pos %f,%f %f %f", &la, &lo, &v, &p) == 4) {
    gps_lat = la; gps_lng = lo; hasCoord = true; speed_kmh = v; pressure_bar = p;
    Serial.printf("[OK] POS+DATA lat=%.6f lng=%.6f v=%.2f p=%.3f\n", gps_lat, gps_lng, speed_kmh, pressure_bar);
    return true;
  }
  if (sscanf(s.c_str(), "pos %f,%f,%f,%f", &la, &lo, &v, &p) == 4) {
    gps_lat = la; gps_lng = lo; hasCoord = true; speed_kmh = v; pressure_bar = p;
    Serial.printf("[OK] POS+DATA lat=%.6f lng=%.6f v=%.2f p=%.3f\n", gps_lat, gps_lng, speed_kmh, pressure_bar);
    return true;
  }
  return false;
}

bool tryParsePressureLimits(const String& s) {
  float v;
  if (sscanf(s.c_str(), "limit pmin=%f", &v) == 1) {
    PRESSURE_LIMIT_MIN = v;
    Serial.printf("[OK] PRESSURE_LIMIT_MIN=%.2f bar\n", PRESSURE_LIMIT_MIN);
    return true;
  }
  if (sscanf(s.c_str(), "limit pmax=%f", &v) == 1) {
    PRESSURE_LIMIT_MAX = v;
    Serial.printf("[OK] PRESSURE_LIMIT_MAX=%.2f bar\n", PRESSURE_LIMIT_MAX);
    return true;
  }
  return false;
}

void sendOneNow() {
  if (!wsConnected || !hasCoord) return;
  StaticJsonDocument<256> doc;
  doc["lat"] = gps_lat;
  doc["lng"] = gps_lng;
  doc["speedKmh"] = speed_kmh;
  doc["margin"] = 5;
  String out; serializeJson(doc, out);
  ws.sendTXT(out);
}

void readSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String cmd = lineBuffer; lineBuffer = ""; cmd.trim();
      if (!cmd.length()) return;

      // NEW: pos ... => cập nhật cả vị trí + speed + pressure và gửi ngay WS
      if (cmd.startsWith("pos")) {
        if (!tryParsePosAll(cmd)) {
          Serial.println(F("[ERR] pos <lat>,<lng> <speed> <pressure> | pos <lat>,<lng>,<speed>,<pressure>"));
        } else {
          sendOneNow();  // gửi ngay 1 gói, không chờ chu kỳ
        }
        return;
      }

      if (cmd.startsWith("gps")) {
        if (!tryParseGps(cmd)) Serial.println(F("[ERR] gps <lat>,<lng> | gps=<lat>,<lng> | gps <lat> <lng>"));
        return;
      }
      if (cmd.startsWith("limit")) {
        if (!tryParsePressureLimits(cmd)) Serial.println(F("[ERR] limit pmin=<bar> | limit pmax=<bar>"));
        return;
      }

      float a, b;
      if (tryParseTwoFloats(cmd, a, b)) {
        speed_kmh = a; pressure_bar = b;
        Serial.printf("[OK] v=%.2f km/h, p=%.3f bar\n", speed_kmh, pressure_bar);
      } else {
        Serial.println(F("[ERR] '72.5 1.23' | 'v=72.5 p=1.23' | '72.5,1.23' ; GPS: 'gps 21.0,105.8' ; POS: 'pos lat,lng v p'"));
      }
    } else {
      if (lineBuffer.length() < 120) lineBuffer += c;
    }
  }
}

// Setup / loop
void setup() {
  Serial.begin(115200); delay(200);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println(F("[ERR] OLED not found!")); for(;;); }
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
  drawCenteredText(F("WS Ready"), 22, 2); display.display(); delay(600);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Connecting");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(400); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { Serial.print("[WiFi] "); Serial.println(WiFi.localIP()); }
  else { Serial.println("[WiFi] Offline."); }

  ws.onEvent(onWSEvent);
  ws.begin(WS_HOST, WS_PORT, WS_PATH);
  ws.setReconnectInterval(2000);
  ws.enableHeartbeat(15000, 3000, 2);

  Serial.println("===== INPUT =====");
  Serial.println("- pos 21.0546,105.8684 72.5 1.23   (cap nhat ca toa do + speed + pressure, gui ngay)");
  Serial.println("- gps 21.0546,105.8684              (cap nhat toa do)");
  Serial.println("- 72.5 1.23                         (speed km/h, pressure bar)");
  Serial.println("- limit pmin=1.0 | limit pmax=2.0   (nguong ap suat)");
  Serial.println("=================");
}

void loop() {
  readSerial();
  updateBlink();
  ws.loop();
  sendWS();

  updateLED();
  if (srv_overMax || srv_underMin || (pressure_bar > PRESSURE_LIMIT_MAX) || (pressure_bar < PRESSURE_LIMIT_MIN))
    showAlert();
  else
    showNormal();

  delay(10);
}
