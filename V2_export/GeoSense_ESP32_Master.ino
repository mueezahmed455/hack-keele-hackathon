// ================================================================
//  GEO-SENSE AFRICA v2.2 — ESP32 MASTER NODE (PRODUCTION READY)
//  Role: Central Hub, WiFi Dashboard, OLED Display, Data Fusion
//  Version: 2.2.0 - Security & Stability Hardened
// ================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <cstdio>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

// ── VERSION CONFIGURATION ──────────────────────────────────────
#define FIRMWARE_VERSION "2.2.0"

// ── WIFI CONFIGURATION ──────────────────────────────────────
// NOTE: For production, store these in NVS with encryption
const char* AP_SSID     = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";
const byte DNS_PORT     = 53;
IPAddress apIP(192.168.4.1);

// ── WEB AUTHENTICATION ──────────────────────────────────────
const char* WEB_USERNAME = "admin";
const char* WEB_PASSWORD = "geosense_admin";

// ── PIN DEFINITIONS (ESP32) ─────────────────────────────────
// I2C Bus
#define PIN_SDA         21
#define PIN_SCL         22

// DHT11 Sensor
#define PIN_DHT         13   // Moved from GPIO4 to avoid conflict with LED_BLUE
#define DHT_TYPE        DHT11

// HC-SR04 Ultrasonic (Flood Distance)
#define PIN_TRIG        5
#define PIN_ECHO        17   // Note: Requires voltage divider (5V -> 3.3V)
                             // Moved from GPIO18 to avoid conflict with SPI SCK

// PIR Motion Sensor
#define PIN_PIR         19

// Servo & Relay
#define PIN_SERVO       2    // Moved from GPIO13 to avoid conflict with JOY_BTN
#define PIN_RELAY       12

// UI Controls
#define PIN_TOUCH       32   // Moved from GPIO14 to avoid conflict with LED_RED
#define PIN_JOY_X       34   // ADC1 only
#define PIN_JOY_Y       35   // ADC1 only
#define PIN_JOY_BTN     13   // Moved from GPIO32 to avoid conflict with PIN_TOUCH

// Dot Matrix 8x8 (SPI)
#define PIN_DM_DATA     23   // MOSI
#define PIN_DM_CLK      26   // SCK (MOVED from GPIO18 to avoid conflict with ECHO)
#define PIN_DM_CS       33   // SS (MOVED from GPIO15 to avoid conflict with SLAVE_TX)

// Status LEDs
#define LED_GREEN       25
#define LED_BLUE        4    // Moved from GPIO2 to avoid conflict with SERVO (now on GPIO2)
#define LED_YELLOW      27
#define LED_RED         14   // Moved from GPIO33 to avoid conflict with DM_CS

// Analog Sensors
#define PIN_SOIL        36   // ADC1 only
#define PIN_OBSTACLE    39   // ADC1 only

// Serial2 to Arduino Slave
#define SLAVE_RX        16   // UART2 RX
#define SLAVE_TX        15   // UART2 TX - Moved from GPIO17 to avoid conflict with ECHO

// ── SENSOR CALIBRATION CONSTANTS ────────────────────────────
#define SOIL_SENSOR_MIN_ADC     1200
#define SOIL_SENSOR_MAX_ADC     4095
#define FLOOD_DISTANCE_MIN_CM   20
#define FLOOD_DISTANCE_MAX_CM   200
#define DHT_TEMP_MIN            -40.0f
#define DHT_TEMP_MAX            80.0f
#define DHT_HUMIDITY_MIN        0.0f
#define DHT_HUMIDITY_MAX        100.0f

// ── RISK CALCULATION WEIGHTS ────────────────────────────────
#define WEIGHT_SOIL             0.3f
#define WEIGHT_WATER            0.4f
#define WEIGHT_TILT             0.3f
#define BONUS_PIR_MOTION        10.0f
#define BONUS_OBSTACLE          8.0f

// ── TIMING CONSTANTS ────────────────────────────────────────
#define SAMPLE_INTERVAL_MS      3000
#define DISPLAY_INTERVAL_MS     500
#define SCROLL_INTERVAL_MS      100
#define SERIAL_READ_TIMEOUT_MS  100
#define MAX_SERIAL_ITERATIONS   10

// ── DISPLAY OBJECTS ─────────────────────────────────────────
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C
#define LCD_ADDR        0x27
#define LCD_COLS        16
#define LCD_ROWS        2

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

#define MAX_DEVICES     1
MD_MAX72XX dotMatrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, PIN_DM_CS, PIN_DM_DATA, PIN_DM_CLK, MAX_DEVICES);

DHT dht(PIN_DHT, DHT_TYPE);
Servo warningServo;
WebServer server(80);
DNSServer dnsServer;

