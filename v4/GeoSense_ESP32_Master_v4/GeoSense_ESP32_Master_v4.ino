// ================================================================
//  GEO-SENSE AFRICA v4.1 — ESP32 MASTER NODE
//  Multi-Hazard Early Warning System — Fusion & Gateway
// ================================================================
//  v4.1 IMPROVEMENTS:
//  - Enhanced fluid web dashboard with modern animations
//  - Optimized code structure and memory usage
//  - Improved error handling and validation
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
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <esp_task_wdt.h>

// ════════════════════════════════════════════════════════════
//  CONFIGURATION
// ════════════════════════════════════════════════════════════
const char* AP_SSID = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";
const char* STA_SSID = "YOUR_ROUTER_SSID";
const char* STA_PASSWORD = "YOUR_ROUTER_PASSWORD";
const char* SERVER_IP = "192.168.1.100";
const uint16_t SERVER_PORT = 5000;
const char* SERVER_PATH = "/api/ingest";

#define POST_INTERVAL_MS   10000UL
#define POST_TIMEOUT_MS    4000
#define POST_RETRY_QUEUE   3
#define POST_BACKOFF_MAX   120000UL

// ════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ════════════════════════════════════════════════════════════
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
#define PIN_DM_CS     15
#define PIN_DM_DATA   23
#define PIN_DM_CLK    26
#define LED_GREEN     25
#define LED_BLUE      2
#define LED_YELLOW    27
#define LED_RED       33
#define PIN_JOY_X     34
#define PIN_JOY_Y     35
#define PIN_JOY_BTN   32
#define SLAVE_RX      16
#define SLAVE_TX      17
#define OLED_W        128
#define OLED_H        64
#define OLED_ADDR     0x3C
#define LCD_ADDR      0x27

// ════════════════════════════════════════════════════════════
//  DISPLAY OBJECTS
// ════════════════════════════════════════════════════════════
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
MD_MAX72XX dotMatrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, PIN_DM_CS, PIN_DM_DATA, PIN_DM_CLK, 1);
DHT dht(PIN_DHT, DHT_TYPE);
Servo warningServo;
WebServer server(80);

// ════════════════════════════════════════════════════════════
//  SENSOR DATA STRUCTURE
// ════════════════════════════════════════════════════════════
struct SensorData {
  float soilMoisture, airTemp, airHumidity, distanceCm, evapotranspiration;
  bool pirMotion, obstacleDetected;
  float waterLevel, tiltPct, thermistorTemp, potCalibration, photoPct;
  bool tiltInterrupt;
  float riskIndex;
  int alertLevel;
  char hazardType[12];
};

SensorData data = {50,25,60,400,0,false,false,0,0,25,1,50,false,0,0,"NOMINAL"};

// ════════════════════════════════════════════════════════════
//  EMA SMOOTHING
// ════════════════════════════════════════════════════════════
float ema_soil = 50, ema_water = 0, ema_tilt = 0, ema_temp = 0;
#define EMA_A 0.25f

// ════════════════════════════════════════════════════════════
//  TIMING STATE
// ════════════════════════════════════════════════════════════
uint32_t lastSampleMs=0, lastDisplayMs=0, lastScrollMs=0, lastPostMs=0;
uint32_t lastDecayMs=0, bootMs=0, postBackoffMs=10000, lastSuccessPostMs=0;
#define SAMPLE_INTERVAL    3000UL
#define DISPLAY_INTERVAL   400UL
#define SCROLL_INTERVAL    90UL
#define DECAY_INTERVAL     500UL

// ════════════════════════════════════════════════════════════
//  BUFFERS & STATE
// ════════════════════════════════════════════════════════════
#define HISTORY_LEN 60
#define LOG_SIZE 8
#define SLAVE_BUF 80
#define MAX_PAYLOAD 512

float riskHistory[HISTORY_LEN];
uint8_t historyIdx = 0;

struct AlertEntry { uint32_t ts; char msg[40]; };
AlertEntry alertLog[LOG_SIZE];
uint8_t logHead = 0;

struct PostEntry { char payload[MAX_PAYLOAD]; bool pending; };
PostEntry postQueue[POST_RETRY_QUEUE];
uint8_t postQHead = 0, postQCount = 0;

char slaveBuf[SLAVE_BUF];
uint8_t slaveIdx = 0;

bool serverReachable = false, alertAcked = false;
uint32_t ackedAt = 0;
char nodeID[18] = "00:00:00:00:00:00";

int menuPage = 0;
uint32_t lastJoyMs = 0;
uint8_t dmScrollTick = 0;

uint32_t gAlertCount = 0, gPostsSent = 0, gPostsFailed = 0;
#define MENU_PAGES 4
#define JOY_DEBOUNCE 350
#define ACK_TIMEOUT 60000UL

