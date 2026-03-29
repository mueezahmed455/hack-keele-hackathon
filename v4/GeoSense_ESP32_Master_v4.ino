// ================================================================
//  GEO-SENSE AFRICA v4.0 — ESP32 MASTER NODE
//
//  WHAT'S NEW IN v4.0
//  ─────────────────────────────────────────────────────────────
//  [FEATURE] Remote server HTTP POST (WiFi Station + AP dual mode)
//  [FEATURE] Unique node ID derived from ESP32 MAC address
//  [FEATURE] POST retry queue — stores last 3 failed payloads
//  [FEATURE] Server connection status on OLED page 3 + LCD
//  [FEATURE] POST rate-limited to every 10 s (not every sample)
//  [FEATURE] Non-blocking HTTP with 4 s connect timeout
//  [FEATURE] Exponential backoff on repeated POST failures
//  [FIX]     slopePct decay was running every ~1 ms loop tick
//            → moved to ESP32 side, time-gated to 500 ms
//  [FIX]     bootAnimation() blocked watchdog > 10 s on slow
//            hardware → wdt_reset inside every delay loop
//  [FIX]     logJson strncat built empty strings on fresh boot
//            → skip empty entries in log array builder
//  [FIX]     handleDataRequest buf[640] — added et field was
//            already there but "uptime" used %lu which is
//            platform-unsafe on ESP32 IDF; cast to uint32_t
//  [FIX]     data.tiltPct EMA fed into risk but ema_dist was
//            computed and stored but never used in formula —
//            removed dead ema_dist variable
//
//  ARCHITECTURE
//  ─────────────────────────────────────────────────────────────
//  WiFi Dual Mode:
//    AP  "GeoSense-Africa" → local dashboard at 192.168.4.1
//    STA → connects to team router → POSTs to remote server
//
//  Remote POST endpoint (change SERVER_IP and SERVER_PORT):
//    POST http://SERVER_IP:SERVER_PORT/api/ingest
//    Content-Type: application/json
//    Body: full sensor JSON with node_id and timestamp
//
//  Serial2 (UART2) to Arduino Slave:
//    RX GPIO16 ← Arduino TX D1  (via 1 kΩ + 2 kΩ divider!)
//    TX GPIO17 → Arduino RX D0
//    Baud: 9600 8N1
// ================================================================

// ── LIBRARIES ───────────────────────────────────────────────
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

// ── REQUIRED LIBRARIES (install via Library Manager) ────────
// Adafruit SSD1306 | Adafruit GFX Library | LiquidCrystal I2C
// DHT sensor library (Adafruit) | Adafruit Unified Sensor
// ESP32Servo | MD_MAX72XX
// Board Manager: esp32 by Espressif Systems → ESP32 Dev Module

// ════════════════════════════════════════════════════════════
//  CONFIGURATION  — edit these before flashing
// ════════════════════════════════════════════════════════════

// Local WiFi AP (for dashboard)
const char* AP_SSID      = "GeoSense-Africa";
const char* AP_PASSWORD  = "geosense2024";

// Team router WiFi (for reaching the private server)
// Leave empty strings if the server is on the same AP network
const char* STA_SSID     = "YOUR_ROUTER_SSID";
const char* STA_PASSWORD = "YOUR_ROUTER_PASSWORD";

// Remote server — change IP/port when team member is ready
// The ESP32 will POST to: http://SERVER_IP:SERVER_PORT/api/ingest
const char* SERVER_IP    = "192.168.1.100";  // ← CHANGE THIS
const uint16_t SERVER_PORT = 5000;           // ← CHANGE THIS
const char* SERVER_PATH  = "/api/ingest";

// POST interval and retry
#define POST_INTERVAL_MS   10000UL   // POST every 10 seconds
#define POST_TIMEOUT_MS     4000     // HTTP connect+read timeout
#define POST_RETRY_QUEUE     3       // Store up to 3 failed payloads
#define POST_BACKOFF_MAX  120000UL   // Max backoff: 2 minutes

// ════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ════════════════════════════════════════════════════════════

#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_DHT      4
#define DHT_TYPE    DHT11
#define PIN_TRIG     5    // HC-SR04 TRIG → OUTPUT
#define PIN_ECHO    18    // HC-SR04 ECHO → INPUT (1 kΩ/2 kΩ divider required)
#define PIN_PIR     19
#define PIN_SERVO   13
#define PIN_RELAY   12    // Active LOW
#define PIN_TOUCH   14
#define PIN_JOY_X   34    // ADC1 input-only
#define PIN_JOY_Y   35    // ADC1 input-only
#define PIN_JOY_BTN 32
// Dot matrix SPI — FC16_HW constructor = (type, CS, DATA, CLK, devices)
#define PIN_DM_CS   15
#define PIN_DM_DATA 23    // MOSI/DIN
#define PIN_DM_CLK  26    // SCK — GPIO26 avoids ECHO conflict with GPIO18
// Status LEDs
#define LED_GREEN   25
#define LED_BLUE     2
#define LED_YELLOW  27
#define LED_RED     33
// Analog sensors (ADC1 only pins)
#define PIN_SOIL    36
#define PIN_OBSTACLE 39
// Serial2 to Arduino slave
#define SLAVE_RX    16
#define SLAVE_TX    17

// ════════════════════════════════════════════════════════════
//  DISPLAY OBJECTS
// ════════════════════════════════════════════════════════════

#define OLED_W    128
#define OLED_H     64
#define OLED_ADDR 0x3C
#define LCD_ADDR  0x27

Adafruit_SSD1306  oled(OLED_W, OLED_H, &Wire, -1);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
// Correct FC16_HW arg order: (type, CS, DATA, CLK, devices)
MD_MAX72XX dotMatrix = MD_MAX72XX(MD_MAX72XX::FC16_HW,
                                  PIN_DM_CS, PIN_DM_DATA, PIN_DM_CLK, 1);
DHT    dht(PIN_DHT, DHT_TYPE);
Servo  warningServo;
WebServer server(80);

// ════════════════════════════════════════════════════════════
//  DATA STRUCTURES
// ════════════════════════════════════════════════════════════

