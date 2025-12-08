# TT-DTOT — Hệ Thống Giám Sát Tốc Độ Thông Minh

**Phiên bản cập nhật:** 2025-12-08

Một dự án hoàn chỉnh bao gồm phần cứng IoT (ESP32), backend Node.js, và dashboard web để giám sát tốc độ, áp suất lốp, vị trí GPS của phương tiện. Hệ thống tự động so sánh tốc độ thực tế với giới hạn tốc độ từ OpenStreetMap (Overpass API) và cảnh báo khi vượt tốc độ hoặc áp suất bất thường.

---

## 📋 Tổng Quan Dự Án

Hệ thống TT-DTOT cung cấp một giải pháp toàn diện để:

- **Thu thập dữ liệu**: Tốc độ (từ encoder), áp suất lốp (BMP180), vị trí GPS (NeoM8N), từ ESP32
- **Truyền tải thời gian thực**: Gửi dữ liệu qua WebSocket tới server
- **Phân tích**: So sánh với giới hạn tốc độ từ OSM, phát hiện vi phạm
- **Lưu trữ**: Lưu dữ liệu vào MongoDB
- **Hiển thị**: Dashboard web real-time, OLED trên ESP32, LED cảnh báo

### Thành Phần Dự Án

```
TT-DTOT/
├── main.ino              # Firmware ESP32 (ví dụ cơ bản)
├── main2.ino             # Firmware ESP32 (ví dụ thay thế)
├── main3.ino             # Firmware ESP32 (phiên bản đầy đủ - ĐƯỢC KHUYÊN DÙNG)
├── server.js             # Backend Node.js (REST API + WebSocket)
├── package.json          # Dependencies (Express, MongoDB, WS)
├── public/
│   └── main.html         # Dashboard web
├── arduino.txt           # Ghi chú về Arduino/ESP32
└── README.md             # Tài liệu này
```

---

## 🔧 Yêu Cầu Hệ Thống

### Phần Cứng (Hardware)

1. **Microcontroller**: ESP32 (hoặc tương thích)
   - WiFi tích hợp
   - GPIO, I2C, UART
   - Đủ bộ nhớ cho ArduinoJson, WebSocketsClient

2. **Cảm Biến**:
   - **GPS**: NeoM8N (UART2, TX=17, RX=16, 9600 baud)
   - **Áp suất lốp**: BMP180 (I2C, địa chỉ 0x77)
   - **Encoder tốc độ**: Rotary encoder (GPIO34, 20 xung/vòng, chu vi bánh 0.21m)
   - **Hiển thị**: OLED SSD1306 128×64 (I2C)

3. **Điều khiển**:
   - **LED cảnh báo**: GPIO19 (LED đỏ khi quá tốc độ/áp suất cao)
   - **Đèn nền OLED**: Được điều khiển động

### Phần Mềm (Backend)

- **Node.js 14+** (hỗ trợ ES modules)
- **npm** để cài đặt dependencies
- **MongoDB 4.0+** (tuỳ chọn, mặc định localhost:27017)
- **Kết nối Internet** để truy vấn Overpass API

---

## 📦 Cài Đặt & Cấu Hình

### 1. Chuẩn Bị Phần Mềm Backend

**Bước 1**: Tải và cài đặt Node.js từ https://nodejs.org/

**Bước 2**: Mở PowerShell, chuyển tới thư mục dự án:

```powershell
cd c:\Users\ADMIN\Desktop\TT-DTOT
```

**Bước 3**: Cài đặt dependencies (nếu chưa có):

```powershell
npm install
```

Các package được cài đặt:
- `express` – Server HTTP
- `ws` – WebSocket
- `node-fetch` – HTTP requests (để truy vấn Overpass API)
- `mongodb` – Driver MongoDB

**Bước 4**: Khởi động server:

```powershell
# Cách 1: Mặc định (PORT=3000, OVERPASS_URL=https://overpass-api.de/api/interpreter)
node server.js

# Cách 2: Cài đặt biến môi trường tuỳ chỉnh
$env:PORT = 3000; $env:OVERPASS_URL = 'https://overpass-api.de/api/interpreter'; node server.js
```