// ════════════════════════════════════════════════════════════
//  WEB DASHBOARD HTML (Enhanced Fluid UI v4.1)
// ════════════════════════════════════════════════════════════
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Geo-Sense Africa v4.1</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
:root{--bg:#0a0e1a;--card:#111827;--border:#1e293b;--text:#f1f5f9;--muted:#94a3b8;
--success:#10b981;--warning:#f59e0b;--danger:#ef4444;--info:#3b82f6}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px;
background-image:radial-gradient(circle at 20% 50%,rgba(139,92,246,0.05) 0%,transparent 50%)}
.wrap{max-width:1024px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;padding:20px 0;
border-bottom:1px solid var(--border);margin-bottom:24px;flex-wrap:wrap;gap:16px}
h1{font-size:20px;font-weight:700;background:linear-gradient(135deg,var(--success),var(--info));
-webkit-background-clip:text;-webkit-text-fill-color:transparent;display:flex;align-items:center;gap:10px}
.status-dot{width:10px;height:10px;border-radius:50%;background:var(--success);animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.hdr-right{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
.badge-pill{padding:4px 12px;border-radius:9999px;font-size:11px;font-weight:600;
font-family:monospace;background:rgba(16,185,129,0.1);border:1px solid rgba(16,185,129,0.3);color:var(--success)}
.server-status{font-size:11px;color:var(--muted);display:flex;align-items:center;gap:6px}
.server-dot{width:8px;height:8px;border-radius:50%}
.server-dot.ok{background:var(--success);box-shadow:0 0 8px var(--success)}
.server-dot.err{background:var(--danger);box-shadow:0 0 8px var(--danger)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;margin-bottom:16px}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;
transition:all 0.3s ease;position:relative;overflow:hidden}
.card:hover{border-color:rgba(139,92,246,0.3);transform:translateY(-2px);box-shadow:0 10px 15px -3px rgba(0,0,0,0.4)}
.card-label{font-size:10px;font-weight:700;letter-spacing:1.5px;color:var(--muted);text-transform:uppercase;margin-bottom:12px}
.card-badge{padding:3px 10px;border-radius:6px;font-size:10px;font-weight:700;transition:all 0.3s}
.badge-success{background:rgba(16,185,129,0.15);color:var(--success);border:1px solid rgba(16,185,129,0.3)}
.badge-warning{background:rgba(245,158,11,0.15);color:var(--warning);border:1px solid rgba(245,158,11,0.3)}
.badge-danger{background:rgba(239,68,68,0.15);color:var(--danger);border:1px solid rgba(239,68,68,0.3)}
.big-value{font-size:36px;font-weight:800;line-height:1;margin-bottom:8px}
.unit{font-size:14px;color:var(--muted);font-weight:400;margin-left:4px}
.data-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.04);
font-size:12px;color:var(--muted)}
.data-row:last-child{border:none}.data-row span:last-child{color:var(--text);font-weight:500}
.risk-bar-container{height:10px;background:rgba(30,41,59,0.5);border-radius:9999px;overflow:hidden;margin-top:12px}
.risk-bar{height:100%;border-radius:9999px;background:linear-gradient(90deg,var(--success),var(--warning),var(--danger));
transition:width 0.6s ease}
.alert-banner{grid-column:1/-1;background:rgba(239,68,68,0.1);border:2px solid var(--danger);
border-radius:16px;padding:20px;display:none;align-items:center;gap:12px;animation:bannerPulse 1.5s infinite}
.alert-banner.active{display:flex}
@keyframes bannerPulse{0%,100%{box-shadow:0 0 20px rgba(239,68,68,0.2)}50%{box-shadow:0 0 40px rgba(239,68,68,0.4)}}
.alert-icon{font-size:24px}.alert-title{font-size:13px;font-weight:700;color:var(--danger);text-transform:uppercase}
.alert-message{font-size:13px;color:var(--muted);margin-top:4px}
.chart-container{height:140px;margin-top:12px}
.event-log{font-family:monospace;font-size:11px;color:var(--muted);line-height:1.8;
margin-top:12px;max-height:120px;overflow-y:auto;background:rgba(0,0,0,0.2);border-radius:8px;padding:12px}
.log-entry{padding:2px 0;border-bottom:1px solid rgba(255,255,255,0.02)}
.log-entry::before{content:'› ';color:var(--info)}
footer{text-align:center;margin-top:24px;padding:16px 0;font-size:11px;color:var(--muted)}
@media(max-width:640px){.grid{grid-template-columns:1fr}.big-value{font-size:28px}
header{flex-direction:column;align-items:flex-start}.hdr-right{width:100%;justify-content:space-between}}
</style></head><body><div class="wrap">
<header><h1><span class="status-dot"></span>GEO-SENSE AFRICA</h1>
<div class="hdr-right"><span id="node-badge" class="badge-pill">NODE --</span>
<span class="server-status"><span class="server-dot err" id="server-dot"></span>
<span id="server-text">Connecting...</span></span>
<span id="timestamp" class="timestamp">--:--:--</span></div></header>
<div id="alert-banner" class="alert-banner"><span class="alert-icon">⚠️</span>
<div class="alert-text"><div class="alert-title">Critical Alert</div>
<div class="alert-message" id="alert-message">Standby...</div></div></div>
<div class="grid"><div class="card"><div class="card-label">Risk Index</div>
<span id="risk-badge" class="card-badge badge-success">NOMINAL</span>
<div class="big-value" id="risk-value">0<span class="unit">/9</span></div>
<div class="risk-bar-container"><div id="risk-bar" class="risk-bar" style="width:0%"></div></div>
<div class="chart-container"><canvas id="risk-chart"></canvas></div></div>
<div class="card"><div class="card-label">Flood Monitor</div>
<div class="big-value" id="water-value">0<span class="unit">% level</span></div>
<div class="data-row"><span>River Distance</span><span id="distance-value">-- cm</span></div>
<div class="data-row"><span>Debris/Obstacle</span><span id="debris-value">Clear</span></div>
<div class="data-row"><span>Gate Servo</span><span id="servo-value">0°</span></div>
<div class="data-row"><span>PIR Motion</span><span id="pir-value">None</span></div>
<div class="chart-container"><canvas id="water-chart"></canvas></div></div>
<div class="card"><div class="card-label">Drought & Agriculture</div>
<div class="big-value" id="soil-value">0<span class="unit">% moisture</span></div>
<div class="data-row"><span>Air Temperature</span><span id="air-temp-value">-- °C</span></div>
<div class="data-row"><span>Air Humidity</span><span id="air-hum-value">-- %</span></div>
<div class="data-row"><span>Soil Temperature</span><span id="soil-temp-value">-- °C</span></div>
<div class="data-row"><span>Evapotranspiration</span><span id="et-value">-- AT</span></div>
<div class="data-row"><span>Solar/Light</span><span id="light-value">-- %</span></div>
<div class="chart-container"><canvas id="soil-chart"></canvas></div></div>
<div class="card"><div class="card-label">Geo-Stability & System</div>
<div class="big-value" id="tilt-value">0<span class="unit">% slope</span></div>
<div class="data-row"><span>Tilt ISR</span><span id="tilt-isr-value">OK</span></div>
<div class="data-row"><span>Calibration</span><span id="cal-value">1.00×</span></div>
<div class="data-row"><span>Relay/Siren</span><span id="relay-value">Off</span></div>
<div class="data-row"><span>Alert Count</span><span id="alert-count-value">0</span></div>
<div class="data-row"><span>Uptime</span><span id="uptime-value">--</span></div>
<div class="data-row"><span>Posts Sent/Failed</span><span id="posts-value">--</span></div>
<div class="card-label" style="margin-top:12px">Event Log</div>
<div class="event-log" id="event-log">Initializing...</div></div></div>
<footer>GEO-SENSE AFRICA v4.1 | AP: GeoSense-Africa / geosense2024 | 192.168.4.1</footer></div>
<script>
const createChart=(id,color,min,max,pts)=>{const ctx=document.getElementById(id).getContext('2d');
const gradient=ctx.createLinearGradient(0,0,0,max);gradient.addColorStop(0,color+'40');
gradient.addColorStop(1,color+'05');return new Chart(ctx,{type:'line',
data:{labels:Array(pts).fill(''),datasets:[{data:Array(pts).fill(0),borderColor:color,
backgroundColor:gradient,tension:0.4,fill:true,pointRadius:0,borderWidth:2}]},
options:{responsive:true,maintainAspectRatio:false,animation:{duration:300},
scales:{y:{min,max,grid:{color:'#1e293b'},ticks:{color:'#64748b',font:{size:9}}},x:{display:false}},
plugins:{legend:{display:false}}}})};
const riskChart=createChart('risk-chart','#10b981',0,9,30);
const waterChart=createChart('water-chart','#3b82f6',0,100,20);
const soilChart=createChart('soil-chart','#f59e0b',0,100,20);
const updateChart=(c,v)=>{c.data.datasets[0].data.push(v);c.data.datasets[0].data.shift();c.update('none')};
const getRiskColor=r=>r>6?'#ef4444':r>3?'#f59e0b':'#10b981';
const getRiskBadge=r=>r>6?{c:'badge-danger',t:'CRITICAL'}:r>3?{c:'badge-warning',t:'WARNING'}:{c:'badge-success',t:'NOMINAL'};
const formatUptime=s=>s>3600?`${Math.floor(s/3600)}h ${Math.floor((s%3600)/60)}m`:s>60?`${Math.floor(s/60)}m ${s%60}s`:`${s}s`;
const setFlag=(id,c,t,f,tc,fc)=>{const el=document.getElementById(id);el.textContent=c?t:f;el.style.color=c?tc:fc};
async function poll(){try{const d=await fetch('/data').then(r=>r.json());
const risk=parseFloat(d.risk.toFixed(2)),pct=((risk/9)*100).toFixed(1);
document.getElementById('risk-value').innerHTML=risk.toFixed(1)+'<span class="unit">/9</span>';
const rb=document.getElementById('risk-bar');rb.style.width=pct+'%';
rb.style.background=`linear-gradient(90deg,${getRiskColor(risk)},${getRiskColor(risk)}88)`;
const badge=getRiskBadge(risk),be=document.getElementById('risk-badge');be.className=`card-badge ${badge.c}`;be.textContent=badge.t;
const banner=document.getElementById('alert-banner');
if(d.lsi||risk>7){banner.classList.add('active');document.getElementById('alert-message').textContent=d.lsi?
'LANDSLIDE SENSOR TRIGGERED! EVACUATE IMMEDIATELY.':`HIGH RISK: ${d.hazard}. TAKE ACTION.`;}
else banner.classList.remove('active');
document.getElementById('water-value').innerHTML=parseFloat(d.water).toFixed(1)+'<span class="unit">% level</span>';
document.getElementById('distance-value').textContent=parseFloat(d.dist).toFixed(1)+' cm';
setFlag('debris-value',d.obs,'DETECTED','Clear','#f59e0b','#10b981');
document.getElementById('servo-value').textContent=d.servoPos+'°';
setFlag('pir-value',d.pir,'MOTION','None','#ef4444','#94a3b8');
document.getElementById('soil-value').innerHTML=parseFloat(d.soil).toFixed(1)+'<span class="unit">% moisture</span>';
document.getElementById('air-temp-value').textContent=parseFloat(d.airT).toFixed(1)+' °C';
document.getElementById('air-hum-value').textContent=parseFloat(d.airH).toFixed(1)+' %';
document.getElementById('soil-temp-value').textContent=parseFloat(d.soilT).toFixed(1)+' °C';
document.getElementById('et-value').textContent=parseFloat(d.et).toFixed(2)+' AT';
document.getElementById('light-value').textContent=parseFloat(d.photo).toFixed(0)+' %';
document.getElementById('tilt-value').innerHTML=parseFloat(d.tilt).toFixed(1)+'<span class="unit">% slope</span>';
setFlag('tilt-isr-value',d.lsi,'TRIGGERED!','OK','#ef4444','#10b981');
document.getElementById('cal-value').textContent=parseFloat(d.pot).toFixed(2)+'×';
setFlag('relay-value',d.relay,'ACTIVE','Off','#ef4444','#94a3b8');
document.getElementById('alert-count-value').textContent=d.alertCount;
document.getElementById('uptime-value').textContent=formatUptime(parseInt(d.uptime));
document.getElementById('posts-value').textContent=`${d.postsSent||0}/${d.postsFailed||0}`;
document.getElementById('node-badge').textContent=`NODE ${d.nodeID||'--'}`;
const sd=document.getElementById('server-dot'),st=document.getElementById('server-text');
if(d.serverOk){sd.className='server-dot ok';st.textContent=`OK (${d.lastPost}s ago)`;}
else{sd.className='server-dot err';st.textContent='Unreachable';}
if(d.log&&d.log.length>0)document.getElementById('event-log').innerHTML=d.log.filter(e=>e&&e.length>0)
.map(e=>`<div class="log-entry">${e}</div>`).join('');
updateChart(riskChart,risk);updateChart(waterChart,parseFloat(d.water));updateChart(soilChart,parseFloat(d.soil));
document.getElementById('timestamp').textContent=new Date().toLocaleTimeString();}
catch(e){document.getElementById('timestamp').textContent='Reconnecting...';}}
poll();setInterval(poll,2500);
</script></body></html>
)rawliteral";