struct SensorData {
  // ESP32-local
  float soilMoisture;        // 0–100 %
  float airTemp;             // °C
  float airHumidity;         // %
  float distanceCm;          // HC-SR04 cm (river distance)
  bool  pirMotion;
  bool  obstacleDetected;
  float evapotranspiration;  // Penman apparent temp °C
  // From Arduino slave via Serial2
  float waterLevel;          // 0–100 %
  float tiltPct;             // 0–100 % (spike+decay from ISR frequency)
  float thermistorTemp;      // soil °C
  float potCalibration;      // 0.50–2.00 ×
  float photoPct;            // 0–100 % ambient light / solar
  bool  tiltInterrupt;       // hardware ISR fired on slave
  // Computed
  float riskIndex;           // 0.0–9.0
  int   alertLevel;          // 0=safe 1=warn 2=critical 3=ISR-override
  char  hazardType[12];      // "NOMINAL" | "FLOOD" | "DROUGHT" | "LANDSLIDE"
};

// Safe initialisation defaults
SensorData data = {
  50.0f, 25.0f, 60.0f, 400.0f,   // soil, airT, airH, dist
  false, false, 0.0f,             // pir, obstacle, ET
  0.0f, 0.0f, 25.0f, 1.0f, 50.0f, false,  // slave fields
  0.0f, 0, "NOMINAL"             // risk, alertLevel, hazard
};

// ════════════════════════════════════════════════════════════
//  EMA SMOOTHING BUFFERS
// ════════════════════════════════════════════════════════════

float ema_soil  = 50.0f;
float ema_water = 0.0f;
float ema_tilt  = 0.0f;
float ema_temp  = 0.0f;
#define EMA_A 0.25f

// ════════════════════════════════════════════════════════════
//  TIMING
// ════════════════════════════════════════════════════════════

uint32_t lastSampleMs  = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastScrollMs  = 0;
uint32_t lastPostMs    = 0;
uint32_t lastDecayMs   = 0;   // FIX: time-gate slopePct decay
uint32_t bootMs        = 0;
uint32_t postBackoffMs = 10000UL;  // Starts at 10s, doubles on failure
uint32_t lastFailedMs  = 0;
int      postFailCount = 0;

#define SAMPLE_INTERVAL    3000UL
#define DISPLAY_INTERVAL    400UL
#define SCROLL_INTERVAL      90UL
#define DECAY_INTERVAL      500UL   // slopePct decay tick

// ════════════════════════════════════════════════════════════
//  RISK HISTORY (OLED sparkline)
// ════════════════════════════════════════════════════════════

#define HISTORY_LEN 60
float   riskHistory[HISTORY_LEN];
uint8_t historyIdx = 0;

// ════════════════════════════════════════════════════════════
//  ALERT LOG
// ════════════════════════════════════════════════════════════

#define LOG_SIZE 8
struct AlertEntry { uint32_t ts; char msg[40]; };
AlertEntry alertLog[LOG_SIZE];
uint8_t logHead = 0;

// ════════════════════════════════════════════════════════════
//  POST RETRY QUEUE
// ════════════════════════════════════════════════════════════

struct PostEntry { char payload[512]; bool pending; };
PostEntry postQueue[POST_RETRY_QUEUE];
uint8_t postQHead = 0;
uint8_t postQCount = 0;

// ════════════════════════════════════════════════════════════
//  SERVER STATUS
// ════════════════════════════════════════════════════════════

bool     serverReachable  = false;
uint32_t lastSuccessPostMs = 0;
char     nodeID[18]       = "00:00:00:00:00:00";  // Set from MAC

// ════════════════════════════════════════════════════════════
//  JOYSTICK MENU
// ════════════════════════════════════════════════════════════

int      menuPage  = 0;
uint32_t lastJoyMs = 0;
#define  MENU_PAGES       4
#define  JOY_DEBOUNCE_MS  350

// ════════════════════════════════════════════════════════════
//  SLAVE SERIAL BUFFER
// ════════════════════════════════════════════════════════════

#define SLAVE_BUF 80
char    slaveBuf[SLAVE_BUF];
uint8_t slaveIdx = 0;

// ════════════════════════════════════════════════════════════
//  ACKNOWLEDGEMENT
// ════════════════════════════════════════════════════════════

bool     alertAcked = false;
uint32_t ackedAt    = 0;
#define  ACK_TIMEOUT_MS   60000UL

// ════════════════════════════════════════════════════════════
//  GLOBAL ALERT COUNTER
// ════════════════════════════════════════════════════════════

int gAlertCount = 0;

// ════════════════════════════════════════════════════════════
//  DOT MATRIX
// ════════════════════════════════════════════════════════════

uint8_t dmScrollTick = 0;

// ════════════════════════════════════════════════════════════
//  WEB DASHBOARD (PROGMEM)
// ════════════════════════════════════════════════════════════