Khi khởi động thành công, bạn sẽ thấy:
```
[✓] MongoDB connected (telemetry collection)
[✓] Server started: HTTP: http://localhost:3000   WS: ws://localhost:3000/ws
```

### 2. Chuẩn Bị Phần Cứng & Firmware ESP32

**Bước 1**: Cài đặt Arduino IDE từ https://www.arduino.cc/en/software

**Bước 2**: Cài đặt Board ESP32:
- Mở Arduino IDE → File → Preferences
- Thêm URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Tools → Board Manager → Tìm "esp32" → Cài đặt

**Bước 3**: Cài đặt thư viện (Library):
- Sketch → Include Library → Manage Libraries
- Tìm và cài đặt:
  - `ArduinoJson` (bản 6.x)
  - `WebSocketsClient` (bản 2.x)
  - `Adafruit SSD1306` (bản 2.x)
  - `Adafruit BMP085` (bản 1.x)
  - `Adafruit Sensor` (bản 1.x)
  - `TinyGPSPlus` (bản 1.x)

**Bước 4**: Mở `main3.ino` trong Arduino IDE

**Bước 5**: Cấu hình phần cứng trong code (chỉnh sửa nếu cần):

```cpp
// Cấu hình WiFi
const char* WIFI_SSID = "P4";
const char* WIFI_PASS = "p45024ltm";

// Địa chỉ IP server (thay bằng IP của PC chạy Node.js)
const char* WS_HOST = "192.168.1.15";
const uint16_t WS_PORT = 3000;

// Biển số xe
const char* LICENSE_PLATE = "72F-345.67";

// Cấu hình cảm biến
#define WHEEL_CIRCUMFERENCE_M 0.21f  // Chu vi bánh (mét)
#define PULSES_PER_REV 20             // Xung trên encoder
```

**Bước 6**: Nạp firmware:
- Chọn Board: Tools → Board → ESP32 → ESP32 Dev Module
- Chọn COM port
- Nhấn Upload (Ctrl+U)

---

## 🔌 Firmware ESP32 (main3.ino)

### Các Chức Năng Chính

#### 1. **Đọc GPS (NeoM8N)**
- UART2 (TX=17, RX=16, 9600 baud)
- Sử dụng thư viện `TinyGPSPlus`
- Trạng thái:
  - `gpsFix = true`: Đã có fix vị trí
  - `gpsFix = false`: Chưa có fix, dùng vị trí lần cuối

#### 2. **Đo Tốc Độ từ Encoder**
- GPIO34 (input, ISR - ngắt)
- Công thức: `Speed = (WHEEL_CIRCUMFERENCE × Pulses) / (PULSES_PER_REV × Sample Time)`
- Chu vi bánh mặc định: **0.21m** (thay đổi theo bánh xe thực tế)
- Lấy mẫu mỗi **500ms**

#### 3. **Đo Áp Suất Lốp (BMP180)**
- I2C (địa chỉ 0x77)
- Chuyển đổi: `Áp suất (bar) = Áp suất (Pa) / 100000`
- Cập nhật mỗi **1 giây**

#### 4. **Hiển Thị OLED SSD1306**
- Kích thước: 128×64 pixels
- Thông tin hiển thị:
  - Tốc độ hiện tại (km/h)
  - Giới hạn tốc độ từ server (km/h)
  - Áp suất lốp (bar)
  - Trạng thái GPS (FIX/NO FIX)
  - Cảnh báo (OVER/UNDER/PRESS HIGH/LOW)

#### 5. **LED Cảnh báo (GPIO19)**
- **Sáng đỏ**: Khi có bất kỳ cảnh báo nào (quá tốc độ, áp suất cao, v.v.)
- **Nhấp nháy**: Phát hiện 5 lần/giây khi có lỗi