// ════════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ════════════════════════════════════════════════════════════
void readLocalSensors();
void handleSlaveSerial();
void parseSlaveData(char* line);
void computeRisk();
void updateOutputs();
void updateOLED();
void updateLCD();
void handleJoystick();
void scrollDotMatrix();
void handleDataRequest();
void handleLogRequest();
void handleResetRequest();
void addLog(const char* msg);
void bootAnimation();
void buildPayload(char* buf, size_t len);
bool postToServer(const char* payload);
void enqueuePost(const char* payload);
void drainPostQueue();
void decaySlope();

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  esp_task_wdt_init(10, true);
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
  pinMode(PIN_JOY_BTN, INPUT_PULLUP);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) Serial.println(F("OLED not found"));
  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);
  oled.setCursor(0, 0); oled.println(F("GEO-SENSE AFRICA"));
  oled.setCursor(0, 10); oled.println(F("v4.1 BOOTING...")); oled.display();

  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.print(F("GEO-SENSE v4.1  ")); lcd.setCursor(0, 1); lcd.print(F("  BOOTING...    "));
  dht.begin();
  warningServo.attach(PIN_SERVO); warningServo.write(0);
  dotMatrix.begin(); dotMatrix.control(MD_MAX72XX::INTENSITY, 3); dotMatrix.clear();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print(F("AP IP: ")); Serial.println(WiFi.softAPIP());

  if (strlen(STA_SSID) > 0) {
    WiFi.begin(STA_SSID, STA_PASSWORD);
    Serial.print(F("Connecting to router"));
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      esp_task_wdt_reset(); delay(500); Serial.print('.'); attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("\nSTA IP: ")); Serial.println(WiFi.localIP());
      addLog("WiFi STA connected");
    } else {
      Serial.println(F("\nSTA failed - AP-only mode"));
      addLog("WiFi STA failed");
    }
  }

  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(nodeID, sizeof(nodeID), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  Serial.print(F("Node ID: ")); Serial.println(nodeID);

  server.on("/", []() { server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleDataRequest);
  server.on("/log", handleLogRequest);
  server.on("/reset", handleResetRequest);
  server.begin();

  memset(riskHistory, 0, sizeof(riskHistory));
  for (int i = 0; i < POST_RETRY_QUEUE; i++) postQueue[i].pending = false;

  bootMs = millis();
  bootAnimation();
  addLog("GEO-SENSE v4.1 online");
  Serial.println(F("GEO-SENSE AFRICA v4.1 - ONLINE"));
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  uint32_t now = millis();

  while (Serial2.available()) handleSlaveSerial();

  if (now - lastDecayMs >= DECAY_INTERVAL) { lastDecayMs = now; decaySlope(); }

  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readLocalSensors();
    computeRisk();
    updateOutputs();
    char cmd[12];
    snprintf(cmd, sizeof(cmd), "CMD:%d\n", data.alertLevel);
    Serial2.print(cmd);
  }

  uint32_t effInterval = min(postBackoffMs, POST_BACKOFF_MAX);
  if (now - lastPostMs >= effInterval) {
    lastPostMs = now;
    char payload[MAX_PAYLOAD];
    buildPayload(payload, sizeof(payload));
    if (postToServer(payload)) {
      gPostsSent++; postBackoffMs = POST_INTERVAL_MS;
      lastSuccessPostMs = now; serverReachable = true; drainPostQueue();
    } else {
      gPostsFailed++;
      postBackoffMs = min(postBackoffMs * 2, POST_BACKOFF_MAX);
      serverReachable = false; enqueuePost(payload);
    }
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    handleJoystick(); updateOLED(); updateLCD();
  }

  if (now - lastScrollMs >= SCROLL_INTERVAL) { lastScrollMs = now; scrollDotMatrix(); }

  if (digitalRead(PIN_TOUCH) == HIGH && !alertAcked) {
    alertAcked = true; ackedAt = now;
    digitalWrite(PIN_RELAY, HIGH); warningServo.write(0);
    addLog("Alert ACK (touch)");
    lcd.clear(); lcd.print(F("Alert ACK'd     "));
  }
  if (alertAcked && (now - ackedAt > ACK_TIMEOUT)) alertAcked = false;
}

