// ================================================================
//  GEO-SENSE AFRICA v2.0 — ESP32 MASTER NODE (OPTIMIZED)
//  Role: Central Hub, WiFi Dashboard, OLED Display, Data Fusion
// ================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <cstdio>

// ── WIFI CONFIGURATION ──────────────────────────────────────
const char* AP_SSID     = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";

// ── PIN DEFINITIONS (ESP32) ─────────────────────────────────
// I2C Bus
#define PIN_SDA         21
#define PIN_SCL         22

// DHT11 Sensor
#define PIN_DHT         4
#define DHT_TYPE        DHT11

// HC-SR04 Ultrasonic (Flood Distance)
#define PIN_TRIG        5
#define PIN_ECHO        18   // Note: Requires voltage divider (5V -> 3.3V)

// PIR Motion Sensor
#define PIN_PIR         19

// Servo & Relay
#define PIN_SERVO       13
#define PIN_RELAY       12

// UI Controls
#define PIN_TOUCH       14
#define PIN_JOY_X       34   // ADC1 only
#define PIN_JOY_Y       35   // ADC1 only
#define PIN_JOY_BTN     32

// Dot Matrix 8x8 (SPI)
#define PIN_DM_DATA     23   // MOSI
#define PIN_DM_CLK      26   // SCK (MOVED from GPIO18 to avoid conflict with ECHO)
#define PIN_DM_CS       15   // SS

// Status LEDs
#define LED_GREEN       25
#define LED_BLUE        2    // Moved to avoid conflict with DM_CLK (GPIO26)
#define LED_YELLOW      27
#define LED_RED         33

// Analog Sensors
#define PIN_SOIL        36   // ADC1 only
#define PIN_OBSTACLE    39   // ADC1 only

// Serial2 to Arduino Slave
#define SLAVE_RX        16
#define SLAVE_TX        17

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

// EMA smoothing buffers
float ema_soil = 0, ema_water = 0, ema_tilt = 0, ema_temp = 0, ema_dist = 0;
#define EMA_A 0.25f

uint32_t lastSampleMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastScrollMs = 0;
#define SAMPLE_INTERVAL  3000
#define DISPLAY_INTERVAL 500

#define HISTORY_LEN  50
float riskHistory[HISTORY_LEN];
int   historyIdx = 0;

int menuPage = 0;
#define MENU_PAGES 4

// ── WEB DASHBOARD HTML ───────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawHTML(
<!DOCTYPE html><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
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
    </div>
  </div>

  <footer>
    &copy; 2024 Geo-Sense Africa v2.1 &bull; System Uptime: <span id="uptime">0s</span><br>
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
setInterval(()=>{ up++; document.getElementById('uptime').textContent = up + 's'; }, 1000);
</script>
</body></html>
)rawHTML";

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  Wire.begin(PIN_SDA, PIN_SCL);

  // Initialize all LED pins first for boot sequence
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  // Boot sequence (non-blocking visual feedback)
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_GREEN, HIGH);
    delay(50);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    delay(50);
    digitalWrite(LED_BLUE, LOW);
  }

  // Initialize OLED Display
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found. Check wiring and address (0x3C).");
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("GEO-SENSE AFRICA");
    oled.println("v2.0 BOOTING...");
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

  // Start WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup Web Server
  server.on("/", []() { server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleDataRequest);
  server.begin();

  // Initialize Risk History
  for (int i = 0; i < HISTORY_LEN; i++) riskHistory[i] = 0;

  Serial.println("Geo-Sense Africa v2.0 - Master Node Ready");
}

void loop() {
  server.handleClient();
  uint32_t now = millis();

  // ── SENSORS ──────────────────────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readLocalSensors();
    computeRisk();
    updateOutputs();
    
    // Send alert level to Arduino Slave
    char cmdBuf[16];
    snprintf(cmdBuf, sizeof(cmdBuf), "CMD:%d\n", data.alertLevel);
    Serial2.print(cmdBuf);
  }

  // ── SERIAL FROM SLAVE ────────────────────────────────────
  while (Serial2.available()) {
    handleSlaveSerial();
  }

  // ── DISPLAY ──────────────────────────────────────────────
  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    handleJoystick();
    updateOLED();
    updateLCD();
  }

  // ── DOT MATRIX ───────────────────────────────────────────
  if (now - lastScrollMs >= 100) {
    lastScrollMs = now;
    dotMatrix.transform(MD_MAX72XX::TSL);
    if ((now / 800) % 2 == 0) {
      uint8_t col = (data.alertLevel == 0) ? 0x18 : (data.alertLevel == 1) ? 0x3C : 0xFF;
      dotMatrix.setColumn(0, 0, col);
    }
  }

  // ── ACKNOWLEDGE ──────────────────────────────────────────
  if (digitalRead(PIN_TOUCH) == HIGH) {
    digitalWrite(PIN_RELAY, HIGH);
    warningServo.write(0);
  }
}

void readLocalSensors() {
  // DHT
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) data.airTemp = t;
  if (!isnan(h)) data.airHumidity = h;

  // Soil
  int rawSoil = analogRead(PIN_SOIL);
  data.soilMoisture = constrain(map(rawSoil, 4095, 1200, 0, 100), 0, 100);

  // HC-SR04
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 25000);
  data.distanceCm = (duration == 0) ? 400.0 : (duration * 0.0343 / 2.0);

  data.pirMotion = digitalRead(PIN_PIR);
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);
}