static const char DASHBOARD_HTML[] PROGMEM = R"rawHTML(
<!DOCTYPE html><html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GEO-SENSE Africa</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
:root{--bg:#0a0e14;--card:#111720;--bd:#1e2d45;--tx:#e8eaed;--sub:#6b7a93;--acc:#00ff88;--fl:#5bb8ff;--dr:#f5a623;--ls:#ff6b6b}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--tx);font-family:'Segoe UI',Arial,sans-serif;padding:14px}
.wrap{max-width:980px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--bd);padding-bottom:10px;margin-bottom:16px;flex-wrap:wrap;gap:8px}
h1{font-size:17px;color:var(--acc);letter-spacing:2px}
.hdr-right{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
.dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--acc);margin-right:6px;animation:blink 2s infinite}
.srv-dot{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:4px}
.srv-ok{background:#5dd882}.srv-err{background:#ff6b6b}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;margin-bottom:14px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:14px;transition:border-color .2s}
.card:hover{border-color:rgba(0,255,136,.3)}
.lbl{font-size:10px;font-weight:700;letter-spacing:1px;color:var(--sub);text-transform:uppercase;margin-bottom:6px;display:flex;justify-content:space-between;align-items:center}
.big{font-size:30px;font-weight:800;line-height:1;margin-bottom:4px}
.unit{font-size:13px;color:var(--sub);font-weight:400;margin-left:3px}
.row{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid rgba(255,255,255,.04);font-size:12px;color:var(--sub)}
.row:last-child{border:none}
.row span:last-child{color:var(--tx)}
.badge{padding:3px 8px;border-radius:4px;font-size:10px;font-weight:700;letter-spacing:.5px}
.g{background:rgba(0,255,136,.1);color:#5dd882;border:1px solid #5dd882}
.a{background:rgba(245,166,35,.1);color:var(--dr);border:1px solid var(--dr)}
.r{background:rgba(255,107,107,.1);color:var(--ls);border:1px solid var(--ls)}
.banner{grid-column:1/-1;background:rgba(255,107,107,.08);border:2px solid var(--ls);border-radius:10px;padding:14px;display:none}
.banner.on{display:block;animation:pulse 1.2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.45}}
.risk-bar-wrap{height:8px;background:var(--bd);border-radius:4px;margin-top:8px;overflow:hidden}
.risk-bar{height:100%;border-radius:4px;transition:width .6s,background .6s}
.log{font-family:'Courier New',monospace;font-size:10px;color:var(--sub);line-height:1.9;margin-top:4px}
.node-pill{background:rgba(0,255,136,.08);border:1px solid rgba(0,255,136,.25);border-radius:4px;padding:3px 8px;font-size:10px;font-family:monospace;color:var(--acc)}
footer{text-align:center;margin-top:20px;font-size:10px;color:var(--sub)}
</style>
</head>
<body>
<div class="wrap">
<header>
  <h1><span class="dot"></span>GEO-SENSE AFRICA v4.0</h1>
  <div class="hdr-right">
    <span id="node-lbl" class="node-pill">NODE --</span>
    <span id="srv-status" style="font-size:10px;color:var(--sub)"><span class="srv-dot srv-err" id="srv-dot"></span>Server --</span>
    <span id="ts" style="font-size:10px;color:var(--sub)">connecting...</span>
  </div>
</header>
<div class="grid">

  <div id="alert-banner" class="banner">
    <strong style="color:var(--ls);font-size:14px">CRITICAL ALERT</strong>
    <span id="alert-msg"> — standby</span>
  </div>

  <div class="card">
    <div class="lbl">Risk index <span id="h-badge" class="badge g">NOMINAL</span></div>
    <div class="big" id="ri">0<span class="unit">/9</span></div>
    <div class="risk-bar-wrap"><div id="rb" class="risk-bar" style="width:0%;background:#5dd882"></div></div>
    <div style="height:150px;margin-top:12px"><canvas id="rc"></canvas></div>
  </div>

  <div class="card">
    <div class="lbl">Flood monitor</div>
    <div class="big" id="wv">0<span class="unit">% level</span></div>
    <div class="row"><span>River distance</span><span id="dv">--</span></div>
    <div class="row"><span>Debris / obstacle</span><span id="ov">Clear</span></div>
    <div class="row"><span>Gate servo</span><span id="sv">0°</span></div>
    <div class="row"><span>PIR motion</span><span id="pv">None</span></div>
    <div style="height:100px;margin-top:8px"><canvas id="fc"></canvas></div>
  </div>

  <div class="card">
    <div class="lbl">Drought & agriculture</div>
    <div class="big" id="smv">0<span class="unit">% moisture</span></div>
    <div class="row"><span>Air temperature</span><span id="atv">--</span></div>
    <div class="row"><span>Air humidity</span><span id="ahv">--</span></div>
    <div class="row"><span>Soil temperature</span><span id="stv">--</span></div>
    <div class="row"><span>Evapotranspiration</span><span id="etv">--</span></div>
    <div class="row"><span>Solar / light</span><span id="phtv">--</span></div>
    <div style="height:90px;margin-top:8px"><canvas id="sc"></canvas></div>
  </div>

  <div class="card">
    <div class="lbl">Geo-stability & system</div>
    <div class="big" id="tv">0<span class="unit">% slope</span></div>
    <div class="row"><span>Tilt ISR</span><span id="lsv">OK</span></div>
    <div class="row"><span>Calibration</span><span id="potv">1.00×</span></div>
    <div class="row"><span>Relay / siren</span><span id="relv">Off</span></div>
    <div class="row"><span>Alert count</span><span id="acv">0</span></div>
    <div class="row"><span>Uptime</span><span id="upv">--</span></div>
    <div class="row"><span>Posts sent / failed</span><span id="postv">--</span></div>
    <div class="lbl" style="margin-top:10px">Event log</div>
    <div class="log" id="logv">—</div>
  </div>

</div>
<footer>GEO-SENSE AFRICA v4.0 &bull; AP: GeoSense-Africa / geosense2024 &bull; Dashboard: 192.168.4.1</footer>
</div>

<script>
const mkChart = (id, type, len, color, min, max) => {
  const data = Array(len).fill(0);
  const ctx  = document.getElementById(id).getContext('2d');
  const chart = new Chart(ctx, {
    type,
    data: { labels: Array(len).fill(''), datasets: [{ data, borderColor: color, backgroundColor: color + '22', tension: 0.4, fill: true, pointRadius: 0, borderWidth: 1.5 }] },
    options: { responsive: true, maintainAspectRatio: false, animation: { duration: 250 },
      scales: { y: { min, max, grid: { color: '#1e2d45' }, ticks: { color: '#6b7a93', font: { size: 9 } } }, x: { display: false } },
      plugins: { legend: { display: false } } }
  });
  return { chart, data };
};

const rc = mkChart('rc', 'line', 30, '#00ff88', 0, 9);
const fc = mkChart('fc', 'bar',  20, '#5bb8ff', 0, 100);
const sc = mkChart('sc', 'line', 20, '#f5a623', 0, 100);

let totalPosts = 0, failedPosts = 0;

const push = (obj, val) => { obj.data.push(val); obj.data.shift(); obj.chart.update('none'); };
const col  = r => r > 6 ? '#ff6b6b' : r > 3 ? '#f5a623' : '#00ff88';
const flag = (id, cond, yes, no, yc, nc) => { const el = document.getElementById(id); el.textContent = cond ? yes : no; el.style.color = cond ? yc : nc; };
const fmtUp = s => s > 3600 ? `${Math.floor(s/3600)}h ${Math.floor(s/60)%60}m` : s > 60 ? `${Math.floor(s/60)}m ${s%60}s` : `${s}s`;

async function poll() {
  try {
    const d = await fetch('/data').then(r => r.json());
    const ri = parseFloat(d.risk.toFixed(2));
    const rp = ((ri / 9) * 100).toFixed(1);

    document.getElementById('ri').innerHTML = ri.toFixed(1) + '<span class="unit">/9</span>';
    const rb = document.getElementById('rb');
    rb.style.width = rp + '%'; rb.style.background = col(ri);

    const hb = document.getElementById('h-badge');
    if      (ri > 6) { hb.className = 'badge r'; hb.textContent = 'CRITICAL'; }
    else if (ri > 3) { hb.className = 'badge a'; hb.textContent = 'WARNING'; }
    else             { hb.className = 'badge g'; hb.textContent = 'NOMINAL'; }

    const banner = document.getElementById('alert-banner');
    if (d.lsi || ri > 7) {
      banner.classList.add('on');
      document.getElementById('alert-msg').textContent = d.lsi
        ? ' — LANDSLIDE SENSOR TRIGGERED! EVACUATE IMMEDIATELY.'
        : ' — HIGH RISK: ' + d.hazard + '. TAKE IMMEDIATE ACTION.';
    } else banner.classList.remove('on');

    document.getElementById('wv').innerHTML  = (+d.water).toFixed(1) + '<span class="unit">% level</span>';
    document.getElementById('dv').textContent = (+d.dist).toFixed(1) + ' cm';
    flag('ov', d.obs,  'DEBRIS DETECTED', 'Clear',  '#f5a623', '#5dd882');
    document.getElementById('sv').textContent = d.servoPos + '°';
    flag('pv', d.pir,  'MOTION',          'None',   '#ff6b6b', '#6b7a93');

    document.getElementById('smv').innerHTML = (+d.soil).toFixed(1) + '<span class="unit">% moisture</span>';
    document.getElementById('atv').textContent = (+d.airT).toFixed(1) + '°C';
    document.getElementById('ahv').textContent = (+d.airH).toFixed(1) + '%';
    document.getElementById('stv').textContent = (+d.soilT).toFixed(1) + '°C';
    document.getElementById('etv').textContent = (+d.et).toFixed(2) + ' AT';
    document.getElementById('phtv').textContent = (+d.photo).toFixed(0) + '%';

    document.getElementById('tv').innerHTML = (+d.tilt).toFixed(1) + '<span class="unit">% slope</span>';
    flag('lsv',  d.lsi,       'TRIGGERED!', 'OK',   '#ff6b6b', '#5dd882');
    document.getElementById('potv').textContent  = (+d.pot).toFixed(2) + '×';
    flag('relv', d.relay,     'ACTIVE',     'Off',  '#ff6b6b', '#6b7a93');
    document.getElementById('acv').textContent   = d.alertCount;
    document.getElementById('upv').textContent   = fmtUp(+d.uptime);

    // Server status
    document.getElementById('node-lbl').textContent = 'NODE ' + (d.nodeID || '--');
    const srvOk = d.serverOk;
    const dot = document.getElementById('srv-dot');
    dot.className = 'srv-dot ' + (srvOk ? 'srv-ok' : 'srv-err');
    document.getElementById('srv-status').innerHTML =
      '<span class="srv-dot ' + (srvOk ? 'srv-ok' : 'srv-err') + '" id="srv-dot"></span>' +
      (srvOk ? 'Server OK ' + d.lastPost + 's ago' : 'Server unreachable');
    totalPosts  = +d.postsSent   || 0;
    failedPosts = +d.postsFailed || 0;
    document.getElementById('postv').textContent = totalPosts + ' / ' + failedPosts;

    // Log
    if (d.log && d.log.length) {
      document.getElementById('logv').innerHTML = d.log
        .filter(e => e && e.length > 0)
        .map(e => '› ' + e)
        .join('<br>');
    }

    push(rc, ri);
    push(fc, +d.water);
    push(sc, +d.soil);
    document.getElementById('ts').textContent = 'Updated ' + new Date().toLocaleTimeString();
  } catch(e) {
    document.getElementById('ts').textContent = 'Reconnecting...';
  }
}

poll();
setInterval(poll, 2500);
</script>
</body></html>
)rawHTML";