// ════════════════════════════════════════════════════════════
//  READ LOCAL SENSORS
// ════════════════════════════════════════════════════════════
void readLocalSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) data.airTemp = t;
  if (!isnan(h)) data.airHumidity = h;

  int rawSoil = analogRead(PIN_SOIL);
  data.soilMoisture = constrain((float)map(rawSoil, 3200, 1200, 0, 100), 0.0f, 100.0f);

  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  uint32_t dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  data.distanceCm = (dur == 0) ? 400.0f : (float)dur * 0.01715f;

  data.pirMotion = digitalRead(PIN_PIR);
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);

  float e = (data.airHumidity / 100.0f) * 0.6108f * expf(17.27f * data.airTemp / (data.airTemp + 237.3f));
  data.evapotranspiration = data.airTemp + (0.33f * e) - 4.0f;
}

// ════════════════════════════════════════════════════════════
//  HANDLE SLAVE SERIAL
// ════════════════════════════════════════════════════════════
void handleSlaveSerial() {
  char c = (char)Serial2.read();
  if (c == '\n' || c == '\r') {
    if (slaveIdx > 0) { slaveBuf[slaveIdx] = '\0'; parseSlaveData(slaveBuf); }
    slaveIdx = 0;
  } else if (slaveIdx < SLAVE_BUF - 1) {
    slaveBuf[slaveIdx++] = c;
  } else { slaveIdx = 0; }
}

