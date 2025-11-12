# TT-DTOT — Speed limit helper

Phiên bản cập nhật: 2025-11-12

Một dự án minh họa bao gồm mã Arduino và một server Node.js để truy xuất giới hạn tốc độ xung quanh một toạ độ (dùng OSM Overpass) và so sánh với tốc độ thiết bị gửi lên.

## Tổng quan

Thư mục chính chứa:

- `main.ino` — mã nguồn Arduino (ví dụ thiết bị gửi vị trí/tốc độ).
- `main2.ino` — mã Arduino bổ sung (ví dụ khác).
- `server.js` — server Node.js cung cấp REST API và WebSocket.
- `public/` — giao diện HTML mẫu (`main.html`).
- `package.json` — manifest (project dùng ES modules — "type": "module").

Server cung cấp:

- REST GET `/limit?lat=<lat>&lng=<lng>` — lấy thông tin giới hạn tốc độ gần toạ độ.
- REST POST `/compare` — so sánh tốc độ gửi lên với giới hạn (payload JSON: `{ lat, lng, speedKmh, margin }`).
- WebSocket endpoint `/ws` — nhận JSON tương tự và trả về kết quả so sánh trong thời gian thực.

Mục tiêu: dễ thử nghiệm (local), demo tích hợp ESP32/Arduino gửi vị trí+tốc độ, và minh hoạ fallback khi OSM không có thẻ maxspeed.

## Yêu cầu

- Node.js 14+ (do project dùng ES modules). Tải tại https://nodejs.org/
- Mạng internet để truy vấn Overpass API (mặc định: `https://overpass-api.de/api/interpreter`).

Tuỳ chọn: cài các phụ thuộc nếu `package.json` có ghi. Hiện repo dùng module chuẩn; nếu có lỗi, chạy `npm install` trước khi khởi động.

## Cấu hình (biến môi trường)

- `OVERPASS_URL` — URL của Overpass API (mặc định: `https://overpass-api.de/api/interpreter`).
- `PORT` — cổng HTTP/WS (mặc định: `3000`).

Ví dụ (PowerShell):

```powershell
$env:PORT = 3000; $env:OVERPASS_URL = 'https://overpass-api.de/api/interpreter'
node server.js
```

## Chạy server (nhanh)

1. Mở PowerShell trong thư mục dự án `c:\Users\ADMIN\Desktop\TT-DTOT`.
2. (Tuỳ chọn) Cài phụ thuộc:

```powershell
npm install
```

3. Chạy server:

```powershell
node server.js
```

Sau khi chạy, bạn sẽ thấy log: `HTTP: http://localhost:3000   WS: ws://localhost:3000/ws`.

## REST API — ví dụ

1) Lấy giới hạn tốc độ gần toạ độ

```bash
curl "http://localhost:3000/limit?lat=51.5074&lng=-0.1278"
```

PowerShell:

```powershell
Invoke-RestMethod -Uri "http://localhost:3000/limit?lat=51.5074&lng=-0.1278"
```

Trả về JSON ví dụ:

```json
{
   "source":"osm-overpass",
   "wayId":123456,
   "highway":"primary",
   "maxspeed":50,
   "unit":"km/h"
}
```

2) So sánh tốc độ (POST `/compare`)

```bash
curl -H "Content-Type: application/json" -d '{"lat":51.5074,"lng":-0.1278,"speedKmh":70}' http://localhost:3000/compare
```

PowerShell:

```powershell
$body = @{ lat = 51.5074; lng = -0.1278; speedKmh = 70 } | ConvertTo-Json
Invoke-RestMethod -Uri 'http://localhost:3000/compare' -Method POST -Body $body -ContentType 'application/json'
```

Trả về JSON ví dụ:

```json
{
   "limitKmh":50,
   "minKmh":null,
   "overMax":true,
   "underMin":false,
   "highway":"primary",
   "unit":"km/h"
}
```

## WebSocket — ví dụ ngắn

URL WebSocket: `ws://localhost:3000/ws`.

Gửi JSON (string) giống payload `/compare`:

```json
{ "lat":51.5074, "lng":-0.1278, "speedKmh":70 }
```

Node.js client (dùng package `ws`):

```javascript
import WebSocket from 'ws';

const ws = new WebSocket('ws://localhost:3000/ws');

ws.on('open', () => {
   ws.send(JSON.stringify({ lat:51.5074, lng:-0.1278, speedKmh:70 }));
});

ws.on('message', (m) => console.log('recv', m.toString()));
```

Kết quả trả về sẽ chứa `type: "compare_result"` và các trường `limitKmh`, `overMax`, `underMin`, v.v.

## Cách server xác định giới hạn

- Server truy vấn OSM Overpass tìm các `way` có thẻ `highway` quanh toạ độ trong nhiều bán kính (50, 150, 300 m).
- Ưu tiên các way có thẻ `maxspeed`/`minspeed`. Nếu không có, server dùng fallback theo loại `highway` (ví dụ: motorway=100, residential=40).
- Kết quả được cache trong 1 giờ để giảm số lần gọi Overpass.

## Lưu ý phát triển

- File `server.js` đã có logging cơ bản và heartbeat cho WebSocket.
- Project dùng ES modules (`package.json` có `type: "module"`). Hãy chạy với Node 14+.
- Nếu bạn muốn thay đổi logic fallback hoặc TTL cache, sửa `FALLBACK_MAX_BY_HIGHWAY` và `CACHE_TTL_MS` trong `server.js`.

## Gợi ý mở rộng

- Thêm tests unit cho `queryOSMSpeeds` (mock fetch/Overpass).
- Thêm endpoint cho bulk-compare (nhiều vị trí cùng lúc).
- Thêm CI (GitHub Actions) để chạy lint/tests.

## License

MIT — tuỳ chỉnh nếu bạn muốn một license khác.

---

Changelog:

- 2025-11-12: README được tái thiết kế để rõ ràng hơn (mô tả, cài đặt, API, WebSocket, ví dụ). 