// ════════════════════════════════════════════════════════════
//  GLOBAL COUNTERS FOR POST TELEMETRY
// ════════════════════════════════════════════════════════════

uint32_t gPostsSent   = 0;
uint32_t gPostsFailed = 0;

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
void buildPayload(char* buf, size_t bufLen);
bool postToServer(const char* payload);
void drainPostQueue();
void enqueuePost(const char* payload);
void decaySlope();

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);

  // Watchdog — 10 s reset
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  Wire.begin(PIN_SDA, PIN_SCL);

  // Output pins
  pinMode(PIN_TRIG,   OUTPUT); digitalWrite(PIN_TRIG,  LOW);
  pinMode(PIN_RELAY,  OUTPUT); digitalWrite(PIN_RELAY, HIGH);  // OFF
  pinMode(LED_GREEN,  OUTPUT); digitalWrite(LED_GREEN,  LOW);
  pinMode(LED_BLUE,   OUTPUT); digitalWrite(LED_BLUE,   LOW);
  pinMode(LED_YELLOW, OUTPUT); digitalWrite(LED_YELLOW, LOW);
  pinMode(LED_RED,    OUTPUT); digitalWrite(LED_RED,    LOW);

  // Input pins
  pinMode(PIN_PIR,     INPUT);
  pinMode(PIN_TOUCH,   INPUT);
  pinMode(PIN_JOY_BTN, INPUT_PULLUP);

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED not found — check address (try 0x3D)"));
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.println(F("GEO-SENSE AFRICA"));
  oled.setCursor(0, 10); oled.println(F("v4.0 BOOTING..."));
  oled.display();

  // LCD
  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.print(F("GEO-SENSE v4.0  "));
  lcd.setCursor(0, 1); lcd.print(F("  BOOTING...    "));

  // DHT
  dht.begin();

  // Servo
  warningServo.attach(PIN_SERVO);
  warningServo.write(0);

  // Dot matrix
  dotMatrix.begin();
  dotMatrix.control(MD_MAX72XX::INTENSITY, 3);
  dotMatrix.clear();

  // ── WIFI: Start AP first, then STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print(F("AP IP: ")); Serial.println(WiFi.softAPIP());

  // Connect to router if credentials provided
  if (strlen(STA_SSID) > 0) {
    WiFi.begin(STA_SSID, STA_PASSWORD);
    Serial.print(F("Connecting to router"));
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      esp_task_wdt_reset();
      delay(500);
      Serial.print('.');
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("\nSTA IP: ")); Serial.println(WiFi.localIP());
      addLog("WiFi STA connected OK");
    } else {
      Serial.println(F("\nSTA connect failed — AP-only mode"));
      addLog("WiFi STA failed — AP only");
    }
  }

  // Derive node ID from MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(nodeID, sizeof(nodeID), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(F("Node ID: ")); Serial.println(nodeID);

  // Web routes
  server.on("/",      []() { server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data",  handleDataRequest);
  server.on("/log",   handleLogRequest);
  server.on("/reset", handleResetRequest);
  server.begin();

  // Init ring buffers
  memset(riskHistory, 0, sizeof(riskHistory));
  for (int i = 0; i < POST_RETRY_QUEUE; i++) postQueue[i].pending = false;

  bootMs = millis();
  bootAnimation();
  addLog("GEO-SENSE v4.0 boot OK");
  Serial.println(F("GEO-SENSE AFRICA v4.0 — ONLINE"));
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  uint32_t now = millis();

  // ── Slave serial (char-by-char, non-blocking) ──────────
  while (Serial2.available()) handleSlaveSerial();

  // ── slopePct decay (time-gated, FIX) ──────────────────
  if (now - lastDecayMs >= DECAY_INTERVAL) {
    lastDecayMs = now;
    decaySlope();
  }

  // ── Sample sensors + compute + output ──────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readLocalSensors();
    computeRisk();
    updateOutputs();
    char cmd[12];
    snprintf(cmd, sizeof(cmd), "CMD:%d\n", data.alertLevel);
    Serial2.print(cmd);
  }

  // ── Remote POST (rate-limited + retry queue) ───────────
  uint32_t effectiveInterval = min(postBackoffMs, POST_BACKOFF_MAX);
  if (now - lastPostMs >= effectiveInterval) {
    lastPostMs = now;
    char payload[512];
    buildPayload(payload, sizeof(payload));
    if (postToServer(payload)) {
      gPostsSent++;
      postBackoffMs = POST_INTERVAL_MS;  // Reset backoff on success
      lastSuccessPostMs = now;
      serverReachable = true;
    } else {
      gPostsFailed++;
      postBackoffMs = min(postBackoffMs * 2, POST_BACKOFF_MAX);  // Exponential backoff
      serverReachable = false;
      enqueuePost(payload);  // Store for retry
    }
    // Drain retry queue on the next successful network window
    if (serverReachable) drainPostQueue();
  }

  // ── Displays ───────────────────────────────────────────
  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    handleJoystick();
    updateOLED();
    updateLCD();
  }

  // ── Dot matrix ─────────────────────────────────────────
  if (now - lastScrollMs >= SCROLL_INTERVAL) {
    lastScrollMs = now;
    scrollDotMatrix();
  }

  // ── Touch acknowledge ──────────────────────────────────
  if (digitalRead(PIN_TOUCH) == HIGH) {
    if (!alertAcked) {
      alertAcked = true;
      ackedAt    = now;
      digitalWrite(PIN_RELAY, HIGH);
      warningServo.write(0);
      addLog("Alert ACK via touch");
      lcd.clear(); lcd.print(F("Alert ACK'd     "));
    }
  }
  if (alertAcked && (now - ackedAt > ACK_TIMEOUT_MS)) alertAcked = false;
}

