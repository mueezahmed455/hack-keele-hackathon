// ================================================================
//  GEO-SENSE AFRICA v2.0 — ESP32 MASTER NODE
//  Role: Central Hub, WiFi Dashboard, OLED Display, Data Fusion
//
//  NEW HARDWARE (Starter Kit + ESP32):
//  - ESP32 DevKit         → Master controller + WiFi AP
//  - IIC LCD 1602         → Human-readable status display
//  - 0.96" OLED (I2C)    → Live risk gauge + waveform
//  - HC-SR04 Ultrasound   → Riverbank/flood distance sensing
//  - HC-SR501 PIR Motion  → Community evacuation detection
//  - SG90 Servo           → Physical flood gate / warning flag
//  - 1-Way Relay Module   → External siren / flood lamp power
//  - 8x8 Dot Matrix       → Scrolling alert messages
//  - TTP223B Touch        → Manual alarm reset / acknowledge
//  - Joystick Module      → Menu navigation for LCD
//  - DHT11                → Temperature & Humidity
//  - Soil Humidity        → Moisture / drought sensing
//  - Obstacle Avoidance   → Flood debris detection
//  - Green/Blue/Yellow/Red LEDs → Hazard-specific indicators
//
//  ARCHITECTURE:
//  ESP32 ←→ Arduino Uno (Serial2) for sensor fusion data
//  ESP32 hosts WiFi Access Point → Live web dashboard
//  ESP32 drives all display/output hardware
// ================================================================

// ── LIBRARIES ───────────────────────────────────────────────
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <MD_MAX72xx.h>        // 8x8 Dot Matrix
#include <SPI.h>

// ── INSTALL THESE LIBRARIES IN ARDUINO IDE ──────────────────
// 1. Adafruit SSD1306          (OLED)
// 2. Adafruit GFX Library      (Graphics)
// 3. LiquidCrystal I2C         (LCD)
// 4. DHT sensor library        (Adafruit)
// 5. ESP32Servo                (Servo on ESP32)
// 6. MD_MAX72XX                (Dot Matrix)

// ── WIFI CONFIGURATION ──────────────────────────────────────
const char* AP_SSID     = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";
// Connect phone/laptop to this WiFi, then visit: http://192.168.4.1

// ── PIN DEFINITIONS (ESP32) ─────────────────────────────────

// I2C Bus (shared: OLED + LCD)
#define PIN_SDA         21
#define PIN_SCL         22

// DHT11
#define PIN_DHT         4
#define DHT_TYPE        DHT11

// HC-SR04 Ultrasound (Flood/River distance)
#define PIN_TRIG        5
#define PIN_ECHO        18

// PIR Motion Sensor
#define PIN_PIR         19

// SG90 Servo (Warning flag / flood gate actuator)
#define PIN_SERVO       13

// 1-Way Relay (external siren / floodlight)
#define PIN_RELAY       12   // LOW = relay ON (active low)

// TTP223B Touch Sensor (manual alert acknowledge)
#define PIN_TOUCH       14

// Joystick (menu navigation)
#define PIN_JOY_X       34   // ADC only pin
#define PIN_JOY_Y       35   // ADC only pin
#define PIN_JOY_BTN     32

// Dot Matrix (SPI)
#define PIN_DM_DATA     23   // MOSI
#define PIN_DM_CLK      18   // SCK  — NOTE: shared with ECHO? Use 26 if conflict
#define PIN_DM_CS       15

// Indicator LEDs (hazard-specific)
#define LED_GREEN       25   // All clear
#define LED_BLUE        26   // Flood warning
#define LED_YELLOW      27   // Drought warning
#define LED_RED         33   // Landslide / critical

// Soil Humidity
#define PIN_SOIL        36   // ADC1 (input only)

// Obstacle Avoidance
#define PIN_OBSTACLE    39   // ADC1 (input only)

// Serial to Arduino Slave
#define SLAVE_RX        16   // ESP32 RX2 → Arduino TX
#define SLAVE_TX        17   // ESP32 TX2 → Arduino RX

