// ================================================================
//  GEO-SENSE AFRICA v4.3 — ESP32 MASTER NODE
//  Multi-Hazard Early Warning System — Fusion & Gateway
// ================================================================
//
//  SERIAL TELEMETRY (115200 Baud)
//  ─────────────────────────────────────────────────────────────
//  [SYS]   System heartbeat every 5s
//  [RX]    Parser feedback on valid Arduino packets
//  [ALERT] Log riskIndex or alertLevel shifts
//
//  OLED DASHBOARD (High Density)
//  ─────────────────────────────────────────────────────────────
//  Header: System Title + IP
//  Risk:   Large Risk Index Box
//  Grid:   Water, Soil, Temp, Slope
//  Footer: Alert Banner
// ================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <SPI.h>
#include <esp_task_wdt.h>

// ════════════════════════════════════════════════════════════
//  CONFIGURATION
// ════════════════════════════════════════════════════════════
const char* AP_SSID = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";
const char* STA_SSID = "";
const char* STA_PASSWORD = "";
const char* SERVER_IP = "192.168.1.100";
const uint16_t SERVER_PORT = 5000;
const char* SERVER_PATH = "/api/ingest";

#define PIN_SDA       21
#define PIN_SCL       22
#define PIN_DHT       4
#define DHT_TYPE      DHT11
#define PIN_TRIG      5
#define PIN_ECHO      18
#define PIN_PIR       19
#define PIN_SOIL      36
#define PIN_OBSTACLE  39
#define PIN_SERVO     13
#define PIN_RELAY     12
#define PIN_TOUCH     14
#define LED_GREEN     25
#define LED_BLUE      2
#define LED_YELLOW    27
#define LED_RED       33
#define SLAVE_RX      16
#define SLAVE_TX      17

#define OLED_W        128
#define OLED_H        64
#define OLED_ADDR     0x3C
#define LCD_ADDR      0x27

// ════════════════════════════════════════════════════════════
//  STATE & OBJECTS
// ════════════════════════════════════════════════════════════
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
DHT dht(PIN_DHT, DHT_TYPE);
Servo warningServo;
WebServer server(80);

struct SensorData {
  float soilMoisture, airTemp, airHumidity, distanceCm, evapotranspiration;
  bool pirMotion, obstacleDetected;
  float waterLevel, tiltPct, thermistorTemp, potCalibration;
  bool tiltInterrupt;
  float riskIndex;
  int alertLevel;
  char hazardType[12];
};

SensorData data = {50,25,60,400,0,false,false,0,0,25,1,false,0,0,"NOMINAL"};
float ema_soil = 50, ema_water = 0, ema_tilt = 0, ema_temp = 0;
#define EMA_A 0.25f

uint32_t lastSampleMs=0, lastDisplayMs=0, lastPostMs=0, lastHeartbeatMs=0;
uint32_t bootMs=0, lastSuccessPostMs=0;
#define SAMPLE_INTERVAL    3000UL
#define DISPLAY_INTERVAL   500UL
#define HEARTBEAT_INTERVAL 5000UL

char slaveBuf[80];
uint8_t slaveIdx = 0;
bool serverReachable = false, alertAcked = false;
uint32_t ackedAt = 0;
char nodeID[18] = "00:00:00:00:00:00";
int lastAlertLevel = -1;
float lastRiskIndex = -1.0f;

