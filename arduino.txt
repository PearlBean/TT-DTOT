// demo thực tế

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_BMP085.h>   // BMP180 compatible
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>

// ================== WiFi / WebSocket ==================
const char* WIFI_SSID = "P4";          // <-- WiFi
const char* WIFI_PASS = "p45024ltm";   // <-- WiFi

// ws://<PC-IP>:3000/ws
const char* WS_HOST = "192.168.1.8";   // <-- IP PC chạy server
const uint16_t WS_PORT = 3000;
const char* WS_PATH = "/ws";

// ================== OLED / LED ==================
#define LED_PIN 19
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================== Encoder (GPIO34) ==================
// Ghi chú: GPIO34 input-only, không có pull-up nội -> cần kéo lên/kéo xuống ngoài nếu đầu ra encoder là dạng hở
#define ENC_PIN               34
#define PULSES_PER_REV        20        // <-- số xung/vòng theo encoder của bạn
#define SAMPLE_MS             500       // khoảng lấy mẫu tốc độ
#define MIN_PULSE_US          200       // lọc nhiễu biên
#define WHEEL_CIRCUMFERENCE_M 0.21f     // <-- chu vi bánh/đĩa (m)

volatile uint32_t encPulseCount = 0;
volatile uint32_t encLastPulseUs = 0;
void IRAM_ATTR onEncPulse() {
  uint32_t nowUs = micros();
  if (nowUs - encLastPulseUs >= MIN_PULSE_US) {
    encPulseCount++;
    encLastPulseUs = nowUs;
  }
}

// ================== BMP180 (I2C) ==================
Adafruit_BMP085 bmp;

// ================== GPS (UART2) ==================
#define GPS_RX 16   // ESP32 RX2  ← TX GPS
#define GPS_TX 17   // ESP32 TX2  → RX GPS
#define GPS_BAUD 9600
HardwareSerial SerialGPS(2);
TinyGPSPlus gps;

// ================== Trạng thái & ngưỡng ==================
WebSocketsClient ws;
bool wsConnected = false;

float speed_kmh    = 0.0f;   // từ encoder
float pressure_bar = 0.0f;   // từ BMP180

// Ngưỡng áp suất (cục bộ cho OLED/LED)
float PRESSURE_LIMIT_MIN = 1.00f;  // bar
float PRESSURE_LIMIT_MAX = 2.00f;  // bar

// GPS: current + last-fix
bool   gpsFix = false;          // có fix hiện tại?
double curLat = 0.0, curLng = 0.0;
double lastLat = 0.0, lastLng = 0.0;
unsigned long lastFixMs = 0;    // thời điểm có fix lần gần nhất (millis)

// Phản hồi server cho tốc độ
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

// ================== Helpers hiển thị ==================
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

// ================== Màn hình ==================
void showNormal(bool showGpsLine = true) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText(F("GIAM SAT"), 0, 1);

  // Tốc độ
  display.setTextSize(1);
  display.setCursor(2, 14); display.print(F("Toc do: "));
  display.setTextSize(2);   display.print(speed_kmh, 2);
  display.setTextSize(1);   display.setCursor(100, 22); display.print(F("km/h"));

  // Áp suất
  display.setTextSize(1);
  display.setCursor(2, 36); display.print(F("Ap suat: "));
  display.setTextSize(2);   display.print(pressure_bar, 2);
  display.setTextSize(1);   display.setCursor(100, 44); display.print(F("bar"));

  // GPS dòng trạng thái
  if (showGpsLine) {
    display.setTextSize(1);
    display.setCursor(2, 56);
    if (gpsFix) {
      display.print(F("GPS:"));
      display.print(curLat, 4);
      display.print(",");
      display.print(curLng, 4);
    } else {
      unsigned long age = (millis() - lastFixMs) / 1000;
      display.print(F("GPS:"));
      display.print(age);
      display.print(F("s)"));
    }
  }

  display.display();
}