// ── DATA STRUCTURES ─────────────────────────────────────────
struct SensorData {
  float soilMoisture;
  float airTemp;
  float airHumidity;
  float distanceCm;
  bool  pirMotion;
  bool  obstacleDetected;
  bool  touchPressed;
  float waterLevel;
  float tiltAngle;
  float thermistorTemp;
  float potCalibration;
  bool  tiltInterrupt;
  float riskIndex;
  float evapotranspiration;
  const char* hazardType;
  int   alertLevel;
};

SensorData data = {0, 25.0, 50.0, 400.0, false, false, false, 0, 0, 25.0, 1.0, false, 0, 0, "NOMINAL", 0};

// ── THREAD SAFETY: Mutex for shared data access ─────────────
SemaphoreHandle_t dataMutex = NULL;

// ── KALMAN FILTER STATE ─────────────────────────────────────
struct KalmanState {
  float q;  // Process noise covariance
  float r;  // Measurement noise covariance
  float x;  // Estimated value
  float p;  // Estimation error covariance
  float k;  // Kalman gain
};

KalmanState kf_dist = {0.01, 0.1, 400.0, 1.0, 0};
KalmanState kf_tilt = {0.01, 0.1, 0.0, 1.0, 0};

// ── KALMAN FILTER UPDATE FUNCTION ───────────────────────────
float kalmanUpdate(KalmanState* state, float measurement) {
  if (state == nullptr || isnan(measurement)) return state->x;
  
  // Prediction step
  state->p = state->p + state->q;
  
  // Update step
  state->k = state->p / (state->p + state->r);
  state->x = state->x + state->k * (measurement - state->x);
  state->p = (1.0f - state->k) * state->p;
  
  return state->x;
}

// ── EMA SMOOTHING BUFFERS ───────────────────────────────────
float ema_soil = 0, ema_water = 0, ema_tilt = 0, ema_temp = 0, ema_dist = 0;
#define EMA_A 0.25f

// ── TIMING VARIABLES ────────────────────────────────────────
uint32_t lastSampleMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastScrollMs = 0;
uint32_t systemStartMs = 0;

// ── RISK HISTORY BUFFER ─────────────────────────────────────
#define HISTORY_LEN  50
float riskHistory[HISTORY_LEN];
int historyIdx = 0;

// ── MENU STATE ──────────────────────────────────────────────
int menuPage = 0;
#define MENU_PAGES 4

// ── SERIAL BUFFER FOR SLAVE COMMUNICATION ───────────────────
#define SLAVE_BUF_SIZE 64
char slaveBuffer[SLAVE_BUF_SIZE];
uint8_t slaveIdx = 0;