void parseSlaveData(char* line) {
  auto getF = [&](const char* key) -> float {
    char* p = strstr(line, key);
    return p ? atof(p + strlen(key)) : -999.0f;
  };
  float w = getF("W:"); if (w > -999.0f) data.waterLevel = constrain(w, 0.0f, 100.0f);
  float t = getF("T:"); if (t > -999.0f) data.thermistorTemp = constrain(t, -40.0f, 125.0f);
  float s = getF("S:"); if (s > -999.0f) data.tiltPct = constrain(s, 0.0f, 100.0f);
  float p = getF("P:"); if (p > -999.0f) data.potCalibration = constrain(p, 0.5f, 2.0f);
  float l = getF("L:"); if (l > -999.0f) data.photoPct = constrain(l, 0.0f, 100.0f);
  char* ip = strstr(line, "I:");
  if (ip) {
    bool newTilt = (atoi(ip + 2) == 1);
    if (newTilt && !data.tiltInterrupt) addLog("TILT ISR triggered");
    data.tiltInterrupt = newTilt;
  }
}

// ════════════════════════════════════════════════════════════
//  SLOPE DECAY
// ════════════════════════════════════════════════════════════
void decaySlope() {
  if (!data.tiltInterrupt && data.tiltPct > 0.0f) data.tiltPct = max(0.0f, data.tiltPct - 1.5f);
}

// ════════════════════════════════════════════════════════════
//  COMPUTE RISK
// ════════════════════════════════════════════════════════════
void computeRisk() {
  if (data.tiltInterrupt) {
    data.riskIndex = 9.0f; data.alertLevel = 3;
    strncpy(data.hazardType, "LANDSLIDE", sizeof(data.hazardType));
    gAlertCount = 99;
    riskHistory[historyIdx % HISTORY_LEN] = 9.0f; historyIdx++;
    return;
  }

  float floodPct = constrain((float)map((int)data.distanceCm, 200, 20, 0, 100), 0.0f, 100.0f);
  ema_soil = EMA_A * data.soilMoisture + (1.0f - EMA_A) * ema_soil;
  ema_water = EMA_A * max(floodPct, data.waterLevel) + (1.0f - EMA_A) * ema_water;
  ema_tilt = EMA_A * data.tiltPct + (1.0f - EMA_A) * ema_tilt;
  float tempDelta = constrain(fabsf(data.airTemp - data.thermistorTemp) / 15.0f * 100.0f, 0.0f, 100.0f);
  ema_temp = EMA_A * tempDelta + (1.0f - EMA_A) * ema_temp;

  float weighted = (ema_soil * 0.25f) + (ema_water * 0.40f) + (ema_tilt * 0.25f) + (ema_temp * 0.10f);
  if (data.pirMotion && ema_water > 40.0f) weighted += 10.0f;
  if (data.obstacleDetected && ema_water > 25.0f) weighted += 8.0f;
  if (data.soilMoisture < 20.0f && data.airTemp > 35.0f) weighted += 12.0f;

  float cal = (data.potCalibration >= 0.5f && data.potCalibration <= 2.0f) ? data.potCalibration : 1.0f;
  data.riskIndex = constrain((weighted * cal / 100.0f) * 9.0f, 0.0f, 9.0f);

  bool flood = (ema_water > 60.0f && data.distanceCm < 100.0f);
  bool drought = (ema_soil > 65.0f && ema_water < 20.0f && data.airTemp > 28.0f);
  bool slope = (ema_tilt > 50.0f);

  if (slope) strncpy(data.hazardType, "LANDSLIDE", sizeof(data.hazardType));
  else if (flood) strncpy(data.hazardType, "FLOOD", sizeof(data.hazardType));
  else if (drought) strncpy(data.hazardType, "DROUGHT", sizeof(data.hazardType));
  else strncpy(data.hazardType, "NOMINAL", sizeof(data.hazardType));
  data.hazardType[sizeof(data.hazardType) - 1] = '\0';

  int newLevel = (data.riskIndex > 6.0f) ? 2 : (data.riskIndex > 3.0f) ? 1 : 0;
  if (newLevel >= 2) gAlertCount++; else gAlertCount = max(0, (int)gAlertCount - 1);
  data.alertLevel = (newLevel == 2 && gAlertCount >= 3) ? 2 : (newLevel == 1) ? 1 : 0;

  if (data.alertLevel == 2 && gAlertCount == 3) {
    char msg[40];
    snprintf(msg, sizeof(msg), "CRITICAL %s RI=%.1f", data.hazardType, data.riskIndex);
    addLog(msg);
  }
  riskHistory[historyIdx % HISTORY_LEN] = data.riskIndex;
  historyIdx++;
}