// ════════════════════════════════════════════════════════════
//  READ LOCAL SENSORS
// ════════════════════════════════════════════════════════════

void readLocalSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) data.airTemp     = t;
  if (!isnan(h)) data.airHumidity = h;

  // Capacitive soil sensor (12-bit ADC, 3.3 V)
  // ~3200 = completely dry, ~1200 = saturated water
  int rawSoil = analogRead(PIN_SOIL);
  data.soilMoisture = constrain(
    (float)map(rawSoil, 3200, 1200, 0, 100), 0.0f, 100.0f);

  // HC-SR04 ultrasound
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 25000UL);
  // cm = µs * speed_of_sound / 2 = µs * 0.01715 cm/µs
  data.distanceCm = (dur == 0) ? 400.0f : (float)dur * 0.01715f;

  data.pirMotion        = digitalRead(PIN_PIR);
  data.obstacleDetected = (digitalRead(PIN_OBSTACLE) == LOW);

  // Penman apparent temperature (evapotranspiration proxy)
  // AT = T + 0.33 * e - 4.00 where e = vapour pressure in kPa
  float e = (data.airHumidity / 100.0f) *
            0.6108f * expf(17.27f * data.airTemp / (data.airTemp + 237.3f));
  data.evapotranspiration = data.airTemp + (0.33f * e) - 4.0f;
}

// ════════════════════════════════════════════════════════════
//  SLAVE SERIAL (safe character-by-character parser)
// ════════════════════════════════════════════════════════════

void handleSlaveSerial() {
  char c = (char)Serial2.read();
  if (c == '\n' || c == '\r') {
    if (slaveIdx > 0) {
      slaveBuf[slaveIdx] = '\0';
      parseSlaveData(slaveBuf);
    }
    slaveIdx = 0;
  } else if (slaveIdx < SLAVE_BUF - 1) {
    slaveBuf[slaveIdx++] = c;
  } else {
    slaveIdx = 0;  // Buffer overflow — discard corrupt frame
  }
}