// ── WEB DASHBOARD HTML ───────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawHTML(
<!DOCTYPE html><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="manifest" href="/manifest.json">
<title>GEO-SENSE Africa | Live Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
:root{--bg:#0a0e14;--card:#111720;--border:#1e2d45;--text:#e8eaed;--sub:#6b7a93;--accent:#00ff88;--flood:#5bb8ff;--drought:#f5a623;--landslide:#ff6b6b}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;padding:15px;line-height:1.4}
.container{max-width:1000px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;border-bottom:1px solid var(--border);padding-bottom:10px}
h1{font-size:18px;color:var(--accent);letter-spacing:2px;text-transform:uppercase}
.status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--accent);margin-right:5px;box-shadow:0 0 8px var(--accent)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:15px}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;transition:transform 0.2s}
.card:hover{border-color:var(--accent)}
.label{font-size:10px;font-weight:700;letter-spacing:1px;color:var(--sub);text-transform:uppercase;margin-bottom:8px;display:flex;justify-content:space-between}
.val{font-size:32px;font-weight:800;margin-bottom:4px}
.unit{font-size:14px;color:var(--sub);font-weight:400;margin-left:4px}
.chart-container{height:180px;margin-top:10px;position:relative}
.badge{padding:4px 10px;border-radius:4px;font-size:10px;font-weight:700}
.bg-green{background:rgba(93,216,130,0.1);color:#5dd882;border:1px solid #5dd882}
.bg-amber{background:rgba(245,166,35,0.1);color:#f5a623;border:1px solid #f5a623}
.bg-red{background:rgba(232,69,69,0.1);color:#ff6b6b;border:1px solid #ff6b6b}
.sensor-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.05);font-size:13px}
.sensor-row:last-child{border:none}
.alert-banner{grid-column:1/-1;background:rgba(255,107,107,0.1);border:1px solid var(--landslide);padding:15px;border-radius:12px;display:none;animation:pulse 2s infinite}
@keyframes pulse{0%{opacity:1}50%{opacity:0.6}100%{opacity:1}}
.btn{display:inline-block;background:var(--border);color:var(--text);padding:8px 15px;border-radius:6px;text-decoration:none;font-size:12px;margin-top:10px;border:1px solid var(--border)}
.btn:hover{background:var(--card);border-color:var(--accent)}
footer{text-align:center;margin-top:30px;font-size:11px;color:var(--sub)}
</style>
</head>
<body>
<div class="container">
  <header>
    <h1><span class="status-dot"></span>Geo-Sense Africa</h1>
    <div id="connection-time" style="font-size:10px;color:var(--sub)">Connecting...</div>
  </header>

  <div class="grid">
    <div id="critical-alert" class="alert-banner">
      <strong style="color:var(--landslide)">CRITICAL ALERT:</strong> <span id="alert-msg">Potential Landslide/Flood detected. Immediate action required.</span>
    </div>

    <div class="card">
      <div class="label">Hazard Status <span id="h-badge" class="badge bg-green">NOMINAL</span></div>
      <div class="val" id="hazard-type">--</div>
      <div class="label" style="margin-top:15px">Risk Index Progression</div>
      <div class="chart-container"><canvas id="riskChart"></canvas></div>
    </div>

    <div class="card">
      <div class="label">Flood Monitor (River Level)</div>
      <div class="val" id="water-val">--<span class="unit">%</span></div>
      <div class="sensor-row"><span>Surface Distance</span><span id="dist-val">-- cm</span></div>
      <div class="sensor-row"><span>Obstacle/Debris</span><span id="obs-val">Clear</span></div>
      <div class="sensor-row"><span>Flood Gate Status</span><span id="servo-val">--</span></div>
      <div class="chart-container" style="height:120px"><canvas id="floodChart"></canvas></div>
    </div>

    <div class="card">
      <div class="label">Drought & Agriculture</div>
      <div class="val" id="soil-val">--<span class="unit">% Moisture</span></div>
      <div class="sensor-row"><span>Air Temperature</span><span id="temp-val">--°C</span></div>
      <div class="sensor-row"><span>Air Humidity</span><span id="hum-val">--%</span></div>
      <div class="sensor-row"><span>Soil Temperature</span><span id="soil-t-val">--°C</span></div>
      <div class="chart-container" style="height:120px"><canvas id="soilChart"></canvas></div>
    </div>

    <div class="card">
      <div class="label">Landslide & Geo-Stability</div>
      <div class="val" id="tilt-val">--<span class="unit">% Slope</span></div>
      <div class="sensor-row"><span>Hardware Interrupt</span><span id="lsi-val">OK</span></div>
      <div class="sensor-row"><span>System Calibration</span><span id="pot-val">1.0x</span></div>
      <div class="sensor-row"><span>PIR Activity</span><span id="pir-val">None</span></div>
      <div class="sensor-row"><span>Siren/Relay</span><span id="relay-val">Off</span></div>
      <a href="/download" class="btn">&#128190; Download Data Logs (CSV)</a>
    </div>
  </div>

  <footer>
    &copy; 2024 Geo-Sense Africa v2.2 &bull; <a href="http://geosense.local" style="color:var(--accent);text-decoration:none">geosense.local</a> &bull; Uptime: <span id="uptime">0s</span><br>
    Hardware: ESP32 Master + Arduino Uno Slave Fusion Node
  </footer>
</div>

<script>
let riskHistory = Array(20).fill(0);
let floodHistory = Array(20).fill(0);
let soilHistory = Array(20).fill(0);
let labels = Array(20).fill('');

const ctxR = document.getElementById('riskChart').getContext('2d');
const riskChart = new Chart(ctxR, {
  type: 'line',
  data: { labels, datasets: [{ label:'Risk', data: riskHistory, borderColor:'#00ff88', tension:0.4, fill:true, backgroundColor:'rgba(0,255,136,0.1)' }] },
  options: { responsive:true, maintainAspectRatio:false, scales:{y:{min:0,max:9,grid:{color:'#1e2d45'}},x:{display:false}}, plugins:{legend:{display:false}} }
});

const ctxF = document.getElementById('floodChart').getContext('2d');
const floodChart = new Chart(ctxF, {
  type: 'bar',
  data: { labels, datasets: [{ label:'Water %', data: floodHistory, backgroundColor:'#5bb8ff' }] },
  options: { responsive:true, maintainAspectRatio:false, scales:{y:{min:0,max:100,grid:{color:'#1e2d45'}},x:{display:false}}, plugins:{legend:{display:false}} }
});

const ctxS = document.getElementById('soilChart').getContext('2d');
const soilChart = new Chart(ctxS, {
  type: 'line',
  data: { labels, datasets: [{ label:'Soil %', data: soilHistory, borderColor:'#f5a623', tension:0.4 }] },
  options: { responsive:true, maintainAspectRatio:false, scales:{y:{min:0,max:100,grid:{color:'#1e2d45'}},x:{display:false}}, plugins:{legend:{display:false}} }
});

function updateData() {
  fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('hazard-type').textContent = d.hazard;
    document.getElementById('water-val').innerHTML = d.water.toFixed(1) + '<span class="unit">%</span>';
    document.getElementById('soil-val').innerHTML = d.soil.toFixed(1) + '<span class="unit">%</span>';
    document.getElementById('temp-val').textContent = d.airT.toFixed(1) + '°C';
    document.getElementById('hum-val').textContent = d.airH.toFixed(1) + '%';
    document.getElementById('soil-t-val').textContent = d.soilT.toFixed(1) + '°C';
    document.getElementById('dist-val').textContent = d.dist.toFixed(1) + ' cm';
    document.getElementById('tilt-val').innerHTML = d.tilt.toFixed(1) + '<span class="unit">%</span>';
    document.getElementById('pot-val').textContent = d.pot.toFixed(2) + 'x';
    document.getElementById('obs-val').textContent = d.obs ? 'DEBRIS DETECTED' : 'Clear';
    document.getElementById('obs-val').style.color = d.obs ? 'var(--drought)' : 'var(--accent)';
    document.getElementById('pir-val').textContent = d.pir ? 'MOTION' : 'Clear';
    document.getElementById('lsi-val').textContent = d.lsi ? '!! TRIGGERED !!' : 'OK';
    document.getElementById('lsi-val').style.color = d.lsi ? 'var(--landslide)' : 'var(--accent)';
    document.getElementById('relay-val').textContent = d.relay ? 'ACTIVE' : 'Off';
    document.getElementById('servo-val').textContent = d.servoPos + '°';

    const hb = document.getElementById('h-badge');
    if (d.risk > 6) { hb.className='badge bg-red'; hb.textContent='CRITICAL'; }
    else if (d.risk > 3) { hb.className='badge bg-amber'; hb.textContent='WARNING'; }
    else { hb.className='badge bg-green'; hb.textContent='NOMINAL'; }

    document.getElementById('critical-alert').style.display = (d.risk > 7 || d.lsi) ? 'block' : 'none';
    if(d.lsi) document.getElementById('alert-msg').textContent = 'LANDSLIDE SENSOR TRIGGERED! EVACUATE NOW.';
    else if(d.risk > 7) document.getElementById('alert-msg').textContent = 'HIGH RISK OF ' + d.hazard + '. PREPARE FOR ACTION.';

    riskHistory.push(d.risk); riskHistory.shift();
    floodHistory.push(d.water); floodHistory.shift();
    soilHistory.push(d.soil); soilHistory.shift();

    riskChart.update('none');
    floodChart.update('none');
    soilChart.update('none');

    document.getElementById('connection-time').textContent = 'Last update: ' + new Date().toLocaleTimeString();
  }).catch(e=>console.error(e));
}

