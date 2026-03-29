// ================================================================
//  GEO-SENSE AFRICA v4.5 — ESP32 MASTER NODE
//  Professional UI & Remote Command Center Refactor
// ================================================================
//  NEW FEATURES:
//  - Remote Control: Toggle Relay & Move Servo from Web
//  - System Diagnostics: RSSI, Free Heap, Uptime monitoring
//  - Configurable Thresholds: Dynamic alert triggers via Web
//  - Heartbeat & Auto-Sync: Seamless data flow
// ================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <esp_task_wdt.h>

// ── CONFIG ───────────────────────────────────────────────────
const char* AP_SSID = "GeoSense-Africa";
const char* AP_PASSWORD = "geosense2024";

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_DHT 4
#define PIN_TRIG 5
#define PIN_ECHO 18
#define PIN_PIR 19
#define PIN_SOIL 36
#define PIN_OBSTACLE 39
#define PIN_SERVO 13
#define PIN_RELAY 12
#define PIN_TOUCH 14
#define LED_GREEN 25
#define LED_BLUE 2
#define LED_YELLOW 27
#define LED_RED 33
#define SLAVE_RX 16
#define SLAVE_TX 17

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHT11);
Servo servo;
WebServer server(80);

// ── EXTENDED DATA STRUCTURE ──────────────────────────────────
struct Config {
  float thresholdWater = 60.0f;
  float thresholdSoil = 70.0f;
  float thresholdTilt = 50.0f;
  bool manualOverride = false;
  bool relayManual = false;
  int servoManual = 0;
} cfg;

struct Data {
  float soil, airT, airH, dist, water, tilt, thermT, pot, risk;
  bool pir, obs, tiltInt;
  char hazard[12];
  int alert;
  uint32_t uptime;
  int rssi;
  uint32_t heap;
} d = {50,25,60,400,0,0,25,1,0,0,0,0,"NOMINAL",0,0,0,0};

float ema_soil=50, ema_water=0, ema_tilt=0, ema_temp=0;
#define EMA_A 0.25f

uint32_t lastSample=0, lastDisplay=0, lastJSON=0, bootMs=0;
#define MONITOR_SIZE 10
String webMonitor[MONITOR_SIZE];
uint8_t monitorIdx = 0;
char slaveBuf[80];
uint8_t slaveIdx = 0;