// ════════════════════════════════════════════════════════════
//  DASHBOARD HTML
// ════════════════════════════════════════════════════════════
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Geo-Sense v4.3</title>
<style>body{background:#0a0e1a;color:#f1f5f9;font-family:sans-serif;padding:20px}
.card{background:#111827;border:1px solid #1e293b;border-radius:12px;padding:15px;margin-bottom:15px}
.risk{font-size:48px;font-weight:bold;text-align:center;color:#10b981}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.val{font-weight:bold;color:#3b82f6}</style></head>
<body><h1>GEO-SENSE AFRICA v4.3</h1><div id="root">Loading...</div>
<script>async function poll(){try{const d=await fetch('/data').then(r=>r.json());
document.getElementById('root').innerHTML=`<div class="card">Risk: <span class="risk">${d.risk.toFixed(1)}</span>/9 [${d.hazard}]</div>
<div class="grid"><div class="card">Water: <span class="val">${d.water.toFixed(1)}%</span></div>
<div class="card">Soil: <span class="val">${d.soil.toFixed(1)}%</span></div>
<div class="card">Temp: <span class="val">${d.airT.toFixed(1)}C</span></div>
<div class="card">Slope: <span class="val">${d.tilt.toFixed(1)}%</span></div></div>`;}catch(e){}}
setInterval(poll,2000);poll();</script></body></html>
)rawliteral";

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,      // 10 seconds
    .idle_core_mask = 0,      // watch both cores (0 = both)
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  Wire.begin(PIN_SDA, PIN_SCL);

  pinMode(PIN_TRIG, OUTPUT); digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, HIGH);
  pinMode(LED_GREEN, OUTPUT); digitalWrite(LED_GREEN, LOW);
  pinMode(LED_BLUE, OUTPUT); digitalWrite(LED_BLUE, LOW);
  pinMode(LED_YELLOW, OUTPUT); digitalWrite(LED_YELLOW, LOW);
  pinMode(LED_RED, OUTPUT); digitalWrite(LED_RED, LOW);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TOUCH, INPUT);

  if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1); oled.setCursor(0,0); oled.println("GEO-SENSE v4.3");
    oled.display();
  }
  lcd.init(); lcd.backlight();
  dht.begin();
  warningServo.attach(PIN_SERVO); warningServo.write(0);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(nodeID, sizeof(nodeID), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  server.on("/", []() { server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleDataRequest);
  server.begin();

  bootMs = millis();
  Serial.println(F("[SYS] GEO-SENSE Master v4.3 Ready"));
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  uint32_t now = millis();

  while (Serial2.available()) handleSlaveSerial();

  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readLocalSensors();
    computeRisk();
    updateOutputs();
    Serial2.printf("CMD:%d\n", data.alertLevel);
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    updateOLED();
    updateLCD();
  }

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL) {
    lastHeartbeatMs = now;
    Serial.printf("[SYS] WiFi: %s | IP: %s | Uptime: %lus\n", 
                  AP_SSID, WiFi.softAPIP().toString().c_str(), (now-bootMs)/1000);
  }

  if (digitalRead(PIN_TOUCH) == HIGH && !alertAcked) {
    alertAcked = true; ackedAt = now;
    digitalWrite(PIN_RELAY, HIGH); warningServo.write(0);
    lcd.clear(); lcd.print("Alert ACK'd");
  }
  if (alertAcked && (now - ackedAt > 60000UL)) alertAcked = false;
}

void readLocalSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) data.airTemp = t;
  if (!isnan(h)) data.airHumidity = h;
  data.soilMoisture = constrain((float)map(analogRead(PIN_SOIL), 3200, 1200, 0, 100), 0.0f, 100.0f);
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  uint32_t dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  data.distanceCm = (dur == 0) ? 400.0f : (float)dur * 0.01715f;
  data.pirMotion = digitalRead(PIN_PIR);
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);
}

void handleSlaveSerial() {
  char c = (char)Serial2.read();
  if (c == '\n' || c == '\r') {
    if (slaveIdx > 0) {
      slaveBuf[slaveIdx] = '\0';
      parseSlaveData(slaveBuf);
    }
    slaveIdx = 0;
  } else if (slaveIdx < 79) slaveBuf[slaveIdx++] = c;
}

void parseSlaveData(char* line) {
  auto getF = [&](const char* k) -> float { char* p = strstr(line, k); return p ? atof(p+strlen(k)) : -999.0f; };
  float w = getF("W:"); if (w > -999.0f) data.waterLevel = w;
  float t = getF("T:"); if (t > -999.0f) data.thermistorTemp = t;
  float s = getF("S:"); if (s > -999.0f) data.tiltPct = s;
  float p = getF("P:"); if (p > -999.0f) data.potCalibration = p;
  char* ip = strstr(line, "I:"); if (ip) data.tiltInterrupt = (atoi(ip+2) == 1);
  
  Serial.printf("[RX] Data Sync - W:%.1f T:%.1f S:%.1f Risk:%.0f\n", 
                data.waterLevel, data.thermistorTemp, data.tiltPct, data.riskIndex * 10);
}