setInterval(updateData, 2000);
updateData();

let up = 0;
setInterval(()=>{ up = (up + 1) % 86400; document.getElementById('uptime').textContent = up + 's'; }, 1000);
</script>
</body></html>
)rawHTML";

// ── FUNCTION PROTOTYPES ─────────────────────────────────────
void readLocalSensors();
void handleSlaveSerial();
void parseSlaveData(char* line, size_t len);
void computeRisk();
void updateOutputs();
void handleJoystick();
void updateOLED();
void updateLCD();
void handleDataRequest();
void handleDownloadRequest();
void logToFile();
void bootSequence();

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  Wire.begin(PIN_SDA, PIN_SCL);

  // Configure ADC resolution for ESP32
  analogReadResolution(12);  // ESP32 has 12-bit ADC (0-4095)
  analogSetAttenuation(ADC_11db);  // 0-3.3V range

  // Initialize mutex for thread-safe data access
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("Failed to create data mutex");
  }

  // Initialize Filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed - entering safe mode");
    // Enter safe mode: flash LEDs continuously
    pinMode(LED_RED, OUTPUT);
    while (1) {
      digitalWrite(LED_RED, HIGH);
      delay(500);
      digitalWrite(LED_RED, LOW);
      delay(500);
    }
  }

  // Initialize all LED pins first for boot sequence
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  // Boot sequence (visual feedback)
  bootSequence();

  // Initialize OLED Display
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found. Check wiring and address (0x3C).");
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("GEO-SENSE AFRICA");
    oled.printf("v%s BOOTING...", FIRMWARE_VERSION);
    oled.display();
  }

  // Initialize LCD Display
  lcd.init();
  lcd.backlight();
  lcd.print("GEO-SENSE AFRICA");

  // Initialize DHT11
  dht.begin();

  // Initialize Pin Modes
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TOUCH, INPUT);
  pinMode(PIN_JOY_BTN, INPUT_PULLUP);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);  // Relay OFF (active LOW)

  // Initialize Servo
  warningServo.attach(PIN_SERVO);
  warningServo.write(0);

  // Initialize Dot Matrix
  dotMatrix.begin();
  dotMatrix.control(MD_MAX72XX::INTENSITY, 2);
  dotMatrix.clear();

  // Start WiFi Access Point with Captive Portal
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.setSleep(true); // Enable Modem Sleep for power saving

  dnsServer.start(DNS_PORT, "*", apIP);

  if (MDNS.begin("geosense")) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS responder started: geosense.local\n");
  }

  // Setup Web Server with Authentication
  server.on("/", []() {
    if (!server.authenticate(WEB_USERNAME, WEB_PASSWORD)) {
      server.requestAuthentication();
      return;
    }
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  
  server.on("/data", handleDataRequest);
  server.on("/download", handleDownloadRequest);
  server.on("/manifest.json", []() {
    server.send(200, "application/json", "{\"name\":\"GeoSense Africa\",\"short_name\":\"GeoSense\",\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#0a0e14\",\"theme_color\":\"#00ff88\",\"icons\":[{\"src\":\"https://cdn-icons-png.flaticon.com/512/2503/2503508.png\",\"sizes\":\"512x512\",\"type\":\"image/png\"}]}");
  });

  // Captive Portal Redirect (must be last)
  server.onNotFound([]() {
    if (server.uri() == "/manifest.json" || server.uri() == "/data") {
      server.send(404, "text/plain", "Not Found");
      return;
    }
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  // Initialize Risk History
  for (int i = 0; i < HISTORY_LEN; i++) riskHistory[i] = 0;

  // Initialize EMA buffers with default values
  ema_soil = data.soilMoisture;
  ema_water = data.waterLevel;
  ema_tilt = data.tiltAngle;
  ema_temp = data.airTemp;
  ema_dist = data.distanceCm;

  // Initialize Watchdog Timer (10 second timeout)
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  systemStartMs = millis();
  Serial.printf("Geo-Sense Africa v%s - Master Node Ready\n", FIRMWARE_VERSION);
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();

  // Process captive portal DNS requests
  dnsServer.processNextRequest();
  
  // Handle web server requests
  server.handleClient();
  
  uint32_t now = millis();

  // ── SENSORS (Every 3 seconds) ─────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    readLocalSensors();
    computeRisk();
    updateOutputs();
    logToFile();

    // Send alert level to Arduino Slave
    char cmdBuf[16];
    snprintf(cmdBuf, sizeof(cmdBuf), "CMD:%d\n", data.alertLevel);
    Serial2.print(cmdBuf);
  }

  // ── SERIAL FROM SLAVE (With iteration limit to prevent starvation) ─
  int serialCount = 0;
  while (Serial2.available() && serialCount++ < MAX_SERIAL_ITERATIONS) {
    handleSlaveSerial();
  }

  // ── DISPLAY (Every 500ms) ─────────────────────────────────
  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    handleJoystick();
    updateOLED();
    updateLCD();
  }

  // ── DOT MATRIX (Every 100ms) ──────────────────────────────
  if (now - lastScrollMs >= SCROLL_INTERVAL_MS) {
    lastScrollMs = now;
    dotMatrix.transform(MD_MAX72XX::TSL);
    if ((now / 800) % 2 == 0) {
      uint8_t col = (data.alertLevel == 0) ? 0x18 : (data.alertLevel == 1) ? 0x3C : 0xFF;
      dotMatrix.setColumn(0, 0, col);
    }
  }

  // ── ACKNOWLEDGE (Touch sensor override) ───────────────────
  if (digitalRead(PIN_TOUCH) == HIGH) {
    digitalWrite(PIN_RELAY, HIGH);
    warningServo.write(0);
  }
}

// ── SENSOR READING FUNCTIONS ────────────────────────────────
void readLocalSensors() {
  if (dataMutex == NULL) {
    Serial.println("Data mutex not initialized in readLocalSensors");
    return;
  }
  
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("Failed to acquire data mutex in readLocalSensors");
    return;
  }

  // DHT11 with validation
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && t >= DHT_TEMP_MIN && t <= DHT_TEMP_MAX) {
    data.airTemp = t;
  } else {
    Serial.println("DHT11 temperature reading invalid");
  }
  if (!isnan(h) && h >= DHT_HUMIDITY_MIN && h <= DHT_HUMIDITY_MAX) {
    data.airHumidity = h;
  }

  // Soil Moisture with proper constrain/map order
  int rawSoil = analogRead(PIN_SOIL);
  int constrainedSoil = constrain(rawSoil, SOIL_SENSOR_MIN_ADC, SOIL_SENSOR_MAX_ADC);
  data.soilMoisture = (float)map(constrainedSoil, SOIL_SENSOR_MAX_ADC, SOIL_SENSOR_MIN_ADC, 0, 100);

  // HC-SR04 with validation (Note: pulseIn is blocking - consider NewPing library for production)
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 25000);

  float rawDist;
  if (duration == 0 || duration > 25000) {
    rawDist = 400.0;  // Out of range
  } else if (duration < 150) {  // < ~2.5cm is physically impossible
    rawDist = 0.0;
  } else {
    rawDist = duration * 0.0343f / 2.0f;
  }
  data.distanceCm = kalmanUpdate(&kf_dist, rawDist);

  // PIR and Obstacle
  data.pirMotion = (digitalRead(PIN_PIR) == HIGH);
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);

  xSemaphoreGive(dataMutex);
}