// ════════════════════════════════════════════════════════════
//  UPDATE OUTPUTS
// ════════════════════════════════════════════════════════════
void updateOutputs() {
  bool critical = (data.alertLevel >= 2 || data.tiltInterrupt) && !alertAcked;
  bool isFlood = (strncmp(data.hazardType, "FLOOD", 5) == 0);
  bool isDrought = (strncmp(data.hazardType, "DROUGHT", 7) == 0);
  bool isLandslide = (strncmp(data.hazardType, "LANDSLIDE", 9) == 0);

  digitalWrite(LED_GREEN, (data.alertLevel == 0) ? HIGH : LOW);
  digitalWrite(LED_BLUE, isFlood ? HIGH : LOW);
  digitalWrite(LED_YELLOW, isDrought ? HIGH : LOW);
  digitalWrite(LED_RED, (critical || isLandslide) ? HIGH : LOW);
  digitalWrite(PIN_RELAY, critical ? LOW : HIGH);

  int servoPos = 0;
  if (data.tiltInterrupt) servoPos = 180;
  else if (data.alertLevel == 2) servoPos = 135;
  else if (data.alertLevel == 1) servoPos = 80;
  warningServo.write(servoPos);
}

// ════════════════════════════════════════════════════════════
//  UPDATE OLED
// ════════════════════════════════════════════════════════════
void updateOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  for (int i = 0; i < MENU_PAGES; i++) oled.fillCircle(54 + i * 8, 62, (i == menuPage) ? 2 : 1, SSD1306_WHITE);
  char buf[32];

  switch (menuPage) {
    case 0:
      oled.setCursor(0, 0); oled.print(data.hazardType);
      oled.setTextSize(3); oled.setCursor(0, 14);
      snprintf(buf, sizeof(buf), "%d", (int)data.riskIndex);
      oled.print(buf);
      oled.setTextSize(1); oled.setCursor(30, 28); oled.print("/9");
      oled.drawRect(50, 16, 76, 10, SSD1306_WHITE);
      oled.fillRect(51, 17, (int)(data.riskIndex / 9.0f * 74.0f), 8, SSD1306_WHITE);
      for (int i = 1; i < 40; i++) {
        int i0 = (historyIdx + i - 1) % HISTORY_LEN;
        int i1 = (historyIdx + i) % HISTORY_LEN;
        int x0 = i * 3 + 2, y0 = 52 - (int)(riskHistory[i0] / 9.0f * 14.0f), y1 = 52 - (int)(riskHistory[i1] / 9.0f * 14.0f);
        oled.drawLine(x0, y0, x0 + 3, y1, SSD1306_WHITE);
      }
      break;
    case 1:
      oled.setCursor(0, 0); oled.print(F("FLOOD"));
      snprintf(buf, sizeof(buf), "Dist: %.1fcm", data.distanceCm); oled.setCursor(0, 12); oled.print(buf);
      snprintf(buf, sizeof(buf), "Water: %.1f%%", data.waterLevel); oled.setCursor(0, 22); oled.print(buf);
      oled.setCursor(0, 32); oled.print(F("Debris:")); oled.print(data.obstacleDetected ? "YES!" : "No");
      oled.setCursor(0, 42); oled.print(F("PIR: ")); oled.print(data.pirMotion ? "MOTION!" : "None");
      snprintf(buf, sizeof(buf), "Gate: %ddeg", warningServo.read()); oled.setCursor(0, 52); oled.print(buf);
      break;
    case 2:
      oled.setCursor(0, 0); oled.print(F("DROUGHT"));
      snprintf(buf, sizeof(buf), "Soil: %.1f%%", data.soilMoisture); oled.setCursor(0, 12); oled.print(buf);
      snprintf(buf, sizeof(buf), "AirT: %.1fC", data.airTemp); oled.setCursor(0, 22); oled.print(buf);
      snprintf(buf, sizeof(buf), "SoilT: %.1fC", data.thermistorTemp); oled.setCursor(0, 32); oled.print(buf);
      snprintf(buf, sizeof(buf), "ET: %.2f", data.evapotranspiration); oled.setCursor(0, 42); oled.print(buf);
      snprintf(buf, sizeof(buf), "Solar: %.0f%%", data.photoPct); oled.setCursor(0, 52); oled.print(buf);
      break;
    case 3:
      oled.setCursor(0, 0); oled.print(F("SYSTEM"));
      oled.setCursor(0, 10); oled.print(serverReachable ? F("Srv: OK ") : F("Srv: FAIL "));
      if (serverReachable) { snprintf(buf, sizeof(buf), "%lus ago", (unsigned long)((millis() - lastSuccessPostMs) / 1000)); oled.print(buf); }
      snprintf(buf, sizeof(buf), "Posts: %lu/%lu", gPostsSent, gPostsFailed); oled.setCursor(0, 20); oled.print(buf);
      snprintf(buf, sizeof(buf), "Cal: %.2fx", data.potCalibration); oled.setCursor(0, 30); oled.print(buf);
      snprintf(buf, sizeof(buf), "Up: %lus", (unsigned long)((millis() - bootMs) / 1000)); oled.setCursor(0, 40); oled.print(buf);
      oled.setCursor(0, 50); oled.print(F("Touch=ACK Joy=Menu"));
      break;
  }
  oled.display();
}