// ── DISPLAY OBJECTS ─────────────────────────────────────────
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C
#define LCD_ADDR        0x27
#define LCD_COLS        16
#define LCD_ROWS        2

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Dot Matrix setup
#define MAX_DEVICES     1
MD_MAX72XX dotMatrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, PIN_DM_DATA, PIN_DM_CLK, PIN_DM_CS, MAX_DEVICES);

// ── OBJECTS ──────────────────────────────────────────────────
DHT dht(PIN_DHT, DHT_TYPE);
Servo warningServo;
WebServer server(80);

// ── DATA STRUCTURES ─────────────────────────────────────────
struct SensorData {
  // From ESP32 directly
  float soilMoisture;      // 0–100%
  float airTemp;           // °C
  float airHumidity;       // %
  float distanceCm;        // Ultrasound → flood/river level
  bool  pirMotion;         // Evacuation movement detected
  bool  obstacleDetected;  // Debris in water
  bool  touchPressed;      // Manual acknowledge

  // From Arduino slave (via Serial2)
  float waterLevel;        // 0–100 (water level sensor)
  float tiltAngle;         // 0–100 (tilt/slope)
  float thermistorTemp;    // Soil temperature °C
  float potCalibration;    // Sensitivity 0.5–2.0x
  bool  tiltInterrupt;     // Hardware landslide interrupt

  // Computed
  float riskIndex;         // 0–9
  float evapotranspiration;
  String hazardType;       // "FLOOD", "DROUGHT", "LANDSLIDE", "NOMINAL"
  int   alertLevel;        // 0=green,1=amber,2=red,3=CRITICAL
};

SensorData data;

// EMA smoothing buffers
float ema_soil   = 0, ema_water = 0, ema_tilt = 0, ema_temp = 0, ema_dist = 0;
#define EMA_A 0.25f

// Alert hysteresis
int alertCount = 0;
bool alertAcknowledged = false;
unsigned long lastSampleMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastScrollMs = 0;
#define SAMPLE_INTERVAL  3000
#define DISPLAY_INTERVAL 500

// History buffer for OLED waveform
#define HISTORY_LEN  50
float riskHistory[HISTORY_LEN];
int   historyIdx = 0;

// Joystick menu
int menuPage = 0;
#define MENU_PAGES 4

// Dot matrix scroll
String scrollMsg = "GEO-SENSE AFRICA ONLINE";
int scrollPos = 0;