// ── SERIAL HANDLING FROM ARDUINO SLAVE ──────────────────────
void handleSlaveSerial() {
  char c = Serial2.read();
  
  if (c == '\n' || c == '\r') {
    slaveBuffer[slaveIdx] = '\0';
    if (slaveIdx > 0) {
      parseSlaveData(slaveBuffer, slaveIdx);
    }
    slaveIdx = 0;
  } else {
    if (slaveIdx < SLAVE_BUF_SIZE - 1) {
      slaveBuffer[slaveIdx++] = c;
      slaveBuffer[slaveIdx] = '\0';  // Maintain null termination
    } else {
      // Buffer overflow - discard and reset
      slaveIdx = 0;
      slaveBuffer[0] = '\0';
    }
  }
}

// ── PARSE SLAVE DATA WITH CHECKSUM VALIDATION ────────────────
void parseSlaveData(char* line, size_t len) {
  // Format: DATA*CHECKSUM (HEX)
  char* starPtr = strchr(line, '*');
  if (!starPtr) return;

  *starPtr = '\0';
  char* dataPart = line;
  char* checkPart = starPtr + 1;

  // Verify XOR Checksum
  uint8_t calculated = 0;
  for (int i = 0; dataPart[i] != '\0'; i++) calculated ^= (uint8_t)dataPart[i];
  uint8_t received = (uint8_t)strtol(checkPart, NULL, 16);

  if (calculated != received) {
    Serial.printf("Serial Checksum Fail: %02X != %02X\n", calculated, received);
    return;
  }

  if (dataMutex == NULL || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  // Water Level (W:)
  char* wPtr = strstr(dataPart, "W:");
  if (wPtr != nullptr) {
    char* endPtr = nullptr;
    float waterVal = strtof(wPtr + 2, &endPtr);
    if (endPtr != nullptr && waterVal >= 0.0f && waterVal <= 100.0f) {
      data.waterLevel = waterVal;
    }
  }

  // Thermistor Temp (T:)
  char* tPtr = strstr(dataPart, "T:");
  if (tPtr != nullptr) {
    char* endPtr = nullptr;
    float tempVal = strtof(tPtr + 2, &endPtr);
    if (endPtr != nullptr && tempVal >= -40.0f && tempVal <= 125.0f) {
      data.thermistorTemp = tempVal;
    }
  }

  // Tilt Angle (S:) - Apply Kalman Filter
  char* sPtr = strstr(dataPart, "S:");
  if (sPtr != nullptr) {
    char* endPtr = nullptr;
    float rawTilt = strtof(sPtr + 2, &endPtr);
    if (endPtr != nullptr && rawTilt >= 0.0f && rawTilt <= 100.0f) {
      data.tiltAngle = kalmanUpdate(&kf_tilt, rawTilt);
    }
  }

  // Potentiometer Calibration (P:)
  char* pPtr = strstr(dataPart, "P:");
  if (pPtr != nullptr) {
    char* endPtr = nullptr;
    float potVal = strtof(pPtr + 2, &endPtr);
    if (endPtr != nullptr && potVal >= 0.1f && potVal <= 10.0f) {
      data.potCalibration = potVal;
    }
  }

  // Tilt Interrupt (I:)
  char* iPtr = strstr(dataPart, "I:");
  if (iPtr != nullptr) {
    char* endPtr = nullptr;
    long tiltVal = strtol(iPtr + 2, &endPtr, 10);
    if (endPtr != nullptr && (tiltVal == 0 || tiltVal == 1)) {
      data.tiltInterrupt = (tiltVal == 1);
    }
  }

  xSemaphoreGive(dataMutex);
}

// ── RISK COMPUTATION ────────────────────────────────────────
void computeRisk() {
  if (dataMutex == NULL || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("Failed to acquire data mutex in computeRisk");
    return;
  }

  // Landslide interrupt = immediate critical alert
  if (data.tiltInterrupt) {
    data.riskIndex = 9.0;
    data.hazardType = "LANDSLIDE";
    data.alertLevel = 2;
    xSemaphoreGive(dataMutex);
    return;
  }

  // EMA Smoothing
  ema_soil = (EMA_A * data.soilMoisture) + ((1.0f - EMA_A) * ema_soil);
  
  float floodPct = (float)map((int)data.distanceCm, FLOOD_DISTANCE_MAX_CM, FLOOD_DISTANCE_MIN_CM, 0, 100);
  floodPct = constrain(floodPct, 0, 100);
  ema_water = (EMA_A * max(floodPct, data.waterLevel)) + ((1.0f - EMA_A) * ema_water);

  // Weighted risk calculation
  float weighted = (ema_soil * WEIGHT_SOIL) + (ema_water * WEIGHT_WATER) + (data.tiltAngle * WEIGHT_TILT);
  
  // Add bonuses for motion and obstacles
  if (data.pirMotion) weighted += BONUS_PIR_MOTION;
  if (data.obstacleDetected) weighted += BONUS_OBSTACLE;

  // Apply calibration and constrain
  data.potCalibration = constrain(data.potCalibration, 0.1f, 10.0f);
  weighted = constrain(weighted * data.potCalibration, 0, 100);
  
  // Scale to 0-9 risk index
  data.riskIndex = (weighted / 100.0f) * 9.0f;

  // Determine hazard type
  if (ema_water > 60) {
    data.hazardType = "FLOOD";
  } else if (ema_soil > 70) {
    data.hazardType = "DROUGHT";
  } else {
    data.hazardType = "NOMINAL";
  }

  // Determine alert level
  if (data.riskIndex > 6) {
    data.alertLevel = 2;
  } else if (data.riskIndex > 3) {
    data.alertLevel = 1;
  } else {
    data.alertLevel = 0;
  }

  // Update history with proper bounds checking
  riskHistory[historyIdx] = data.riskIndex;
  historyIdx = (historyIdx + 1) % HISTORY_LEN;

  xSemaphoreGive(dataMutex);
}

// ── OUTPUT CONTROL ──────────────────────────────────────────
void updateOutputs() {
  if (dataMutex == NULL || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  // LED Indicators
  digitalWrite(LED_GREEN, data.alertLevel == 0);
  digitalWrite(LED_YELLOW, data.alertLevel == 1);
  digitalWrite(LED_BLUE, (data.hazardType != nullptr && data.hazardType[0] == 'F'));
  digitalWrite(LED_RED, data.alertLevel == 2);
  
  // Relay (active LOW)
  digitalWrite(PIN_RELAY, (data.alertLevel == 2) ? LOW : HIGH);

  // Servo position with validation
  int pos = (data.alertLevel == 2) ? 180 : (data.alertLevel == 1) ? 90 : 0;
  pos = constrain(pos, 0, 180);
  warningServo.write(pos);

  xSemaphoreGive(dataMutex);
}

// ── JOYSTICK HANDLING ───────────────────────────────────────
void handleJoystick() {
  int x = analogRead(PIN_JOY_X);
  if (x < 1000) {
    menuPage = (menuPage + 1) % MENU_PAGES;
    delay(50);  // Debounce
  } else if (x > 3000) {
    menuPage = (menuPage - 1 + MENU_PAGES) % MENU_PAGES;
    delay(50);  // Debounce
  }
}

// ── OLED DISPLAY UPDATE ─────────────────────────────────────
void updateOLED() {
  char buf[64];

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);

  snprintf(buf, sizeof(buf), "PAGE %d: %s", menuPage, data.hazardType);
  oled.print(buf);

  if (menuPage == 0) {
    oled.setTextSize(3);
    oled.setCursor(0, 20);
    snprintf(buf, sizeof(buf), "%.1f/9", data.riskIndex);
    oled.print(buf);
    oled.drawRect(0, 50, 128, 10, 1);
    oled.fillRect(0, 50, (int)(data.riskIndex * 14.2), 10, 1);
  } else {
    oled.setCursor(0, 15);
    snprintf(buf, sizeof(buf), "Soil: %.1f%%", data.soilMoisture);
    oled.println(buf);
    snprintf(buf, sizeof(buf), "Water: %.1f%%", data.waterLevel);
    oled.println(buf);
    snprintf(buf, sizeof(buf), "Temp: %.1fC", data.airTemp);
    oled.println(buf);
    snprintf(buf, sizeof(buf), "Tilt: %.1f%%", data.tiltAngle);
    oled.print(buf);
  }
  oled.display();
}

// ── LCD DISPLAY UPDATE ──────────────────────────────────────
void updateLCD() {
  char buf[32];

  lcd.setCursor(0, 0);
  snprintf(buf, sizeof(buf), "RISK:%d/9 %s    ", (int)data.riskIndex, data.hazardType);
  lcd.print(buf);

  lcd.setCursor(0, 1);
  snprintf(buf, sizeof(buf), "W:%.0f%% S:%.0f%% T:%.0f ", data.waterLevel, data.soilMoisture, data.airTemp);
  lcd.print(buf);
}

// ── WEB API: SENSOR DATA ────────────────────────────────────
void handleDataRequest() {
  // Authentication check
  if (!server.authenticate(WEB_USERNAME, WEB_PASSWORD)) {
    server.requestAuthentication();
    return;
  }

  if (dataMutex == NULL) {
    server.send(503, "application/json", "{\"error\":\"Data mutex not initialized\"}");
    return;
  }
  
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    server.send(503, "application/json", "{\"error\":\"Service unavailable\"}");
    return;
  }

  // Build JSON response with proper buffer size (512 bytes minimum)
  char buf[512];
  const char* hazard = (data.hazardType != nullptr) ? data.hazardType : "UNKNOWN";
  
  int written = snprintf(buf, sizeof(buf),
    "{\"risk\":%.2f,\"hazard\":\"%s\",\"dist\":%.1f,\"water\":%.1f,\"soil\":%.1f,"
    "\"airT\":%.1f,\"airH\":%.1f,\"soilT\":%.1f,\"tilt\":%.1f,\"pot\":%.1f,"
    "\"pir\":%s,\"obs\":%s,\"lsi\":%s,\"relay\":%s,\"servoPos\":%d}",
    data.riskIndex,
    hazard,
    data.distanceCm, 
    data.waterLevel, 
    data.soilMoisture, 
    data.airTemp, 
    data.airHumidity,
    data.thermistorTemp, 
    data.tiltAngle, 
    data.potCalibration,
    data.pirMotion ? "true" : "false",
    data.obstacleDetected ? "true" : "false",
    data.tiltInterrupt ? "true" : "false",
    (digitalRead(PIN_RELAY) == LOW) ? "true" : "false",
    warningServo.read());

  xSemaphoreGive(dataMutex);
  
  if (written > 0 && written < (int)sizeof(buf)) {
    server.send(200, "application/json", buf);
  } else {
    server.send(500, "application/json", "{\"error\":\"JSON formatting failed\"}");
  }
}