#### 6. **Kết Nối WebSocket**
- Gửi dữ liệu mỗi **2 giây**
- Payload JSON:
  ```json
  {
    "lat": 21.0278,
    "lng": 105.8342,
    "gpsFix": true,
    "fixAgeMs": 1234,
    "speedKmh": 45.5,
    "pressureBar": 2.1,
    "margin": 5,
    "licensePlate": "72F-345.67"
  }
  ```

#### 7. **Nhập Toạ Độ từ Serial**
- Mở Serial Monitor (baud 115200)
- Gõ: `lat <giá trị>` hoặc `lng <giá trị>` để nhập toạ độ thủ công
- Ví dụ: `lat 21.0278` → Đặt latitude = 21.0278

---

## 🌐 API REST Endpoints

### 1. **GET `/`**
- **Mô tả**: Trả về trang HTML chính (Dashboard)
- **Response**: HTML (main.html)
- **Ví dụ**: `http://localhost:3000/`

### 2. **GET `/limit`**
- **Mô tả**: Lấy giới hạn tốc độ tại toạ độ cho trước (từ Overpass API)
- **Query Parameters**:
  - `lat` (float): Vĩ độ
  - `lng` (float): Kinh độ
  - `margin` (int, tuỳ chọn): Sai số tốc độ (km/h), mặc định = 5
- **Response** (JSON):
  ```json
  {
    "limitKmh": 50,
    "minKmh": 10,
    "note": "Tìm thấy maxspeed=50 km/h",
    "lat": 21.0278,
    "lng": 105.8342
  }
  ```
- **Lỗi**:
  ```json
  {
    "limitKmh": -1,
    "minKmh": 10,
    "note": "Không tìm thấy maxspeed từ OSM, dùng mặc định 50 km/h",
    "lat": 21.0278,
    "lng": 105.8342
  }
  ```
- **Ví dụ**: `http://localhost:3000/limit?lat=21.0278&lng=105.8342&margin=5`

### 3. **POST `/compare`**
- **Mô tả**: So sánh tốc độ gửi lên với giới hạn tốc độ
- **Body** (JSON):
  ```json
  {
    "lat": 21.0278,
    "lng": 105.8342,
    "speedKmh": 60,
    "margin": 5,
    "licensePlate": "72F-345.67"
  }
  ```
- **Response** (JSON):
  ```json
  {
    "type": "compare_result",
    "limitKmh": 50,
    "minKmh": 10,
    "speedKmh": 60,
    "overMax": true,
    "underMin": false,
    "excessKmh": 10,
    "note": "Vượt tốc độ 10 km/h"
  }
  ```
- **Ví dụ cURL**:
  ```bash
  curl -X POST http://localhost:3000/compare \
    -H "Content-Type: application/json" \
    -d '{"lat":21.0278,"lng":105.8342,"speedKmh":60,"margin":5,"licensePlate":"72F-345.67"}'
  ```

### 4. **GET `/history`**
- **Mô tả**: Lấy lịch sử dữ liệu từ MongoDB
- **Query Parameters**:
  - `licensePlate` (string): Biển số xe (tuỳ chọn)
  - `limit` (int): Số bản ghi tối đa, mặc định = 1000
  - `skip` (int): Bỏ qua số bản ghi, mặc định = 0
- **Response** (JSON):
  ```json
  {
    "count": 150,
    "data": [
      {
        "_id": "507f1f77bcf86cd799439011",
        "timestamp": "2025-12-08T10:30:45.123Z",
        "licensePlate": "72F-345.67",
        "lat": 21.0278,
        "lng": 105.8342,
        "speedKmh": 45.5,
        "pressureBar": 2.1,
        "limitKmh": 50,
        "overMax": false,
        "underMin": false
      }
    ]
  }
  ```
- **Ví dụ**: `http://localhost:3000/history?licensePlate=72F-345.67&limit=100`

### 5. **GET `/plates`**
- **Mô tả**: Lấy danh sách các biển số xe đã ghi nhận
- **Response** (JSON):
  ```json
  {
    "plates": ["72F-345.67", "29A-98765", "30B-11111"]
  }
  ```