// ── WEB DASHBOARD HTML ───────────────────────────────────────
// Stored in PROGMEM to save RAM
const char DASHBOARD_HTML[] PROGMEM = R"rawHTML(
<!DOCTYPE html><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="4">
<title>GEO-SENSE Africa</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0e14;color:#e8eaed;font-family:'Courier New',monospace;padding:12px}
h1{font-size:20px;color:#00ff88;letter-spacing:.15em;margin-bottom:4px}
.sub{font-size:11px;color:#6b7a93;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin-bottom:12px}
.card{background:#111720;border:1px solid #1e2d45;border-radius:8px;padding:14px}
.label{font-size:9px;letter-spacing:.2em;color:#6b7a93;margin-bottom:6px}
.val{font-size:26px;font-weight:700;line-height:1}
.unit{font-size:12px;color:#6b7a93;margin-left:3px}
.risk-bar{height:12px;background:#1e2d45;border-radius:6px;margin-top:8px;overflow:hidden}
.risk-fill{height:100%;border-radius:6px;transition:width .5s}
.badge{display:inline-block;padding:4px 12px;border-radius:4px;font-size:11px;letter-spacing:.1em;margin-top:8px}
.green{color:#5dd882}.blue{color:#5bb8ff}.amber{color:#f5a623}.red{color:#ff6b6b}
.bg-green{background:rgba(93,216,130,.15);border:1px solid rgba(93,216,130,.3);color:#5dd882}
.bg-amber{background:rgba(245,166,35,.15);border:1px solid rgba(245,166,35,.3);color:#f5a623}
.bg-red{background:rgba(232,69,69,.15);border:1px solid rgba(232,69,69,.4);color:#ff6b6b}
.full{grid-column:1/-1}
.row{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid #1e2d45;font-size:12px}
.row:last-child{border:none}
footer{text-align:center;font-size:10px;color:#6b7a93;margin-top:12px}
</style>
</head>
<body>
<h1>&#9632; GEO-SENSE AFRICA</h1>
<div class="sub">MULTI-HAZARD EARLY WARNING SYSTEM &bull; AUTO-REFRESH 4s</div>
<div class="grid">
  <div class="card">
    <div class="label">RISK INDEX</div>
    <div class="val" id="ri">--<span class="unit">/9</span></div>
    <div class="risk-bar"><div class="risk-fill" id="rf"></div></div>
    <div id="badge" class="badge">LOADING...</div>
  </div>
  <div class="card">
    <div class="label">HAZARD TYPE</div>
    <div class="val" id="ht" style="font-size:16px;margin-top:4px">--</div>
    <div class="row" style="margin-top:8px"><span>PIR Motion</span><span id="pir">--</span></div>
    <div class="row"><span>Debris</span><span id="obs">--</span></div>
  </div>
  <div class="card">
    <div class="label">FLOOD SENSOR (Ultrasound)</div>
    <div class="val" id="dist">--<span class="unit">cm</span></div>
    <div class="row" style="margin-top:8px"><span>Water Level</span><span id="wl" class="blue">--%</span></div>
    <div class="row"><span>Obstacle</span><span id="ob2">--</span></div>
  </div>
  <div class="card">
    <div class="label">DROUGHT (Soil)</div>
    <div class="val" id="sm">--<span class="unit">%</span></div>
    <div class="row" style="margin-top:8px"><span>Air Temp</span><span id="at" class="amber">--°C</span></div>
    <div class="row"><span>Humidity</span><span id="ah">--%</span></div>
  </div>
  <div class="card full">
    <div class="label">ALL SENSOR READINGS</div>
    <div class="row"><span>Soil Temperature</span><span id="st" class="amber">--°C</span></div>
    <div class="row"><span>Evapotranspiration Index</span><span id="et" class="amber">--</span></div>
    <div class="row"><span>Slope / Tilt</span><span id="tilt" class="red">--%</span></div>
    <div class="row"><span>Sensitivity (Pot Cal.)</span><span id="pot">--x</span></div>
    <div class="row"><span>Landslide Interrupt</span><span id="lsi">--</span></div>
    <div class="row"><span>Relay (Siren)</span><span id="relay">--</span></div>
    <div class="row"><span>Servo Position</span><span id="servo">--°</span></div>
  </div>
</div>
<footer>GEO-SENSE AFRICA v2.0 &bull; VISIT 192.168.4.1 &bull; WIFI: GeoSense-Africa</footer>
<script>
fetch('/data').then(r=>r.json()).then(d=>{
  const ri=Math.round(d.risk);
  document.getElementById('ri').innerHTML=ri+'<span class="unit">/9</span>';
  const pct=(ri/9)*100;
  const col=ri<=3?'#5dd882':ri<=6?'#f5a623':'#ff6b6b';
  document.getElementById('rf').style.cssText='width:'+pct+'%;background:'+col;
  const badges=['bg-green','bg-amber','bg-red'];
  const bi=ri<=3?0:ri<=6?1:2;
  const bl=['NOMINAL','WARNING','CRITICAL'];
  document.getElementById('badge').className='badge '+badges[bi];
  document.getElementById('badge').textContent=bl[bi];
  document.getElementById('ht').textContent=d.hazard;
  document.getElementById('ht').className='val '+(d.hazard=='FLOOD'?'blue':d.hazard=='DROUGHT'?'amber':d.hazard=='LANDSLIDE'?'red':'green');
  document.getElementById('dist').innerHTML=d.dist.toFixed(1)+'<span class="unit">cm</span>';
  document.getElementById('wl').textContent=d.water.toFixed(1)+'%';
  document.getElementById('sm').innerHTML=d.soil.toFixed(1)+'<span class="unit">%</span>';
  document.getElementById('at').textContent=d.airT.toFixed(1)+'°C';
  document.getElementById('ah').textContent=d.airH.toFixed(1)+'%';
  document.getElementById('st').textContent=d.soilT.toFixed(1)+'°C';
  document.getElementById('et').textContent=d.et.toFixed(2);
  document.getElementById('tilt').textContent=d.tilt.toFixed(1)+'%';
  document.getElementById('pot').textContent=d.pot.toFixed(2)+'x';
  document.getElementById('pir').textContent=d.pir?'DETECTED':'Clear';
  document.getElementById('pir').className=d.pir?'red':'green';
  document.getElementById('obs').textContent=d.obs?'DEBRIS':'Clear';
  document.getElementById('obs').className=d.obs?'amber':'green';
  document.getElementById('ob2').textContent=d.obs?'DEBRIS':'Clear';
  document.getElementById('lsi').textContent=d.lsi?'!! TRIGGERED !!':'OK';
  document.getElementById('lsi').className=d.lsi?'red':'green';
  document.getElementById('relay').textContent=d.relay?'ACTIVE':'Off';
  document.getElementById('servo').textContent=d.servoPos+'°';
}).catch(()=>{});
</script>
</body></html>
)rawHTML";

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);

  // ── I2C
  Wire.begin(PIN_SDA, PIN_SCL);

  // ── OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("GEO-SENSE AFRICA");
  oled.println("v2.0 BOOTING...");
  oled.display();

  // ── LCD
  lcd.init();
  lcd.backlight();
  lcd.print("GEO-SENSE AFRICA");
  lcd.setCursor(0, 1);
  lcd.print("  MHEWS  v2.0   ");

  // ── DHT
  dht.begin();

  // ── Pins
  pinMode(PIN_PIR,      INPUT);
  pinMode(PIN_TOUCH,    INPUT);
  pinMode(PIN_JOY_BTN,  INPUT_PULLUP);
  pinMode(PIN_RELAY,    OUTPUT);
  digitalWrite(PIN_RELAY, HIGH); // Relay OFF (active low)

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);

  // ── Servo
  warningServo.attach(PIN_SERVO);
  warningServo.write(0); // Neutral position

  // ── Dot Matrix
  dotMatrix.begin();
  dotMatrix.control(MD_MAX72XX::INTENSITY, 4);
  dotMatrix.clear();

  // ── WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // ── Web server routes
  server.on("/", []() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/data", handleDataRequest);
  server.begin();

  // ── Init history
  for (int i = 0; i < HISTORY_LEN; i++) riskHistory[i] = 0;

  // ── Boot animation
  bootAnimation();

  Serial.println("GEO-SENSE v2.0 ONLINE");
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  server.handleClient();

  unsigned long now = millis();

  // ── SAMPLE SENSORS ──────────────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readAllSensors();
    computeRisk();
    updateOutputs();
    sendSlaveCommand();
    logToSerial();
  }

  // ── UPDATE DISPLAYS ─────────────────────────────────────
  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    handleJoystickMenu();
    updateOLED();
    updateLCD();
  }

  // ── SCROLL DOT MATRIX ──────────────────────────────────
  if (now - lastScrollMs >= 80) {
    lastScrollMs = now;
    scrollDotMatrix();
  }

  // ── TOUCH ACKNOWLEDGE ──────────────────────────────────
  if (digitalRead(PIN_TOUCH) == HIGH && data.alertLevel >= 2) {
    alertAcknowledged = true;
    alertCount = 0;
    digitalWrite(PIN_RELAY, HIGH); // Silence external siren
    warningServo.write(0);
    Serial.println("ALERT ACKNOWLEDGED via touch sensor");
    lcd.clear();
    lcd.print("Alert ACK'd");
    delay(2000);
    alertAcknowledged = false;
  }
}

// ── READ ALL SENSORS ─────────────────────────────────────────
void readAllSensors() {

  // DHT11
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  data.airTemp     = isnan(t) ? 28.0 : t;
  data.airHumidity = isnan(h) ? 65.0 : h;

  // Soil Moisture (inverted)
  int rawSoil = analogRead(PIN_SOIL);
  data.soilMoisture = constrain(map(rawSoil, 4095, 1200, 0, 100), 0, 100);

  // HC-SR04 Ultrasound (flood distance — lower = higher water)
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  data.distanceCm = (duration == 0) ? 400.0 : (duration * 0.0343 / 2.0);

  // PIR Motion
  data.pirMotion = digitalRead(PIN_PIR);

  // Obstacle
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);

  // Touch
  data.touchPressed = (digitalRead(PIN_TOUCH) == HIGH);

  // Joystick button (manual flood test)
  // Handled in menu function

  // ── READ FROM ARDUINO SLAVE (Serial2) ─────────────────
  // Protocol: "W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x\n"
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    parseSlaveData(line);
  }

  // ── EMA SMOOTHING ─────────────────────────────────────
  // Convert distance to flood % (closer = higher risk; baseline 200cm)
  float floodPct = constrain(map((int)data.distanceCm, 200, 20, 0, 100), 0, 100);

  ema_soil  = EMA_A * data.soilMoisture + (1.0f - EMA_A) * ema_soil;
  ema_water = EMA_A * max(floodPct, data.waterLevel) + (1.0f - EMA_A) * ema_water;
  ema_dist  = EMA_A * floodPct + (1.0f - EMA_A) * ema_dist;
  ema_tilt  = EMA_A * data.tiltAngle + (1.0f - EMA_A) * ema_tilt;
  ema_temp  = EMA_A * abs(data.airTemp - data.thermistorTemp) / 15.0f * 100.0f
              + (1.0f - EMA_A) * ema_temp;
  ema_temp  = constrain(ema_temp, 0, 100);

  // ── EVAPOTRANSPIRATION ────────────────────────────────
  // Australian BOM Apparent Temperature + Penman-Monteith simplified
  float vapourPressure = (data.airHumidity / 100.0f) * 0.611f *
                          exp(17.27f * data.airTemp / (data.airTemp + 237.3f));
  data.evapotranspiration = data.airTemp + (0.33f * vapourPressure) - 4.0f;
}

// ── PARSE SLAVE DATA ─────────────────────────────────────────
void parseSlaveData(String line) {
  // Format: W:38.2,T:22.5,S:12.3,P:1.00,I:0
  int wIdx = line.indexOf("W:");
  int tIdx = line.indexOf("T:");
  int sIdx = line.indexOf("S:");
  int pIdx = line.indexOf("P:");
  int iIdx = line.indexOf("I:");

  if (wIdx >= 0) data.waterLevel    = line.substring(wIdx+2, line.indexOf(',', wIdx)).toFloat();
  if (tIdx >= 0) data.thermistorTemp = line.substring(tIdx+2, line.indexOf(',', tIdx)).toFloat();
  if (sIdx >= 0) data.tiltAngle     = line.substring(sIdx+2, line.indexOf(',', sIdx)).toFloat();
  if (pIdx >= 0) data.potCalibration = line.substring(pIdx+2, line.indexOf(',', pIdx)).toFloat();
  if (iIdx >= 0) data.tiltInterrupt  = (line.substring(iIdx+2).toInt() == 1);
}

// ── COMPUTE RISK INDEX ───────────────────────────────────────
void computeRisk() {
  // CRITICAL: Hardware landslide interrupt from slave overrides everything
  if (data.tiltInterrupt) {
    data.riskIndex  = 9.0;
    data.hazardType = "LANDSLIDE";
    data.alertLevel = 3;
    alertCount = 99;
    return;
  }

  // PIR motion + high water = mass evacuation event
  float pirBonus = (data.pirMotion && ema_water > 50) ? 10.0f : 0.0f;

  // Debris + flood = compound risk
  float debrisBonus = (data.obstacleDetected && ema_water > 30) ? 8.0f : 0.0f;

  // Core weighted formula
  float weighted = (ema_soil  * 0.25f) +
                   (ema_water * 0.35f) +
                   (ema_tilt  * 0.25f) +
                   (ema_temp  * 0.10f) +
                   pirBonus + debrisBonus;

  // Apply potentiometer sensitivity calibration from slave
  float cal = (data.potCalibration > 0) ? data.potCalibration : 1.0f;
  weighted = constrain(weighted * cal, 0.0f, 100.0f);

  data.riskIndex = (weighted / 100.0f) * 9.0f;

  // Determine dominant hazard
  bool floodDominant   = (ema_water > ema_soil && ema_water > 50 && data.distanceCm < 80);
  bool droughtDominant = (ema_soil > 70 && ema_water < 25 && data.airTemp > 30);
  bool landDominant    = (ema_tilt > 60);

  if (landDominant)        data.hazardType = "LANDSLIDE";
  else if (floodDominant)  data.hazardType = "FLOOD";
  else if (droughtDominant)data.hazardType = "DROUGHT";
  else                     data.hazardType = "NOMINAL";

  // Alert level
  if      (data.riskIndex <= 3) data.alertLevel = 0;
  else if (data.riskIndex <= 6) data.alertLevel = 1;
  else                          data.alertLevel = 2;

  // Hysteresis
  if (data.alertLevel >= 2) alertCount++;
  else alertCount = max(0, alertCount - 1);

  // Push to history
  riskHistory[historyIdx % HISTORY_LEN] = data.riskIndex;
  historyIdx++;

  // Set scroll message
  if (data.alertLevel == 0) scrollMsg = "ALL CLEAR - GEO-SENSE NOMINAL ";
  else if (data.alertLevel == 1) scrollMsg = "WARNING: " + data.hazardType + " PRECURSOR DETECTED ";
  else scrollMsg = "!! CRITICAL " + data.hazardType + " ALERT - EVACUATE !! ";
}

// ── UPDATE OUTPUTS ───────────────────────────────────────────
void updateOutputs() {
  bool critical = (alertCount >= 3 && !alertAcknowledged) || data.tiltInterrupt;

  // ── LEDs
  digitalWrite(LED_GREEN,  data.alertLevel == 0 ? HIGH : LOW);
  digitalWrite(LED_YELLOW, data.hazardType == "DROUGHT"   ? HIGH : LOW);
  digitalWrite(LED_BLUE,   data.hazardType == "FLOOD"     ? HIGH : LOW);
  digitalWrite(LED_RED,    data.alertLevel == 2 || data.tiltInterrupt ? HIGH : LOW);

  // ── Relay (external siren/lamp)
  digitalWrite(PIN_RELAY, critical ? LOW : HIGH); // Active low

  // ── Servo (warning flag / flood gate)
  if (data.tiltInterrupt) {
    warningServo.write(180); // Full evacuation position
  } else if (data.alertLevel == 2) {
    warningServo.write(135); // Warning position
  } else if (data.alertLevel == 1) {
    warningServo.write(90);  // Caution position
  } else {
    warningServo.write(0);   // All clear
  }
}

// ── UPDATE OLED DISPLAY ──────────────────────────────────────
void updateOLED() {
  oled.clearDisplay();

  // Page based on joystick menu selection
  switch (menuPage) {

    case 0: // RISK GAUGE
    {
      oled.setTextSize(1);
      oled.setCursor(0, 0);
      oled.print("RISK INDEX");
      oled.setTextSize(3);
      oled.setCursor(0, 16);
      oled.print((int)data.riskIndex);
      oled.setTextSize(1);
      oled.setCursor(22, 30);
      oled.print("/9");
      oled.setCursor(60, 0);
      oled.print(data.hazardType.substring(0, 8));
      // Risk bar
      oled.drawRect(60, 14, 65, 10, SSD1306_WHITE);
      int barW = (int)((data.riskIndex / 9.0f) * 63.0f);
      oled.fillRect(61, 15, barW, 8, SSD1306_WHITE);
      // Waveform
      oled.drawLine(0, 55, 128, 55, SSD1306_WHITE);
      for (int i = 0; i < 50; i++) {
        int idx = (historyIdx + i) % HISTORY_LEN;
        int x   = i * 2 + 14;
        int y   = 55 - (int)(riskHistory[idx] / 9.0f * 18.0f);
        oled.drawPixel(x, y, SSD1306_WHITE);
        if (i > 0) {
          int pidx = (historyIdx + i - 1) % HISTORY_LEN;
          int px   = (i - 1) * 2 + 14;
          int py   = 55 - (int)(riskHistory[pidx] / 9.0f * 18.0f);
          oled.drawLine(px, py, x, y, SSD1306_WHITE);
        }
      }
    } break;

    case 1: // FLOOD DATA
    {
      oled.setTextSize(1);
      oled.setCursor(0, 0);  oled.print("-- FLOOD DATA --");
      oled.setCursor(0, 12); oled.print("Distance: ");
      oled.print(data.distanceCm, 1); oled.print("cm");
      oled.setCursor(0, 22); oled.print("WaterLvl: ");
      oled.print(data.waterLevel, 1); oled.print("%");
      oled.setCursor(0, 32); oled.print("Debris:   ");
      oled.print(data.obstacleDetected ? "YES!" : "Clear");
      oled.setCursor(0, 42); oled.print("PIR:      ");
      oled.print(data.pirMotion ? "MOTION!" : "Clear");
      oled.setCursor(0, 54); oled.print("Servo: ");
      oled.print(warningServo.read()); oled.print("deg");
    } break;

    case 2: // DROUGHT / TEMP
    {
      oled.setTextSize(1);
      oled.setCursor(0, 0);  oled.print("-- DROUGHT DATA -");
      oled.setCursor(0, 12); oled.print("Soil: ");
      oled.print(data.soilMoisture, 1); oled.print("% moisture");
      oled.setCursor(0, 22); oled.print("AirT: ");
      oled.print(data.airTemp, 1); oled.print("C ");
      oled.print(data.airHumidity, 0); oled.print("%H");
      oled.setCursor(0, 32); oled.print("SoilT:");
      oled.print(data.thermistorTemp, 1); oled.print("C");
      oled.setCursor(0, 42); oled.print("ET:   ");
      oled.print(data.evapotranspiration, 2);
      oled.setCursor(0, 54); oled.print("Wilting risk: ");
      oled.print((int)(data.soilMoisture)); oled.print("%");
    } break;

    case 3: // SYSTEM / WIFI
    {
      oled.setTextSize(1);
      oled.setCursor(0, 0);  oled.print("-- SYSTEM INFO --");
      oled.setCursor(0, 10); oled.print("WiFi: GeoSense-Afr");
      oled.setCursor(0, 20); oled.print("IP: 192.168.4.1");
      oled.setCursor(0, 30); oled.print("Sensitivity: ");
      oled.print(data.potCalibration, 2); oled.print("x");
      oled.setCursor(0, 40); oled.print("Alert count: ");
      oled.print(alertCount);
      oled.setCursor(0, 50); oled.print("Touch=ACK Tilt=ISR");
    } break;
  }

  oled.display();
}

// ── UPDATE LCD ───────────────────────────────────────────────
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);

  // Line 1: Hazard + risk
  String line1 = data.hazardType;
  while (line1.length() < 10) line1 += " ";
  line1 += " R:";
  line1 += String((int)data.riskIndex);
  line1 += "/9";
  lcd.print(line1.substring(0, 16));

  // Line 2: Key sensor values
  lcd.setCursor(0, 1);
  char line2[17];
  snprintf(line2, 17, "T:%.0fC H:%.0f%% S:%.0f",
           data.airTemp, data.airHumidity, data.soilMoisture);
  lcd.print(line2);
}

// ── JOYSTICK MENU NAVIGATION ─────────────────────────────────
void handleJoystickMenu() {
  int jx = analogRead(PIN_JOY_X);
  // Left/right to change page
  if (jx < 1000) {
    menuPage = (menuPage + 1) % MENU_PAGES;
    delay(300);
  } else if (jx > 3000) {
    menuPage = (menuPage - 1 + MENU_PAGES) % MENU_PAGES;
    delay(300);
  }
}

// ── DOT MATRIX SCROLL ────────────────────────────────────────
void scrollDotMatrix() {
  // Simple pixel scroll using MD_MAX72XX
  dotMatrix.transform(MD_MAX72XX::TSL); // Shift left
  // Load new column from scrollMsg
  // (simplified: just show alert level indicator)
  static int dotFrame = 0;
  dotFrame++;
  if (dotFrame % 8 == 0) {
    byte col = 0;
    if (data.alertLevel == 0) col = 0b00011000; // Two middle dots
    else if (data.alertLevel == 1) col = 0b00111100;
    else col = 0b11111111; // Full column = critical
    dotMatrix.setColumn(0, MAX_DEVICES - 1, col);
  }
}

// ── SEND COMMAND TO SLAVE ─────────────────────────────────────
void sendSlaveCommand() {
  // ESP32 → Arduino: override commands if needed
  // Format: "CMD:ALERT_LEVEL\n"
  Serial2.print("CMD:");
  Serial2.println(data.alertLevel);
}

// ── WEB DATA ENDPOINT ────────────────────────────────────────
void handleDataRequest() {
  String json = "{";
  json += "\"risk\":" + String(data.riskIndex, 2) + ",";
  json += "\"hazard\":\"" + data.hazardType + "\",";
  json += "\"dist\":" + String(data.distanceCm, 1) + ",";
  json += "\"water\":" + String(data.waterLevel, 1) + ",";
  json += "\"soil\":" + String(data.soilMoisture, 1) + ",";
  json += "\"airT\":" + String(data.airTemp, 1) + ",";
  json += "\"airH\":" + String(data.airHumidity, 1) + ",";
  json += "\"soilT\":" + String(data.thermistorTemp, 1) + ",";
  json += "\"et\":" + String(data.evapotranspiration, 2) + ",";
  json += "\"tilt\":" + String(data.tiltAngle, 1) + ",";
  json += "\"pot\":" + String(data.potCalibration, 2) + ",";
  json += "\"pir\":" + String(data.pirMotion ? "true" : "false") + ",";
  json += "\"obs\":" + String(data.obstacleDetected ? "true" : "false") + ",";
  json += "\"lsi\":" + String(data.tiltInterrupt ? "true" : "false") + ",";
  json += "\"relay\":" + String(digitalRead(PIN_RELAY) == LOW ? "true" : "false") + ",";
  json += "\"servoPos\":" + String(warningServo.read());
  json += "}";
  server.send(200, "application/json", json);
}

// ── BOOT ANIMATION ───────────────────────────────────────────
void bootAnimation() {
  // LED chase
  int leds[] = {LED_GREEN, LED_BLUE, LED_YELLOW, LED_RED};
  for (int r = 0; r < 3; r++) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(leds[i], HIGH); delay(100);
      digitalWrite(leds[i], LOW);
    }
  }
  // Servo sweep
  for (int p = 0; p <= 180; p += 10) { warningServo.write(p); delay(20); }
  for (int p = 180; p >= 0; p -= 10)  { warningServo.write(p); delay(20); }

  // LCD welcome
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("GEO-SENSE AFRICA");
  lcd.setCursor(0, 1); lcd.print("WiFi: 192.168.4.1");
  delay(3000);
}

// ── SERIAL LOGGING ───────────────────────────────────────────
void logToSerial() {
  Serial.println("=== GEO-SENSE v2.0 ===");
  Serial.printf("Risk: %.2f/9  Hazard: %s  Alert: %d\n",
                data.riskIndex, data.hazardType.c_str(), data.alertLevel);
  Serial.printf("Soil: %.1f%%  Water: %.1f%%  Dist: %.1fcm\n",
                data.soilMoisture, data.waterLevel, data.distanceCm);
  Serial.printf("AirT: %.1fC  AirH: %.1f%%  ET: %.2f\n",
                data.airTemp, data.airHumidity, data.evapotranspiration);
  Serial.printf("SoilT: %.1fC  Tilt: %.1f%%  Cal: %.2fx\n",
                data.thermistorTemp, data.tiltAngle, data.potCalibration);
  Serial.printf("PIR: %d  Obstacle: %d  TiltISR: %d\n",
                data.pirMotion, data.obstacleDetected, data.tiltInterrupt);
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("-------------------------------");
}