void computeRisk() {
  if (data.tiltInterrupt) { data.riskIndex = 9.0f; data.alertLevel = 2; strcpy(data.hazardType, "LANDSLIDE"); }
  else {
    float floodPct = constrain((float)map((int)data.distanceCm, 200, 20, 0, 100), 0.0f, 100.0f);
    ema_soil = EMA_A * data.soilMoisture + (1-EMA_A)*ema_soil;
    ema_water = EMA_A * max(floodPct, data.waterLevel) + (1-EMA_A)*ema_water;
    ema_tilt = EMA_A * data.tiltPct + (1-EMA_A)*ema_tilt;
    float w = (ema_soil*0.25f) + (ema_water*0.40f) + (ema_tilt*0.35f);
    data.riskIndex = (w * data.potCalibration / 100.0f) * 9.0f;
    data.alertLevel = (data.riskIndex > 6.0f) ? 2 : (data.riskIndex > 3.0f) ? 1 : 0;
    if (ema_tilt > 50) strcpy(data.hazardType, "SLOPE");
    else if (ema_water > 60) strcpy(data.hazardType, "FLOOD");
    else if (ema_soil > 70) strcpy(data.hazardType, "DROUGHT");
    else strcpy(data.hazardType, "SAFE");
  }

  if (data.alertLevel != lastAlertLevel) {
    const char* levels[] = {"SAFE", "CAUTION", "CRITICAL"};
    Serial.printf("[ALERT] Level Shift: %s -> %s | Cause: %s\n", 
                  levels[max(0, lastAlertLevel)], levels[data.alertLevel], data.hazardType);
    lastAlertLevel = data.alertLevel;
  }
}

void updateOutputs() {
  bool crit = (data.alertLevel >= 2 || data.tiltInterrupt) && !alertAcked;
  digitalWrite(LED_GREEN, (data.alertLevel == 0));
  digitalWrite(LED_BLUE, (strcmp(data.hazardType, "FLOOD") == 0));
  digitalWrite(LED_YELLOW, (data.alertLevel == 1));
  digitalWrite(LED_RED, crit);
  digitalWrite(PIN_RELAY, crit ? LOW : HIGH);
  warningServo.write(data.tiltInterrupt ? 180 : (data.alertLevel == 2 ? 135 : (data.alertLevel == 1 ? 80 : 0)));
}

void updateOLED() {
  oled.clearDisplay();
  oled.setTextSize(1); oled.setCursor(0,0); oled.printf("SYS:%s  IP:4.1", data.hazardType);
  oled.drawRect(0, 12, 128, 32, SSD1306_WHITE);
  oled.setTextSize(3); oled.setCursor(10, 18); oled.printf("R:%.1f", data.riskIndex);
  oled.setTextSize(1); oled.setCursor(95, 28); oled.print("/9");
  oled.setCursor(0, 48); oled.printf("W:%d%% S:%d%% T:%dC", (int)data.waterLevel, (int)data.soilMoisture, (int)data.airTemp);
  if (data.alertLevel > 0) {
    oled.fillRect(0, 58, 128, 6, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK); oled.setCursor(5, 58);
    oled.print(data.alertLevel == 2 ? "!!! CRITICAL !!!" : "--- WARNING ---");
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.display();
}

void updateLCD() {
  lcd.setCursor(0,0); lcd.printf("%-8s R:%.1f/9", data.hazardType, data.riskIndex);
  lcd.setCursor(0,1); lcd.printf("W:%d%% S:%d%% %s", (int)data.waterLevel, (int)data.soilMoisture, alertAcked ? "ACK" : "");
}

void handleDataRequest() {
  char buf[512];
  snprintf(buf, sizeof(buf), "{\"risk\":%.2f,\"hazard\":\"%s\",\"water\":%.1f,\"soil\":%.1f,\"airT\":%.1f,\"tilt\":%.1f,\"uptime\":%lu}",
           data.riskIndex, data.hazardType, data.waterLevel, data.soilMoisture, data.airTemp, data.tiltPct, (millis()-bootMs)/1000);
  server.send(200, "application/json", buf);
}