### 6. **GET `/alert-stats`**
- **Mô tả**: Thống kê các cảnh báo (vượt tốc độ, áp suất cao/thấp)
- **Query Parameters**:
  - `licensePlate` (string, tuỳ chọn)
  - `days` (int, tuỳ chọn): Tính từ n ngày gần nhất
- **Response** (JSON):
  ```json
  {
    "totalAlerts": 42,
    "overSpeedAlerts": 25,
    "pressureHighAlerts": 10,
    "pressureLowAlerts": 7,
    "byLicensePlate": {
      "72F-345.67": {
        "total": 25,
        "overSpeed": 15,
        "pressureHigh": 8,
        "pressureLow": 2
      }
    }
  }
  ```

---

## 📡 WebSocket Documentation

### Kết Nối WebSocket

**URL**: `ws://localhost:3000/ws`

### Thông Điệp từ Client (ESP32) → Server

**Loại 1**: Gửi dữ liệu cảm biến (Telemetry)
```json
{
  "lat": 21.0278,
  "lng": 105.8342,
  "gpsFix": true,
  "fixAgeMs": 1234,
  "speedKmh": 45.5,
  "pressureBar": 2.1,
  "margin": 5,
  "licensePlate": "72F-345.67"
}
```
- **Server trả về**:
  ```json
  {
    "type": "compare_result",
    "limitKmh": 50,
    "minKmh": 10,
    "speedKmh": 45.5,
    "overMax": false,
    "underMin": false,
    "note": "Tốc độ bình thường"
  }
  ```

### Thông Điệp từ Web Client → Server

**Loại 1**: Đăng ký nhận cập nhật Dashboard
```json
{
  "type": "dashboard_subscribe"
}
```
- **Server sẽ gửi** (mỗi khi có telemetry từ ESP32):
  ```json
  {
    "type": "telemetry_update",
    "licensePlate": "72F-345.67",
    "lat": 21.0278,
    "lng": 105.8342,
    "speedKmh": 45.5,
    "pressureBar": 2.1,
    "limitKmh": 50,
    "overMax": false,
    "underMin": false,
    "timestamp": "2025-12-08T10:30:45.123Z"
  }
  ```

### Thông Điệp Lỗi

Nếu có lỗi, server gửi:
```json
{
  "type": "error",
  "message": "Lỗi kết nối đến Overpass API",
  "code": "OVERPASS_TIMEOUT"
}
```

### Heartbeat (Ping/Pong)
- Server gửi PING mỗi **30 giây**
- Client phải trả PONG để giữ kết nối

---

## 📊 Dashboard Features

File `public/main.html` cung cấp giao diện web để:

### 1. **Hiển Thị Thời Gian Thực**
- Tốc độ hiện tại (km/h)
- Giới hạn tốc độ (km/h)
- Áp suất lốp (bar)
- Vị trí GPS (lat/lng)
- Trạng thái kết nối

### 2. **Cảnh Báo Realtime**
- **🔴 Quá Tốc Độ** (overMax): Tốc độ > Giới hạn + margin
- **🟡 Dưới Tốc Độ Tối Thiểu** (underMin): Tốc độ < Tốc độ tối thiểu
- **⚠️ Áp Suất Cao**: > 2.0 bar
- **⚠️ Áp Suất Thấp**: < 1.0 bar

### 3. **Lịch Sử & Thống Kê**
- Hiển thị dữ liệu lịch sử từ MongoDB
- Biểu đồ thống kê cảnh báo
- Lọc theo biển số xe

### 4. **Bản Đồ**
- Hiển thị vị trí hiện tại
- Tracking đường đi (nếu có)

---

## 🚨 Ngưỡng Cảnh Báo

### Áp Suất Lốp (Tire Pressure)