void parseSlaveData(char* line) {
  // Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x
  auto getF = [&](const char* key) -> float {
    char* p = strstr(line, key);
    return p ? atof(p + strlen(key)) : -999.0f;
  };
  float w = getF("W:"); if (w > -999.0f) data.waterLevel     = constrain(w, 0.0f, 100.0f);
  float t = getF("T:"); if (t > -999.0f) data.thermistorTemp = constrain(t, -40.0f, 125.0f);
  float s = getF("S:"); if (s > -999.0f) data.tiltPct        = constrain(s, 0.0f, 100.0f);
  float p = getF("P:"); if (p > -999.0f) data.potCalibration = constrain(p, 0.5f,  2.0f);
  float l = getF("L:"); if (l > -999.0f) data.photoPct       = constrain(l, 0.0f, 100.0f);
  char* ip = strstr(line, "I:");
  if (ip) {
    bool newTilt = (atoi(ip + 2) == 1);
    if (newTilt && !data.tiltInterrupt) addLog("!! TILT ISR from slave");
    data.tiltInterrupt = newTilt;
  }
}

// ════════════════════════════════════════════════════════════
//  SLOPE DECAY (time-gated — FIX from v3)
// ════════════════════════════════════════════════════════════

void decaySlope() {
  // data.tiltPct comes from slave (slopePct there)
  // Additional ESP32-side gentle decay if ISR not currently active
  if (!data.tiltInterrupt && data.tiltPct > 0.0f) {
    data.tiltPct = max(0.0f, data.tiltPct - 1.5f);
  }
}

// ════════════════════════════════════════════════════════════
//  COMPUTE RISK
// ════════════════════════════════════════════════════════════

void computeRisk() {
  // Absolute override: hardware tilt ISR
  if (data.tiltInterrupt) {
    data.riskIndex = 9.0f;
    data.alertLevel = 3;
    strncpy(data.hazardType, "LANDSLIDE", sizeof(data.hazardType));
    data.hazardType[sizeof(data.hazardType) - 1] = '\0';
    gAlertCount = 99;
    riskHistory[historyIdx % HISTORY_LEN] = 9.0f;
    historyIdx++;
    return;
  }

  // EMA smoothing
  float floodPct = constrain(
    (float)map((int)data.distanceCm, 200, 20, 0, 100), 0.0f, 100.0f);

  ema_soil  = EMA_A * data.soilMoisture + (1.0f - EMA_A) * ema_soil;
  ema_water = EMA_A * max(floodPct, data.waterLevel) + (1.0f - EMA_A) * ema_water;
  ema_tilt  = EMA_A * data.tiltPct + (1.0f - EMA_A) * ema_tilt;
  // Temp delta (air vs soil) normalised to 0–100
  float tempDeltaNorm = constrain(
    fabsf(data.airTemp - data.thermistorTemp) / 15.0f * 100.0f, 0.0f, 100.0f);
  ema_temp  = EMA_A * tempDeltaNorm + (1.0f - EMA_A) * ema_temp;

  // Weighted fusion
  float w = (ema_soil  * 0.25f) +
            (ema_water * 0.40f) +
            (ema_tilt  * 0.25f) +
            (ema_temp  * 0.10f);

  // Bonus: compound events
  if (data.pirMotion && ema_water > 40.0f)        w += 10.0f;
  if (data.obstacleDetected && ema_water > 25.0f) w +=  8.0f;
  if (data.soilMoisture < 20.0f && data.airTemp > 35.0f) w += 12.0f;  // Compound drought-heat

  // Terrain calibration
  float cal = (data.potCalibration >= 0.5f && data.potCalibration <= 2.0f)
              ? data.potCalibration : 1.0f;
  w = constrain(w * cal, 0.0f, 100.0f);

  data.riskIndex = (w / 100.0f) * 9.0f;

  // Hazard classification
  bool floodD   = (ema_water > 60.0f && data.distanceCm < 100.0f);
  bool droughtD = (ema_soil  > 65.0f && ema_water < 20.0f && data.airTemp > 28.0f);
  bool slopeD   = (ema_tilt  > 50.0f);

  if      (slopeD)   strncpy(data.hazardType, "LANDSLIDE", sizeof(data.hazardType));
  else if (floodD)   strncpy(data.hazardType, "FLOOD",     sizeof(data.hazardType));
  else if (droughtD) strncpy(data.hazardType, "DROUGHT",   sizeof(data.hazardType));
  else               strncpy(data.hazardType, "NOMINAL",   sizeof(data.hazardType));
  data.hazardType[sizeof(data.hazardType) - 1] = '\0';

  // Alert level with hysteresis (3 consecutive readings)
  int newLevel = (data.riskIndex > 6.0f) ? 2 : (data.riskIndex > 3.0f) ? 1 : 0;
  if (newLevel >= 2) gAlertCount++;
  else gAlertCount = max(0, gAlertCount - 1);

  data.alertLevel = (newLevel == 2 && gAlertCount >= 3) ? 2
                  : (newLevel == 1) ? 1 : 0;

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

  bool isFlood     = (strncmp(data.hazardType, "FLOOD",     5) == 0);
  bool isDrought   = (strncmp(data.hazardType, "DROUGHT",   7) == 0);
  bool isLandslide = (strncmp(data.hazardType, "LANDSLIDE", 9) == 0);

  digitalWrite(LED_GREEN,  (data.alertLevel == 0) ? HIGH : LOW);
  digitalWrite(LED_BLUE,   isFlood              ? HIGH : LOW);
  digitalWrite(LED_YELLOW, isDrought            ? HIGH : LOW);
  digitalWrite(LED_RED,    (critical || isLandslide) ? HIGH : LOW);

  digitalWrite(PIN_RELAY, critical ? LOW : HIGH);

  int servoPos = 0;
  if      (data.tiltInterrupt)   servoPos = 180;
  else if (data.alertLevel == 2) servoPos = 135;
  else if (data.alertLevel == 1) servoPos = 80;
  warningServo.write(servoPos);
}

// ════════════════════════════════════════════════════════════
//  OLED DISPLAY
// ════════════════════════════════════════════════════════════

void updateOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  // Page indicator dots
  for (int i = 0; i < MENU_PAGES; i++) {
    oled.fillCircle(54 + i * 8, 62, (i == menuPage) ? 2 : 1, SSD1306_WHITE);
  }
  char buf[32];

  switch (menuPage) {
    case 0: {
      oled.setCursor(0, 0); oled.print(data.hazardType);
      oled.setTextSize(3); oled.setCursor(0, 14);
      snprintf(buf, sizeof(buf), "%d", (int)data.riskIndex);
      oled.print(buf);
      oled.setTextSize(1); oled.setCursor(30, 28); oled.print("/9");
      oled.drawRect(50, 16, 76, 10, SSD1306_WHITE);
      oled.fillRect(51, 17, (int)(data.riskIndex / 9.0f * 74.0f), 8, SSD1306_WHITE);
      for (int i = 1; i < 40; i++) {
        int i0 = (historyIdx + i - 1) % HISTORY_LEN;
        int i1 = (historyIdx + i)     % HISTORY_LEN;
        int x0 = i * 3 + 2;
        int y0 = 52 - (int)(riskHistory[i0] / 9.0f * 14.0f);
        int y1 = 52 - (int)(riskHistory[i1] / 9.0f * 14.0f);
        oled.drawLine(x0, y0, x0 + 3, y1, SSD1306_WHITE);
      }
    } break;

    case 1: {
      oled.setCursor(0, 0);  oled.print(F("FLOOD"));
      snprintf(buf, sizeof(buf), "Dist:  %.1fcm", data.distanceCm);
      oled.setCursor(0, 12); oled.print(buf);
      snprintf(buf, sizeof(buf), "Water: %.1f%%", data.waterLevel);
      oled.setCursor(0, 22); oled.print(buf);
      oled.setCursor(0, 32); oled.print(F("Debris:")); oled.print(data.obstacleDetected ? "YES!" : "No");
      oled.setCursor(0, 42); oled.print(F("PIR:   ")); oled.print(data.pirMotion ? "MOTION!" : "None");
      snprintf(buf, sizeof(buf), "Gate:  %ddeg",  warningServo.read());
      oled.setCursor(0, 52); oled.print(buf);
    } break;

    case 2: {
      oled.setCursor(0, 0);  oled.print(F("DROUGHT"));
      snprintf(buf, sizeof(buf), "Soil:  %.1f%%",  data.soilMoisture);
      oled.setCursor(0, 12); oled.print(buf);
      snprintf(buf, sizeof(buf), "AirT:  %.1fC", data.airTemp);
      oled.setCursor(0, 22); oled.print(buf);
      snprintf(buf, sizeof(buf), "SoilT: %.1fC", data.thermistorTemp);
      oled.setCursor(0, 32); oled.print(buf);
      snprintf(buf, sizeof(buf), "ET:    %.2f",  data.evapotranspiration);
      oled.setCursor(0, 42); oled.print(buf);
      snprintf(buf, sizeof(buf), "Solar: %.0f%%", data.photoPct);
      oled.setCursor(0, 52); oled.print(buf);
    } break;

    case 3: {
      oled.setCursor(0, 0);  oled.print(F("SYSTEM"));
      // Server status
      oled.setCursor(0, 10);
      oled.print(serverReachable ? F("Srv: OK ") : F("Srv: FAIL"));
      if (serverReachable) {
        snprintf(buf, sizeof(buf), "%lus ago", (unsigned long)((millis() - lastSuccessPostMs) / 1000));
        oled.print(buf);
      }
      snprintf(buf, sizeof(buf), "Posts: %lu/%lu", gPostsSent, gPostsFailed);
      oled.setCursor(0, 20); oled.print(buf);
      snprintf(buf, sizeof(buf), "Cal:   %.2fx", data.potCalibration);
      oled.setCursor(0, 30); oled.print(buf);
      uint32_t up = (millis() - bootMs) / 1000;
      snprintf(buf, sizeof(buf), "Up:    %lus", (unsigned long)up);
      oled.setCursor(0, 40); oled.print(buf);
      oled.setCursor(0, 50); oled.print(F("Touch=ACK Joy=Menu"));
    } break;
  }
  oled.display();
}

// ════════════════════════════════════════════════════════════
//  LCD DISPLAY
// ════════════════════════════════════════════════════════════

void updateLCD() {
  char r0[17], r1[17];
  // Row 0: hazard type + risk index
  snprintf(r0, sizeof(r0), "%-8s  R:%d/9", data.hazardType, (int)data.riskIndex);
  // Row 1: water, soil, server status indicator
  snprintf(r1, sizeof(r1), "W:%.0f%% S:%.0f%% %s",
           data.waterLevel, data.soilMoisture,
           serverReachable ? "SRV:OK " : "SRV:ERR");
  lcd.setCursor(0, 0); lcd.print(r0);
  lcd.setCursor(0, 1); lcd.print(r1);
}

// ════════════════════════════════════════════════════════════
//  JOYSTICK
// ════════════════════════════════════════════════════════════

void handleJoystick() {
  uint32_t now = millis();
  if (now - lastJoyMs < JOY_DEBOUNCE_MS) return;
  int x = analogRead(PIN_JOY_X);
  if      (x < 1000) { menuPage = (menuPage + 1) % MENU_PAGES;               lastJoyMs = now; }
  else if (x > 3000) { menuPage = (menuPage - 1 + MENU_PAGES) % MENU_PAGES;  lastJoyMs = now; }
}

// ════════════════════════════════════════════════════════════
//  DOT MATRIX
// ════════════════════════════════════════════════════════════

void scrollDotMatrix() {
  dotMatrix.transform(MD_MAX72XX::TSL);
  dmScrollTick++;
  if (dmScrollTick % 8 == 0) {
    uint8_t col = (data.alertLevel == 0) ? 0x18
                : (data.alertLevel == 1) ? 0x3C
                : 0xFF;
    dotMatrix.setColumn(0, 0, col);
  }
}

// ════════════════════════════════════════════════════════════
//  REMOTE SERVER: BUILD JSON PAYLOAD
// ════════════════════════════════════════════════════════════