// ── WEB API: DOWNLOAD LOGS ──────────────────────────────────
void handleDownloadRequest() {
  // Authentication check
  if (!server.authenticate(WEB_USERNAME, WEB_PASSWORD)) {
    server.requestAuthentication();
    return;
  }

  if (!LittleFS.exists("/geosense_logs.csv")) {
    server.send(404, "text/plain", "Log file not found");
    return;
  }

  File logFile = LittleFS.open("/geosense_logs.csv", "r");
  if (!logFile) {
    server.send(500, "text/plain", "Failed to open log file");
    return;
  }

  server.streamFile(logFile, "text/csv");
  logFile.close();
}

// ── FILE LOGGING (Buffered to save Flash wear) ─────────────
#define LOG_BUFFER_MAX 10
String ramLogBuffer[LOG_BUFFER_MAX];
int logIdx = 0;

void logToFile() {
  if (dataMutex == NULL || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }
  
  char line[96];
  const char* hazard = (data.hazardType != nullptr) ? data.hazardType : "UNKNOWN";
  snprintf(line, sizeof(line), "%lu,%.2f,%s,%.1f,%.1f,%.1f,%.1f\n",
           millis() / 1000UL, data.riskIndex, hazard,
           data.waterLevel, data.soilMoisture, data.airTemp, data.tiltAngle);
  
  xSemaphoreGive(dataMutex);

  // Add to RAM buffer
  ramLogBuffer[logIdx++] = String(line);

  // Periodic write or Critical alert write
  if (logIdx >= LOG_BUFFER_MAX || data.alertLevel == 2) {
    File logFile = LittleFS.open("/geosense_logs.csv", "a");
    if (logFile) {
      for (int i = 0; i < logIdx; i++) {
        logFile.print(ramLogBuffer[i]);
      }
      logFile.close();
      logIdx = 0;
    }
  }
}

// ── BOOT SEQUENCE ───────────────────────────────────────────
void bootSequence() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_GREEN, HIGH);
    delay(50);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    delay(50);
    digitalWrite(LED_BLUE, LOW);
  }
}