| Tình Huống | Giá Trị | Hành Động |
|-----------|--------|---------|
| **Áp suất cao** | > 2.0 bar | LED sáng đỏ, cảnh báo OLED |
| **Áp suất bình thường** | 1.0 - 2.0 bar | Bình thường |
| **Áp suất thấp** | < 1.0 bar | LED sáng đỏ, cảnh báo OLED |

Các ngưỡng được định nghĩa trong:
- **main3.ino**: `PRESSURE_LIMIT_MIN = 1.00f`, `PRESSURE_LIMIT_MAX = 2.00f`
- **server.js**: `PRESSURE_MIN_BAR = 1.0`, `PRESSURE_MAX_BAR = 2.0`

### Tốc Độ (Speed)

| Tình Huống | Hành Động |
|-----------|---------|
| **Vượt tốc độ** (Speed > Limit + Margin) | LED nhấp nháy, OLED hiển thị "OVER" |
| **Dưới tốc độ tối thiểu** (Speed < MinSpeed) | LED nhấp nháy, OLED hiển thị "UNDER" |
| **Tốc độ bình thường** | Bình thường |

- **Margin mặc định**: 5 km/h
- Giới hạn tối thiểu mặc định (khi OSM không có): 10 km/h

---

## 🐛 Debugging Tips

### 1. **Kiểm Tra Kết Nối WiFi ESP32**
Mở Serial Monitor (115200 baud) trong Arduino IDE:
```
[WiFi] Connecting to P4...
[WiFi] Connected. IP: 192.168.1.100
[WS] Connecting to ws://192.168.1.15:3000/ws
[WS] Connected
```

### 2. **Kiểm Tra Dữ Liệu GPS**
Nếu GPS không fix, Serial sẽ hiện:
```
[GPS] Waiting for fix... (age: 0ms)
```

Giải pháp:
- Kiểm tra kết nối TX/RX của GPS
- Đảm bảo cặp (RX=16, TX=17)
- Ngoài trời hoặc gần cửa sổ để GPS fix

### 3. **Kiểm Tra BMP180**
Nếu BMP180 không đọc được:
- Kiểm tra I2C địa chỉ: `0x77`
- Kiểm tra kết nối SDA/SCL
- Dùng I2C scanner để phát hiện thiết bị

### 4. **Kiểm Tra Encoder Tốc Độ**
Nếu tốc độ lúc nào cũng 0:
- Xác minh GPIO34 kết nối encoder đúng
- Kiểm tra `WHEEL_CIRCUMFERENCE_M` có chính xác không
- Serial Monitor hiện: `[ENC] Speed: 0.00 km/h`

### 5. **Kiểm Tra Server Node.js**
```powershell
# Khởi động với log chi tiết
node server.js
```

Nếu MongoDB kết nối thất bại:
```
[✗] MongoDB connection failed: ...
```

Giải pháp:
- Đảm bảo MongoDB chạy: `mongod`
- Hoặc cài đặt `MONGO_URL`: `$env:MONGO_URL = 'mongodb://localhost:27017'; node server.js`

### 6. **Kiểm Tra Overpass API**
Nếu không lấy được giới hạn tốc độ:
- Truy cập: https://overpass-api.de/api/interpreter
- Kiểm tra toạ độ có dữ liệu OSM không
- Có thể thử URL khác

### 7. **Xem Log WebSocket**
Mở Developer Tools trình duyệt (F12 → Network → WS), hoặc:
```javascript
// Thêm vào public/main.html
ws.addEventListener('message', (event) => {
  console.log('WS Message:', JSON.parse(event.data));
});
```

### 8. **Kiểm Tra OLED**
Nếu OLED không hiển thị gì:
- Kiểm tra I2C kết nối (SDA, SCL)
- Xác minh địa chỉ: `0x3C` (hoặc tuỳ chỉnh trong code)
- Cấp nguồn đủ

---

## 📝 Development Notes

### Cấu Trúc Dự Án