void showAlert() {
  bool pressHigh = (pressure_bar > PRESSURE_LIMIT_MAX);
  bool pressLow  = (pressure_bar < PRESSURE_LIMIT_MIN);
  bool spdAlert  = (srv_overMax || srv_underMin);
  bool prsAlert  = (pressHigh || pressLow);

  display.clearDisplay();

  if (blinkOn) {
    display.setTextColor(SSD1306_WHITE);
    drawCenteredText(F("CANH BAO!"), 2, 2);
  }

  String lines[6]; 
  uint8_t n = 0;

  if (prsAlert && !spdAlert) {
    if (pressHigh) lines[n++] = "Ap suat cao!";
    if (pressLow)  lines[n++] = "Ap suat thap!";
    lines[n++] = String(PRESSURE_LIMIT_MIN,2) + "-" + String(PRESSURE_LIMIT_MAX,2) + " bar";
  }
  else if (spdAlert && !prsAlert) {
    if (srv_overMax)  lines[n++] = "Qua toc do!";
    if (srv_underMin) lines[n++] = "Toc do thap!";
    if (srv_limit_kmh >= 0) lines[n++] = String("Max: ") + srv_limit_kmh + " km/h";
  }
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

// ================== LED ==================
void updateLED() {
  bool pressHigh = (pressure_bar > PRESSURE_LIMIT_MAX);
  bool pressLow  = (pressure_bar < PRESSURE_LIMIT_MIN);
  bool alert = srv_overMax || srv_underMin || pressHigh || pressLow;
  digitalWrite(LED_PIN, (alert && blinkOn) ? HIGH : LOW);
}

// ================== WebSocket ==================
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
  uint32_t now = millis();
  if (!wsConnected || (now - lastSendMs < SEND_INTERVAL_MS)) return;
  lastSendMs = now;

  // Dùng vị trí hiện tại nếu có fix; nếu không thì dùng last fix (nếu đã từng có)
  double sendLat = gpsFix ? curLat : lastLat;
  double sendLng = gpsFix ? curLng : lastLng;
  unsigned long fixAgeMs = (lastFixMs == 0) ? (unsigned long)UINT32_MAX : (millis() - lastFixMs);

  StaticJsonDocument<256> doc;
  doc["lat"] = sendLat;
  doc["lng"] = sendLng;
  doc["gpsFix"] = gpsFix;
  doc["fixAgeMs"] = fixAgeMs;
  doc["speedKmh"] = speed_kmh;       // tốc độ từ encoder
  doc["pressureBar"] = pressure_bar; // áp suất từ BMP180
  doc["margin"] = 5;

  String out; serializeJson(doc, out);
  ws.sendTXT(out);
}

// ================== Đọc cảm biến ==================
void pollGPS() {
  while (SerialGPS.available()) gps.encode(SerialGPS.read());

  bool validLoc = gps.location.isValid();
  if (gps.location.isUpdated() && validLoc) {
    curLat = gps.location.lat();
    curLng = gps.location.lng();
    gpsFix = true;
    lastLat = curLat;
    lastLng = curLng;
    lastFixMs = millis();
  } else if (!validLoc) {
    gpsFix = false;
  }
}

bool readPressureBMP180(float &barOut) {
  // readPressure() trả Pa; trả 0 nếu lỗi
  int32_t Pa = bmp.readPressure();
  if (Pa <= 0) return false;
  barOut = Pa / 100000.0f; // Pa -> bar
  return true;
}

// ================== Setup / loop ==================
void setup() {
  Serial.begin(115200); delay(200);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);

  // I2C (OLED + BMP180)
  Wire.begin(21, 22);  // SDA=21, SCL=22

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERR] OLED not found (0x3C)!"));
    for(;;);
  }
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
  drawCenteredText(F("Starting"), 22, 2); display.display(); delay(600);

  // BMP180
  if (!bmp.begin()) {
    Serial.println(F("[ERR] BMP180 not found (0x77)!"));
  } else {
    Serial.println(F("[BMP180] OK"));
  }

  // GPS UART2
  SerialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println(F("[GPS] UART2 started"));

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Connecting");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(400); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) { Serial.print("[WiFi] "); Serial.println(WiFi.localIP()); }
  else { Serial.println("[WiFi] Offline."); }

  // WebSocket
  ws.onEvent(onWSEvent);
  ws.begin(WS_HOST, WS_PORT, WS_PATH);
  ws.setReconnectInterval(2000);
  ws.enableHeartbeat(15000, 3000, 2);

  // Encoder
  pinMode(ENC_PIN, INPUT); // cần phần cứng kéo lên/kéo xuống phù hợp
  attachInterrupt(digitalPinToInterrupt(ENC_PIN), onEncPulse, RISING);
}

void loop() {
  // 1) Nhấp nháy + WS
  updateBlink();
  ws.loop();

  // 2) Cập nhật GPS + BMP180
  pollGPS();

  float pbar;
  if (readPressureBMP180(pbar)) {
    pressure_bar = pbar;
  }

  // 3) Tính tốc độ từ encoder mỗi SAMPLE_MS
  static uint32_t lastCalcMs = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastCalcMs >= SAMPLE_MS) {
    lastCalcMs = nowMs;

    noInterrupts();
    uint32_t pulses = encPulseCount;
    encPulseCount = 0;
    interrupts();

    float revolutions = (float)pulses / (float)PULSES_PER_REV;    // vòng trong SAMPLE_MS
    float rps = revolutions * (1000.0f / (float)SAMPLE_MS);       // vòng/giây
    float v_mps = rps * WHEEL_CIRCUMFERENCE_M;                    // m/s
    speed_kmh = v_mps * 3.6f;                                     // km/h

    Serial.printf("[ENC] pulses=%lu -> %.2f km/h | P=%.3f bar | GPS:%s (age %lus)\n",
      (unsigned long)pulses, speed_kmh, pressure_bar, gpsFix ? "FIX" : "NO", (lastFixMs==0)?999999:((millis()-lastFixMs)/1000));
  }

  // 4) Gửi WS
  sendWS();

  // 5) Hiển thị + LED
  bool pressHigh = (pressure_bar > PRESSURE_LIMIT_MAX);
  bool pressLow  = (pressure_bar < PRESSURE_LIMIT_MIN);
  bool anyAlert  = srv_overMax || srv_underMin || pressHigh || pressLow;
  updateLED();
  if (anyAlert) showAlert(); else showNormal(true);

  delay(10);
}