// ════════════════════════════════════════════════════════════
//  UPDATE LCD
// ════════════════════════════════════════════════════════════
void updateLCD() {
  char r0[17], r1[17];
  snprintf(r0, sizeof(r0), "%-8s R:%d/9", data.hazardType, (int)data.riskIndex);
  snprintf(r1, sizeof(r1), "W:%.0f%% S:%.0f%% %s", data.waterLevel, data.soilMoisture, serverReachable ? "SRV:OK" : "SRV:ERR");
  lcd.setCursor(0, 0); lcd.print(r0);
  lcd.setCursor(0, 1); lcd.print(r1);
}

// ════════════════════════════════════════════════════════════
//  HANDLE JOYSTICK
// ════════════════════════════════════════════════════════════
void handleJoystick() {
  uint32_t now = millis();
  if (now - lastJoyMs < JOY_DEBOUNCE) return;
  int x = analogRead(PIN_JOY_X);
  if (x < 1000) { menuPage = (menuPage + 1) % MENU_PAGES; lastJoyMs = now; }
  else if (x > 3000) { menuPage = (menuPage - 1 + MENU_PAGES) % MENU_PAGES; lastJoyMs = now; }
}

// ════════════════════════════════════════════════════════════
//  SCROLL DOT MATRIX
// ════════════════════════════════════════════════════════════
void scrollDotMatrix() {
  dotMatrix.transform(MD_MAX72XX::TSL);
  dmScrollTick++;
  if (dmScrollTick % 8 == 0) {
    uint8_t col = (data.alertLevel == 0) ? 0x18 : (data.alertLevel == 1) ? 0x3C : 0xFF;
    dotMatrix.setColumn(0, 0, col);
  }
}

// ════════════════════════════════════════════════════════════
//  BUILD JSON PAYLOAD
// ════════════════════════════════════════════════════════════
void buildPayload(char* buf, size_t len) {
  uint32_t uptime = (millis() - bootMs) / 1000;
  snprintf(buf, len, "{\"node_id\":\"%s\",\"uptime\":%lu,\"risk\":%.2f,\"hazard\":\"%s\",\"alert_level\":%d,"
    "\"soil\":%.1f,\"water\":%.1f,\"dist\":%.1f,\"air_temp\":%.1f,\"air_hum\":%.1f,\"soil_temp\":%.1f,"
    "\"et\":%.2f,\"tilt\":%.1f,\"photo\":%.1f,\"pir\":%s,\"obs\":%s,\"tilt_isr\":%s,\"relay\":%s,\"servo\":%d,\"pot\":%.2f}",
    nodeID, (unsigned long)uptime, data.riskIndex, data.hazardType, data.alertLevel,
    data.soilMoisture, data.waterLevel, data.distanceCm, data.airTemp, data.airHumidity, data.thermistorTemp,
    data.evapotranspiration, data.tiltPct, data.photoPct,
    data.pirMotion ? "true" : "false", data.obstacleDetected ? "true" : "false",
    data.tiltInterrupt ? "true" : "false", (digitalRead(PIN_RELAY) == LOW) ? "true" : "false",
    warningServo.read(), data.potCalibration);
}

// ════════════════════════════════════════════════════════════
//  POST TO SERVER
// ════════════════════════════════════════════════════════════
bool postToServer(const char* payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  char url[80];
  snprintf(url, sizeof(url), "http://%s:%u%s", SERVER_IP, SERVER_PORT, SERVER_PATH);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(POST_TIMEOUT_MS);
  int code = http.POST((uint8_t*)payload, strlen(payload));
  http.end();
  return (code >= 200 && code < 300);
}

// ════════════════════════════════════════════════════════════
//  POST QUEUE
// ════════════════════════════════════════════════════════════
void enqueuePost(const char* payload) {
  strncpy(postQueue[postQHead].payload, payload, sizeof(postQueue[postQHead].payload) - 1);
  postQueue[postQHead].payload[sizeof(postQueue[postQHead].payload) - 1] = '\0';
  postQueue[postQHead].pending = true;
  postQHead = (postQHead + 1) % POST_RETRY_QUEUE;
  if (postQCount < POST_RETRY_QUEUE) postQCount++;
}