// ── SERIAL HANDLING FROM ARDUINO SLAVE ─────────────────────
#define SLAVE_BUF_SIZE 64
char slaveBuffer[SLAVE_BUF_SIZE];
uint8_t slaveIdx = 0;

void handleSlaveSerial() {
  char c = Serial2.read();
  
  if (c == '\n' || c == '\r') {
    slaveBuffer[slaveIdx] = '\0';
    parseSlaveData(slaveBuffer);
    slaveIdx = 0;
  } else {
    if (slaveIdx < SLAVE_BUF_SIZE - 1) {
      slaveBuffer[slaveIdx++] = c;
    }
  }
}

void parseSlaveData(char* line) {
  // Parse format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
  
  // Water Level (W:)
  char* wPtr = strstr(line, "W:");
  if (wPtr) data.waterLevel = atof(wPtr + 2);
  
  // Thermistor Temp (T:)
  char* tPtr = strstr(line, "T:");
  if (tPtr) data.thermistorTemp = atof(tPtr + 2);
  
  // Tilt Angle (S:) - currently fixed at 0.0 from slave
  char* sPtr = strstr(line, "S:");
  if (sPtr) data.tiltAngle = atof(sPtr + 2);
  
  // Potentiometer Calibration (P:)
  char* pPtr = strstr(line, "P:");
  if (pPtr) data.potCalibration = atof(pPtr + 2);
  
  // Tilt Interrupt (I:)
  char* iPtr = strstr(line, "I:");
  if (iPtr) data.tiltInterrupt = (atoi(iPtr + 2) == 1);
}

void computeRisk() {
  if (data.tiltInterrupt) {
    data.riskIndex = 9.0;
    data.hazardType = "LANDSLIDE";
    data.alertLevel = 2;
    return;
  }

  // Smoothing
  ema_soil  = (EMA_A * data.soilMoisture) + ((1.0f - EMA_A) * ema_soil);
  float floodPct = constrain(map((int)data.distanceCm, 200, 20, 0, 100), 0, 100);
  ema_water = (EMA_A * max(floodPct, data.waterLevel)) + ((1.0f - EMA_A) * ema_water);
  
  float weighted = (ema_soil * 0.3f) + (ema_water * 0.4f) + (data.tiltAngle * 0.3f);
  if (data.pirMotion) weighted += 10.0f;
  if (data.obstacleDetected) weighted += 8.0f;
  
  weighted = constrain(weighted * data.potCalibration, 0, 100);
  data.riskIndex = (weighted / 100.0f) * 9.0f;

  if (ema_water > 60) data.hazardType = "FLOOD";
  else if (ema_soil > 70) data.hazardType = "DROUGHT";
  else data.hazardType = "NOMINAL";

  data.alertLevel = (data.riskIndex > 6) ? 2 : (data.riskIndex > 3) ? 1 : 0;
  
  riskHistory[historyIdx % HISTORY_LEN] = data.riskIndex;
  historyIdx++;
}

void updateOutputs() {
  digitalWrite(LED_GREEN,  data.alertLevel == 0);
  digitalWrite(LED_YELLOW, data.alertLevel == 1);
  digitalWrite(LED_BLUE,   data.hazardType[0] == 'F');
  digitalWrite(LED_RED,    data.alertLevel == 2);
  digitalWrite(PIN_RELAY, (data.alertLevel == 2) ? LOW : HIGH);

  int pos = (data.alertLevel == 2) ? 180 : (data.alertLevel == 1) ? 90 : 0;
  warningServo.write(pos);
}

void handleJoystick() {
  int x = analogRead(PIN_JOY_X);
  if (x < 1000) menuPage = (menuPage + 1) % MENU_PAGES;
  else if (x > 3000) menuPage = (menuPage - 1 + MENU_PAGES) % MENU_PAGES;
}

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

void updateLCD() {
  char buf[32];
  
  lcd.setCursor(0, 0);
  snprintf(buf, sizeof(buf), "RISK:%d/9 %s    ", (int)data.riskIndex, data.hazardType);
  lcd.print(buf);
  
  lcd.setCursor(0, 1);
  snprintf(buf, sizeof(buf), "W:%.0f%% S:%.0f%% T:%.0f ", data.waterLevel, data.soilMoisture, data.airTemp);
  lcd.print(buf);
}

void handleDataRequest() {
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"risk\":%.2f,\"hazard\":\"%s\",\"dist\":%.1f,\"water\":%.1f,\"soil\":%.1f,\"airT\":%.1f,\"airH\":%.1f,\"soilT\":%.1f,\"tilt\":%.1f,\"pot\":%.1f,\"pir\":%s,\"obs\":%s,\"lsi\":%s,\"relay\":%s,\"servoPos\":%d}",
    data.riskIndex, data.hazardType, data.distanceCm, data.waterLevel, data.soilMoisture, data.airTemp, data.airHumidity, data.thermistorTemp, data.tiltAngle, data.potCalibration,
    data.pirMotion?"true":"false", data.obstacleDetected?"true":"false", data.tiltInterrupt?"true":"false", (digitalRead(PIN_RELAY)==LOW)?"true":"false", warningServo.read());
  server.send(200, "application/json", buf);
}