void buildPayload(char* buf, size_t bufLen) {
  uint32_t uptime = (millis() - bootMs) / 1000;
  snprintf(buf, bufLen,
    "{"
    "\"node_id\":\"%s\","
    "\"uptime\":%lu,"
    "\"risk\":%.2f,"
    "\"hazard\":\"%s\","
    "\"alert_level\":%d,"
    "\"soil\":%.1f,"
    "\"water\":%.1f,"
    "\"dist\":%.1f,"
    "\"air_temp\":%.1f,"
    "\"air_hum\":%.1f,"
    "\"soil_temp\":%.1f,"
    "\"et\":%.2f,"
    "\"tilt\":%.1f,"
    "\"photo\":%.1f,"
    "\"pir\":%s,"
    "\"obs\":%s,"
    "\"tilt_isr\":%s,"
    "\"relay\":%s,"
    "\"servo\":%d,"
    "\"pot\":%.2f"
    "}",
    nodeID,
    (unsigned long)uptime,
    data.riskIndex,
    data.hazardType,
    data.alertLevel,
    data.soilMoisture,
    data.waterLevel,
    data.distanceCm,
    data.airTemp,
    data.airHumidity,
    data.thermistorTemp,
    data.evapotranspiration,
    data.tiltPct,
    data.photoPct,
    data.pirMotion        ? "true" : "false",
    data.obstacleDetected ? "true" : "false",
    data.tiltInterrupt    ? "true" : "false",
    (digitalRead(PIN_RELAY) == LOW) ? "true" : "false",
    warningServo.read(),
    data.potCalibration
  );
}

// ════════════════════════════════════════════════════════════
//  REMOTE SERVER: POST (non-blocking, 4 s timeout)
// ════════════════════════════════════════════════════════════

bool postToServer(const char* payload) {
  // Only attempt if we have STA connectivity
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

  if (code >= 200 && code < 300) {
    return true;
  }
  // code < 0 means connection failed; code ≥ 400 means server error
  return false;
}

// ════════════════════════════════════════════════════════════
//  POST RETRY QUEUE
// ════════════════════════════════════════════════════════════

void enqueuePost(const char* payload) {
  // Overwrite oldest if full (ring buffer)
  strncpy(postQueue[postQHead].payload, payload,
          sizeof(postQueue[postQHead].payload) - 1);
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
      postQueue[i].pending = false;
      gPostsSent++;
      if (postQCount > 0) postQCount--;
    } else {
      break;  // Don't hammer server — try again next cycle
    }
  }
}

// ════════════════════════════════════════════════════════════
//  WEB ENDPOINTS
// ════════════════════════════════════════════════════════════

void handleDataRequest() {
  // Build log JSON — skip empty entries (FIX from v3)
  char logJson[300] = "[";
  int added = 0;
  for (int i = 0; i < LOG_SIZE && added < 4; i++) {
    int idx = ((int)logHead - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (alertLog[idx].msg[0] == '\0') continue;  // FIX: skip empty
    if (added > 0) strncat(logJson, ",", sizeof(logJson) - strlen(logJson) - 1);
    char entry[52];
    snprintf(entry, sizeof(entry), "\"%s\"", alertLog[idx].msg);
    strncat(logJson, entry, sizeof(logJson) - strlen(logJson) - 1);
    added++;
  }
  strncat(logJson, "]", sizeof(logJson) - strlen(logJson) - 1);

  uint32_t secSincePost = serverReachable
    ? (uint32_t)((millis() - lastSuccessPostMs) / 1000) : 9999;

  char buf[768];
  snprintf(buf, sizeof(buf),
    "{"
    "\"risk\":%.2f,\"hazard\":\"%s\","
    "\"dist\":%.1f,\"water\":%.1f,\"soil\":%.1f,"
    "\"airT\":%.1f,\"airH\":%.1f,\"soilT\":%.1f,"
    "\"et\":%.2f,\"tilt\":%.1f,\"pot\":%.2f,\"photo\":%.1f,"
    "\"pir\":%s,\"obs\":%s,\"lsi\":%s,\"relay\":%s,"
    "\"servoPos\":%d,\"alertCount\":%d,"
    "\"uptime\":%lu,"
    "\"nodeID\":\"%s\","
    "\"serverOk\":%s,\"lastPost\":%lu,"
    "\"postsSent\":%lu,\"postsFailed\":%lu,"
    "\"log\":%s"
    "}",
    data.riskIndex, data.hazardType,
    data.distanceCm, data.waterLevel, data.soilMoisture,
    data.airTemp, data.airHumidity, data.thermistorTemp,
    data.evapotranspiration, data.tiltPct,
    data.potCalibration, data.photoPct,
    data.pirMotion        ? "true" : "false",
    data.obstacleDetected ? "true" : "false",
    data.tiltInterrupt    ? "true" : "false",
    (digitalRead(PIN_RELAY) == LOW) ? "true" : "false",
    warningServo.read(), gAlertCount,
    (unsigned long)((millis() - bootMs) / 1000),
    nodeID,
    serverReachable ? "true" : "false",
    (unsigned long)secSincePost,
    (unsigned long)gPostsSent,
    (unsigned long)gPostsFailed,
    logJson
  );
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
  gAlertCount = 0;
  alertAcked  = false;
  server.send(200, "text/plain", "OK");
  addLog("Counter reset via /reset");
}

// ════════════════════════════════════════════════════════════
//  ALERT LOG
// ════════════════════════════════════════════════════════════

void addLog(const char* msg) {
  alertLog[logHead].ts = (millis() - bootMs) / 1000;
  strncpy(alertLog[logHead].msg, msg, sizeof(alertLog[logHead].msg) - 1);
  alertLog[logHead].msg[sizeof(alertLog[logHead].msg) - 1] = '\0';
  logHead = (logHead + 1) % LOG_SIZE;
  Serial.print(F("[LOG] ")); Serial.println(msg);
}

// ════════════════════════════════════════════════════════════
//  BOOT ANIMATION (wdt-safe)
// ════════════════════════════════════════════════════════════

void bootAnimation() {
  int leds[] = { LED_GREEN, LED_BLUE, LED_YELLOW, LED_RED };
  for (int r = 0; r < 2; r++) {
    for (int i = 0; i < 4; i++) {
      esp_task_wdt_reset();   // FIX: reset WDT inside animation
      digitalWrite(leds[i], HIGH); delay(80);
      digitalWrite(leds[i], LOW);
    }
  }
  esp_task_wdt_reset();
  for (int p = 0; p <= 90; p += 10)  { warningServo.write(p); delay(15); }
  for (int p = 90; p >= 0; p -= 10)  { warningServo.write(p); delay(15); }
  esp_task_wdt_reset();
  lcd.clear();
  lcd.print(F("WiFi:GeoSense-Afr"));
  lcd.setCursor(0, 1); lcd.print(F("  192.168.4.1   "));
  delay(1500);
  esp_task_wdt_reset();
}