```
Server Architecture:
├── REST API (Express)
│   ├── GET /       → HTML Dashboard
│   ├── GET /limit  → Lấy giới hạn tốc độ
│   ├── POST /compare → So sánh tốc độ
│   ├── GET /history → Lấy lịch sử
│   ├── GET /plates → Danh sách xe
│   └── GET /alert-stats → Thống kê cảnh báo
├── WebSocket Server
│   ├── ESP32 connects → /ws
│   └── Web clients → /ws
└── Database (MongoDB)
    └── Collection: telemetry (dữ liệu cảm biến)
```

### Caching & Performance

- **Giới hạn tốc độ** được cache 1 giờ (CACHE_TTL_MS = 3600000ms)
- **Overpass API** giới hạn ~1 request/giây, tránh request quá nhiều

### Error Handling

Server xử lý các lỗi:
- Overpass API timeout (5000ms)
- MongoDB disconnect → Reconnect tự động
- WebSocket disconnect → Client tự kết nối lại

### Bảo Mật

- Hiện tại không có authentication
- **Để production**: Thêm token, HTTPS, WSS
- Validate dữ liệu input (lat/lng, speed, pressure)

---

## 💡 Gợi Ý Mở Rộng & Tích Hợp

### 1. **Thêm Xác Thực (Authentication)**
- Token JWT cho API & WebSocket
- Login web dashboard
- User/vehicle management

### 2. **Cảnh Báo Proactive**
- Gửi SMS/Email khi vượt tốc độ
- Push notification qua mobile app
- Integration Firebase Cloud Messaging

### 3. **Phân Tích Dữ Liệu Nâng Cao**
- Machine learning để dự đoán vi phạm
- Heatmap khu vực quá tốc độ
- Report chi tiết tài xế

### 4. **Tích Hợp Bên Ngoài**
- Google Maps API (thay vì bản đồ đơn giản)
- Twilio (SMS cảnh báo)
- AWS/Azure Cloud (lưu trữ lịch sử dài hạn)

### 5. **Mobile App**
- React Native / Flutter
- Push notifications realtime
- Offline support

### 6. **Thêm Cảm Biến**
- Gia tốc (Accelerometer)
- Nhiệt độ động cơ (DHT22)
- Mức xăng (Fuel level)
- Camera DVR (RTSP stream)

### 7. **Dashboard Nâng Cao**
- Chart.js / Plotly cho biểu đồ
- Leaflet/Mapbox cho bản đồ
- Websocket compression (permessage-deflate)

### 8. **Deployment Production**
- Docker container
- PM2 process manager
- NGINX reverse proxy
- SSL/TLS certificate (Let's Encrypt)
- MongoDB Atlas (cloud)

### 9. **Reporting & Analytics**
- PDF report generator
- Daily/Weekly/Monthly summaries
- Violation statistics by time/location
- Driver performance score

### 10. **Integration with Fleet Management**
- Sync với hệ thống quản lý đội xe
- Telematics hub
- Integration OBD-II (CANbus)

---

## 📚 Tài Liệu Tham Khảo

- **ESP32 Documentation**: https://docs.espressif.com/
- **Node.js**: https://nodejs.org/docs/
- **OpenStreetMap Overpass API**: https://wiki.openstreetmap.org/wiki/Overpass_API
- **MongoDB**: https://docs.mongodb.com/
- **Express.js**: https://expressjs.com/
- **WebSocket (RFC 6455)**: https://tools.ietf.org/html/rfc6455

---

## 📄 License & Credit

**Phiên bản**: 2025-12-08  
**Tác giả**: TT-DTOT Team  
**License**: ISC (hoặc tuỳ chỉnh theo nhu cầu)

---

## 🤝 Hỗ Trợ & Liên Hệ

Nếu gặp vấn đề:
1. Kiểm tra **Debugging Tips** ở trên
2. Xem log console (Arduino Serial Monitor / Node.js terminal)
3. Kiểm tra kết nối phần cứng
4. Đảm bảo tất cả dependencies được cài đặt

---

**Cập nhật lần cuối**: 2025-12-08  
**Dự án**: TT-DTOT — Hệ Thống Giám Sát Tốc Độ Thông Minh