void drainPostQueue() {
  for (int i = 0; i < POST_RETRY_QUEUE; i++) {
    if (!postQueue[i].pending) continue;
    esp_task_wdt_reset();
    if (postToServer(postQueue[i].payload)) {
      postQueue[i].pending = false; gPostsSent++;
      if (postQCount > 0) postQCount--;
    } else break;
  }
}

// ════════════════════════════════════════════════════════════
//  WEB ENDPOINTS
// ════════════════════════════════════════════════════════════
void handleDataRequest() {
  char logJson[300] = "[";
  int added = 0;
  for (int i = 0; i < LOG_SIZE && added < 4; i++) {
    int idx = ((int)logHead - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (alertLog[idx].msg[0] == '\0') continue;
    if (added > 0) strncat(logJson, ",", sizeof(logJson) - strlen(logJson) - 1);
    char entry[52];
    snprintf(entry, sizeof(entry), "\"%s\"", alertLog[idx].msg);
    strncat(logJson, entry, sizeof(logJson) - strlen(logJson) - 1);
    added++;
  }
  strncat(logJson, "]", sizeof(logJson) - strlen(logJson) - 1);

  uint32_t secSincePost = serverReachable ? (uint32_t)((millis() - lastSuccessPostMs) / 1000) : 9999;
  char buf[768];
  snprintf(buf, sizeof(buf), "{\"risk\":%.2f,\"hazard\":\"%s\",\"dist\":%.1f,\"water\":%.1f,\"soil\":%.1f,"
    "\"airT\":%.1f,\"airH\":%.1f,\"soilT\":%.1f,\"et\":%.2f,\"tilt\":%.1f,\"pot\":%.2f,\"photo\":%.1f,"
    "\"pir\":%s,\"obs\":%s,\"lsi\":%s,\"relay\":%s,\"servoPos\":%d,\"alertCount\":%d,\"uptime\":%lu,"
    "\"nodeID\":\"%s\",\"serverOk\":%s,\"lastPost\":%lu,\"postsSent\":%lu,\"postsFailed\":%lu,\"log\":%s}",
    data.riskIndex, data.hazardType, data.distanceCm, data.waterLevel, data.soilMoisture,
    data.airTemp, data.airHumidity, data.thermistorTemp, data.evapotranspiration, data.tiltPct,
    data.potCalibration, data.photoPct,
    data.pirMotion ? "true" : "false", data.obstacleDetected ? "true" : "false",
    data.tiltInterrupt ? "true" : "false", (digitalRead(PIN_RELAY) == LOW) ? "true" : "false",
    warningServo.read(), gAlertCount, (unsigned long)((millis() - bootMs) / 1000),
    nodeID, serverReachable ? "true" : "false", (unsigned long)secSincePost,
    (unsigned long)gPostsSent, (unsigned long)gPostsFailed, logJson);
  server.send(200, "application/json", buf);
}

void handleLogRequest() {
  char buf[400] = "[";
  int added = 0;
  for (int i = 0; i < LOG_SIZE; i++) {
    int idx = ((int)logHead - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (alertLog[idx].msg[0] == '\0') continue;
    if (added > 0) strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);
    char entry[52];
    snprintf(entry, sizeof(entry), "\"%s\"", alertLog[idx].msg);
    strncat(buf, entry, sizeof(buf) - strlen(buf) - 1);
    added++;
  }
  strncat(buf, "]", sizeof(buf) - strlen(buf) - 1);
  server.send(200, "application/json", buf);
}

void handleResetRequest() {
  gAlertCount = 0; alertAcked = false;
  server.send(200, "text/plain", "OK");
  addLog("Counter reset via /reset");
}

// ════════════════════════════════════════════════════════════
//  ADD LOG ENTRY
// ════════════════════════════════════════════════════════════
void addLog(const char* msg) {
  alertLog[logHead].ts = (millis() - bootMs) / 1000;
  strncpy(alertLog[logHead].msg, msg, sizeof(alertLog[logHead].msg) - 1);
  alertLog[logHead].msg[sizeof(alertLog[logHead].msg) - 1] = '\0';
  logHead = (logHead + 1) % LOG_SIZE;
  Serial.print(F("[LOG] ")); Serial.println(msg);
}

// ════════════════════════════════════════════════════════════
//  BOOT ANIMATION
// ════════════════════════════════════════════════════════════
void bootAnimation() {
  int leds[] = {LED_GREEN, LED_BLUE, LED_YELLOW, LED_RED};
  for (int r = 0; r < 2; r++) {
    for (int i = 0; i < 4; i++) {
      esp_task_wdt_reset();
      digitalWrite(leds[i], HIGH); delay(80);
      digitalWrite(leds[i], LOW);
    }
  }
  esp_task_wdt_reset();
  for (int p = 0; p <= 90; p += 10) { warningServo.write(p); delay(15); }
  for (int p = 90; p >= 0; p -= 10) { warningServo.write(p); delay(15); }
  esp_task_wdt_reset();
  lcd.clear();
  lcd.print(F("WiFi:GeoSense-Afr"));
  lcd.setCursor(0, 1); lcd.print(F(" 192.168.4.1  "));
  delay(1500);
  esp_task_wdt_reset();
}
