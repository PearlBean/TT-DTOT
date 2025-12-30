# TT-DTOT — Hệ Thống Giám Sát Tốc Độ Thông Minh

[![License: ISC](https://img.shields.io/badge/License-ISC-blue.svg)](https://opensource.org/licenses/ISC)
[![Node.js Version](https://img.shields.io/badge/node-%3E%3D14.0.0-brightgreen)](https://nodejs.org/)
[![MongoDB](https://img.shields.io/badge/MongoDB-4.0+-green)](https://www.mongodb.com/)

**Phiên bản cập nhật:** 2025-12-30

Một dự án hoàn chỉnh bao gồm phần cứng IoT (ESP32), backend Node.js, và dashboard web để giám sát tốc độ, áp suất lốp, vị trí GPS của phương tiện. Hệ thống tự động so sánh tốc độ thực tế với giới hạn tốc độ từ OpenStreetMap (Overpass API) và cảnh báo khi vượt tốc độ hoặc áp suất bất thường.

## 📋 Mục Lục

- [Tổng Quan Dự Án](#-tổng-quan-dự-án)
- [Tính Năng Chính](#-tính-năng-chính)
- [Kiến Trúc Hệ Thống](#-kiến-trúc-hệ-thống)
- [Yêu Cầu Hệ Thống](#-yêu-cầu-hệ-thống)
- [Cài Đặt](#-cài-đặt)
- [Triển Khai với Docker](#-triển-khai-với-docker)
- [Sử Dụng](#-sử-dụng)
- [API Documentation](#-api-documentation)
- [WebSocket Documentation](#-websocket-documentation)
- [Dashboard](#-dashboard)
- [Debugging](#-debugging)
- [Đóng Góp](#-đóng-góp)
- [Giấy Phép](#-giấy-phép)
- [Liên Hệ](#-liên-hệ)

---

## 📋 Tổng Quan Dự Án

Hệ thống TT-DTOT cung cấp một giải pháp toàn diện để giám sát phương tiện thông minh, kết hợp IoT, backend server và giao diện web thời gian thực.

### Thành Phần Dự Án

```
TT-DTOT/
├── main.ino              # Firmware ESP32 (ví dụ cơ bản)
├── main2.ino             # Firmware ESP32 (ví dụ thay thế)
├── main3.ino             # Firmware ESP32 (phiên bản đầy đủ - ĐƯỢC KHUYÊN DÙNG)
├── server.js             # Backend Node.js (REST API + WebSocket)
├── package.json          # Dependencies (Express, MongoDB, WS)
├── Dockerfile            # Docker image cho ứng dụng
├── docker-compose.yml    # Triển khai với Docker
├── .dockerignore         # Loại trừ file cho Docker
├── public/
│   └── main.html         # Dashboard web
├── arduino.txt           # Ghi chú về Arduino/ESP32
└── README.md             # Tài liệu này
```

---

## 🚀 Tính Năng Chính

- **Thu thập dữ liệu thời gian thực**: Tốc độ, áp suất lốp, GPS từ ESP32
- **So sánh tốc độ thông minh**: Tích hợp OpenStreetMap để lấy giới hạn tốc độ
- **Cảnh báo tức thời**: Phát hiện vi phạm tốc độ và áp suất bất thường
- **Lưu trữ dữ liệu**: MongoDB để lưu trữ lịch sử telemetry
- **Dashboard web**: Giao diện thời gian thực với WebSocket
- **Hiển thị OLED**: Thông tin trực quan trên ESP32
- **Triển khai dễ dàng**: Hỗ trợ Docker containerization

---

## 🏗️ Kiến Trúc Hệ Thống

```
┌─────────────────┐    WebSocket    ┌─────────────────┐
│     ESP32       │◄──────────────► │   Node.js       │
│  (Sensors)      │                 │   Server        │
│                 │                 │                 │
│ • GPS NeoM8N    │   REST API      │ • Express       │
│ • BMP180        │◄──────────────► │ • WebSocket     │
│ • Rotary Encoder│                 │ • MongoDB       │
│ • OLED SSD1306  │                 │                 │
└─────────────────┘                 └─────────────────┘
         │                                  │
         │                                  │
         ▼                                  ▼
┌─────────────────┐                 ┌─────────────────┐
│   Web Browser   │◄──────────────► │   OpenStreetMap │
│   Dashboard     │                 │   Overpass API  │
└─────────────────┘                 └─────────────────┘
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

### Phần Mềm (Software)

- **Node.js 14+** (hỗ trợ ES modules)
- **npm** để cài đặt dependencies
- **MongoDB 4.0+** (tuỳ chọn, mặc định localhost:27017)
- **Docker** (tùy chọn cho deployment)
- **Kết nối Internet** để truy vấn Overpass API

---

## 📦 Cài Đặt

### 1. Chuẩn Bị Phần Mềm Backend

**Bước 1**: Tải và cài đặt Node.js từ [https://nodejs.org/](https://nodejs.org/)

**Bước 2**: Mở PowerShell, chuyển tới thư mục dự án:

```powershell
cd c:\Users\ADMIN\Desktop\TT-DTOT
```

**Bước 3**: Cài đặt dependencies:

```powershell
npm install
```

**Bước 4**: Khởi động server:

```powershell
# Mặc định (PORT=3000, MongoDB localhost)
node server.js

# Hoặc với biến môi trường tùy chỉnh
$env:PORT = 3000; $env:MONGO_URL = 'mongodb://localhost:27017'; node server.js
```

### 2. Chuẩn Bị Phần Cứng & Firmware ESP32

**Bước 1**: Cài đặt Arduino IDE từ [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

**Bước 2**: Cài đặt Board ESP32:
- Mở Arduino IDE → File → Preferences
- Thêm URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Tools → Board Manager → Tìm "esp32" → Cài đặt

**Bước 3**: Cài đặt thư viện:
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
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Địa chỉ IP server
const char* WS_HOST = "192.168.1.100";  // IP của máy chạy Node.js
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

## 🐳 Triển Khai với Docker

### Yêu Cầu

- Docker Desktop: [https://www.docker.com/products/docker-desktop](https://www.docker.com/products/docker-desktop)

### Các Bước Triển Khai

**Bước 1**: Cài đặt Docker Desktop và khởi động

**Bước 2**: Mở PowerShell trong thư mục dự án:

```powershell
cd c:\Users\ADMIN\Desktop\TT-DTOT
```

**Bước 3**: Xây dựng và chạy containers:

```powershell
docker-compose up --build
```

**Bước 4**: Truy cập ứng dụng:
- Dashboard: [http://localhost:3000](http://localhost:3000)
- MongoDB: localhost:27017 (internal)

### Dừng Containers

```powershell
docker-compose down
```

### Xây Dựng Lại

```powershell
docker-compose up --build --force-recreate
```

---

## 🔌 Sử Dụng

### Khởi Động Hệ Thống

1. **Khởi động Backend**: `node server.js` hoặc `docker-compose up`
2. **Nạp Firmware ESP32**: Upload `main3.ino` qua Arduino IDE
3. **Truy cập Dashboard**: Mở [http://localhost:3000](http://localhost:3000)

### Giám Sát Thời Gian Thực

- ESP32 tự động gửi dữ liệu mỗi 2 giây qua WebSocket
- Server xử lý và lưu vào MongoDB
- Dashboard cập nhật realtime
- OLED trên ESP32 hiển thị thông tin và cảnh báo

---

## 📡 API Documentation

### REST Endpoints

#### `GET /`
- **Mô tả**: Trang dashboard chính
- **Response**: HTML

#### `GET /limit?lat={lat}&lng={lng}&margin={margin}`
- **Mô tả**: Lấy giới hạn tốc độ tại vị trí
- **Response**: JSON với limitKmh, minKmh

#### `POST /compare`
- **Body**: `{lat, lng, speedKmh, margin, licensePlate}`
- **Response**: JSON với kết quả so sánh

#### `GET /history?licensePlate={plate}&limit={limit}`
- **Mô tả**: Lấy lịch sử dữ liệu
- **Response**: Array JSON của telemetry data

#### `GET /plates`
- **Mô tả**: Danh sách biển số xe
- **Response**: Array string

#### `GET /alert-stats?licensePlate={plate}&from={date}&to={date}`
- **Mô tả**: Thống kê cảnh báo
- **Response**: JSON với thống kê theo ngày

### WebSocket

**URL**: `ws://localhost:3000/ws`

**ESP32 → Server**:
```json
{
  "lat": 21.0278,
  "lng": 105.8342,
  "speedKmh": 45.5,
  "pressureBar": 2.1,
  "licensePlate": "72F-345.67"
}
```

**Server → ESP32**:
```json
{
  "type": "compare_result",
  "limitKmh": 50,
  "overMax": false,
  "underMin": false
}
```

**Dashboard → Server**:
```json
{"type": "dashboard_subscribe"}
```

---

## 📊 Dashboard

Giao diện web cung cấp:

- **Hiển thị realtime**: Tốc độ, áp suất, GPS
- **Cảnh báo**: Quá tốc độ, áp suất bất thường
- **Lịch sử**: Dữ liệu từ MongoDB
- **Thống kê**: Biểu đồ cảnh báo
- **Bản đồ**: Vị trí phương tiện

---

## 🐛 Debugging

### Kiểm tra Logs

**Node.js Server**:
```powershell
node server.js
```

**ESP32 Serial Monitor** (115200 baud):
- WiFi connection
- GPS fix status
- Sensor readings
- WebSocket messages

### Troubleshooting

- **ESP32 không kết nối WiFi**: Kiểm tra SSID/password
- **GPS không fix**: Đảm bảo ngoài trời hoặc gần cửa sổ
- **MongoDB lỗi**: Kiểm tra MongoDB đang chạy
- **Overpass API timeout**: Thử lại hoặc dùng cache

---

## 🤝 Đóng Góp

Chúng tôi hoan nghênh đóng góp! Vui lòng:

1. Fork repository
2. Tạo feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Tạo Pull Request

### Hướng Dẫn Phát Triển

- Sử dụng ESLint cho JavaScript
- Tuân thủ conventional commits
- Test trên ESP32 thực trước khi commit
- Cập nhật documentation

---

## 📄 Giấy Phép

Dự án này được phân phối dưới giấy phép ISC. Xem file `LICENSE` để biết thêm chi tiết.

---

## 📞 Liên Hệ

- **Tác giả**: TT-DTOT Team
- **Email**: [your-email@example.com]
- **GitHub**: [https://github.com/your-username/tt-dtot]

---

**Cập nhật lần cuối**: 2025-12-30  
**Dự án**: TT-DTOT — Hệ Thống Giám Sát Tốc Độ Thông Minh