// ── ADVANCED WEB DASHBOARD (v4.5) ────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>Geo-Sense Command Center</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body{background:#0a0a0c;color:#e0e0e0;font-family:'Segoe UI',sans-serif;margin:0;padding:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(350px,1fr));gap:20px}
.card{background:#16161a;border:1px solid #23232a;border-radius:12px;padding:20px;box-shadow:0 8px 16px rgba(0,0,0,0.5)}
h1{color:#10b981;margin-top:0;font-size:20px;text-transform:uppercase;letter-spacing:1px}
.risk-val{font-size:72px;font-weight:900;text-align:center;color:#10b981;text-shadow:0 0 20px rgba(16,185,129,0.3)}
.btn{background:#25252b;border:1px solid #444;color:#fff;padding:10px 15px;border-radius:6px;cursor:pointer;width:100%;margin-top:10px;transition:0.3s}
.btn:hover{background:#3b82f6;border-color:#3b82f6}.btn-danger:hover{background:#ef4444;border-color:#ef4444}
.stat-item{padding:10px;background:#1c1c22;border-radius:8px;margin-bottom:8px;display:flex;justify-content:space-between}
.stat-label{color:#888;font-size:11px;text-transform:uppercase}
.ctrl-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
textarea{width:100%;height:120px;background:#000;color:#0f0;font-family:monospace;border:1px solid #333;padding:10px;border-radius:8px;font-size:12px}
.badge{padding:4px 8px;border-radius:4px;font-size:10px;font-weight:bold}
.badge-ok{background:rgba(16,185,129,0.2);color:#10b981}
</style></head><body>
<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:20px">
  <h1>GEO-SENSE COMMAND CENTER <span style="color:#444">v4.5</span></h1>
  <div id="sys-status" class="badge badge-ok">SYSTEM ONLINE</div>
</div>
<div class="grid">
  <div class="card"><div class="stat-label">System Risk Index</div><div class="risk-val" id="risk-display">0.0</div><canvas id="riskChart" height="150"></canvas></div>
  <div class="card"><h1>REMOTE OVERRIDE</h1><div class="ctrl-grid">
    <button class="btn" onclick="ctrl('rel')">Toggle Siren</button>
    <button class="btn" onclick="ctrl('servo')">Test Flag</button>
    <button class="btn btn-danger" onclick="ctrl('reset')" style="grid-column:span 2">Emergency Reset</button>
  </div><div style="margin-top:15px">
    <div class="stat-label">Siren Status: <span id="rel-status" style="color:#fff">OFF</span></div>
    <div class="stat-label">Servo Angle: <span id="ser-status" style="color:#fff">0°</span></div>
  </div></div>
  <div class="card"><h1>ALERTS & THRESHOLDS</h1>
    <div class="stat-item"><span>Flood Trigger</span><input type="range" id="t-water" min="10" max="90" onchange="setT('water')"><span id="v-water">60%</span></div>
    <div class="stat-item"><span>Drought Trigger</span><input type="range" id="t-soil" min="10" max="90" onchange="setT('soil')"><span id="v-soil">70%</span></div>
    <div class="stat-item"><span>Slope Trigger</span><input type="range" id="t-tilt" min="10" max="90" onchange="setT('tilt')"><span id="v-tilt">50%</span></div>
  </div>
  <div class="card"><h1>SYSTEM DIAGNOSTICS</h1>
    <div class="stat-item"><span class="stat-label">WiFi Strength</span><span id="rssi">--</span>dBm</div>
    <div class="stat-item"><span class="stat-label">Free Heap</span><span id="heap">--</span>KB</div>
    <div class="stat-item"><span class="stat-label">Uptime</span><span id="uptime">--</span>s</div>
    <div class="stat-item"><span class="stat-label">Active Hazard</span><span id="hazard" style="color:#3b82f6">NOMINAL</span></div>
  </div>
  <div class="card" style="grid-column:span 2"><h1>REMOTE SERIAL TELEMETRY</h1><textarea id="monitor" readonly></textarea></div>
</div>
<script>
const charts={risk:new Chart(document.getElementById('riskChart').getContext('2d'),{type:'line',data:{labels:[],datasets:[{data:[],borderColor:'#10b981',fill:true,tension:0.4}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:false}},scales:{x:{display:false},y:{suggestedMax:9,grid:{color:'#222'}}}}})};
async function poll(){
  const r=await fetch('/data').then(r=>r.json());
  document.getElementById('risk-display').innerText=r.risk.toFixed(1);
  document.getElementById('hazard').innerText=r.hazard;
  document.getElementById('rssi').innerText=r.rssi;
  document.getElementById('heap').innerText=(r.heap/1024).toFixed(1);
  document.getElementById('uptime').innerText=r.uptime;
  document.getElementById('monitor').value=r.mon.join('\n');
  document.getElementById('rel-status').innerText=r.relManual?"MANUAL ON":"AUTO";
  document.getElementById('ser-status').innerText=r.serPos+"°";
  const c=charts.risk;c.data.labels.push('');c.data.datasets[0].data.push(r.risk);if(c.data.labels.length>20){c.data.labels.shift();c.data.datasets[0].data.shift();}c.update('none');
}
async function ctrl(t){await fetch(`/ctrl?type=${t}`);poll();}
async function setT(t){const v=document.getElementById('t-'+t).value;document.getElementById('v-'+t).innerText=v+'%';await fetch(`/set?t=${t}&v=${v}`);}
setInterval(poll,2000);poll();
</script></body></html>
)rawliteral";

// ── HANDLERS ─────────────────────────────────────────────────
void handleData() {
  String mon = "[";
  for(int i=0; i<MONITOR_SIZE; i++){
    int idx = (monitorIdx+i)%MONITOR_SIZE;
    if(webMonitor[idx].length()>0) mon += "\"" + webMonitor[idx] + "\"" + (i==MONITOR_SIZE-1?"":",");
  }
  mon += "]";
  char buf[800];
  snprintf(buf, sizeof(buf), "{\"risk\":%.2f,\"hazard\":\"%s\",\"water\":%.1f,\"soil\":%.1f,\"airT\":%.1f,\"tilt\":%.1f,\"rssi\":%d,\"heap\":%d,\"uptime\":%lu,\"mon\":%s,\"relManual\":%s,\"serPos\":%d}",
           d.risk, d.hazard, d.water, d.soil, d.airT, d.tilt, WiFi.RSSI(), ESP.getFreeHeap(), (millis()-bootMs)/1000, mon.c_str(), cfg.relayManual?"true":"false", (int)servo.read());
  server.send(200, "application/json", buf);
}

void handleCtrl() {
  String type = server.arg("type");
  if(type == "rel") { cfg.relayManual = !cfg.relayManual; cfg.manualOverride = true; }
  else if(type == "servo") { cfg.servoManual = (cfg.servoManual==0)?90:0; cfg.manualOverride = true; }
  else if(type == "reset") { cfg.manualOverride = false; cfg.relayManual = false; cfg.servoManual = 0; d.alert = 0; }
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  String t = server.arg("t");
  float v = server.arg("v").toFloat();
  if(t == "water") cfg.thresholdWater = v;
  else if(t == "soil") cfg.thresholdSoil = v;
  else if(t == "tilt") cfg.thresholdTilt = v;
  server.send(200, "text/plain", "OK");
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  esp_task_wdt_init(10, true); esp_task_wdt_add(NULL);
  Wire.begin(PIN_SDA, PIN_SCL);

  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_RELAY, OUTPUT);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT); pinMode(LED_RED, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);

  if(oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { oled.clearDisplay(); oled.display(); }
  lcd.init(); lcd.backlight(); dht.begin(); servo.attach(PIN_SERVO); servo.write(0);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleData);
  server.on("/ctrl", handleCtrl);
  server.on("/set", handleSet);
  server.begin();
  bootMs = millis();
}

void loop() {
  esp_task_wdt_reset(); server.handleClient();
  uint32_t now = millis();

  while(Serial2.available()) {
    char c = (char)Serial2.read();
    if(c=='\n'||c=='\r') {
      if(slaveIdx>0) {
        slaveBuf[slaveIdx]='\0';
        webMonitor[monitorIdx] = String(slaveBuf);
        monitorIdx = (monitorIdx+1)%MONITOR_SIZE;
        auto getF = [&](const char* k){ char* p=strstr(slaveBuf,k); return p?atof(p+strlen(k)):-999.0f; };
        float w=getF("W:"); if(w>-999) d.water=w;
        float s=getF("S:"); if(s>-999) d.tilt=s;
        float p=getF("P:"); if(p>-999) d.pot=p;
        char* ip=strstr(slaveBuf,"I:"); if(ip) d.tiltInt=(atoi(ip+2)==1);
      }
      slaveIdx=0;
    } else if(slaveIdx<79) slaveBuf[slaveIdx++]=c;
  }

  if(now - lastSample >= 3000) {
    lastSample = now;
    d.airT = dht.readTemperature();
    d.soil = constrain((float)map(analogRead(PIN_SOIL), 3200, 1200, 0, 100), 0, 100);
    
    // Risk Fusion with Dynamic Thresholds
    ema_water = EMA_A*d.water + (1-EMA_A)*ema_water;
    ema_tilt = EMA_A*d.tilt + (1-EMA_A)*ema_tilt;
    ema_soil = EMA_A*d.soil + (1-EMA_A)*ema_soil;

    d.risk = constrain(((ema_water/cfg.thresholdWater*40 + ema_tilt/cfg.thresholdTilt*40 + ema_soil/cfg.thresholdSoil*20)*d.pot/100.0f)*9.0f/100.0f, 0, 9);
    
    if(!cfg.manualOverride) {
      d.alert = (d.risk > 7)?2:(d.risk > 4)?1:0;
      if(d.tiltInt) { d.risk=9; d.alert=2; strcpy(d.hazard, "LANDSLIDE"); }
      else if(d.water > cfg.thresholdWater) strcpy(d.hazard, "FLOOD");
      else if(d.soil > cfg.thresholdSoil) strcpy(d.hazard, "DROUGHT");
      else strcpy(d.hazard, "NOMINAL");
      
      digitalWrite(PIN_RELAY, (d.alert==2)?LOW:HIGH);
      servo.write(d.alert==2?135:d.alert==1?80:0);
    } else {
      digitalWrite(PIN_RELAY, cfg.relayManual?LOW:HIGH);
      servo.write(cfg.servoManual);
    }
    
    Serial2.printf("CMD:%d\n", d.alert);
    Serial.printf("{\"node\":\"master\",\"risk\":%.1f,\"heap\":%d,\"rssi\":%d}\n", d.risk, ESP.getFreeHeap(), WiFi.RSSI());
  }

  if(now - lastDisplay >= 500) {
    lastDisplay = now;
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0); oled.printf("R:%.1f [%s]", d.risk, d.hazard);
    oled.setCursor(0,10); oled.printf("RSSI:%d Heap:%dK", WiFi.RSSI(), ESP.getFreeHeap()/1024);
    oled.setCursor(0,20); oled.printf("W:%.0f%% S:%.0f%%", d.water, d.soil);
    if(cfg.manualOverride) oled.setCursor(0,55), oled.print("!! MANUAL OVERRIDE !!");
    oled.display();
  }
}
