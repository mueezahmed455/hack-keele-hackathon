// ================================================================
//  GEO-SENSE AFRICA v5.2 — ESP32 MASTER GATEWAY (ULTRA-SPEED)
//  Disaster-Resilient Mesh & High-Performance Web Dashboard
// ================================================================
//  v5.2 IMPROVEMENTS:
//  - [FIX] Mesh Tab: Now displays all nodes with stats & icons
//  - [FIX] Command Tab: Logs display fixed, added Reset button
//  - [NEW] Room Temp Tab: Dummy temp graphs with min/max/avg stats
//  - [FIX] JSON Handler: Robust snprintf with all data fields
//  - [FIX] Charts: Pre-initialized with 30 data points
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
const char* AP_SSID = "GeoSense-v5-Mesh";
const char* AP_PASSWORD = "geosense2024";

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_DHT 4
#define PIN_TRIG 5
#define PIN_ECHO 18
#define PIN_SOIL 36
#define PIN_SERVO 13
#define PIN_RELAY 12
#define SLAVE_RX 16
#define SLAVE_TX 17

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHT11);
Servo servo;
WebServer server(80);

// ── DATA STRUCTURES ──────────────────────────────────────────
struct MeshNode { char id[16]; char role[10]; int rssi; bool active; int hops; float load; };
MeshNode mesh[4] = {
  {"GATEWAY-PRO", "Master", -30, true, 0, 12.5},
  {"RELAY-ALPHA", "Relay", -65, true, 1, 38.2},
  {"RELAY-BETA",  "Relay", -78, true, 1, 24.1},
  {"EDGE-SENSOR", "Edge",  -85, true, 2, 5.2}
};

struct Data {
  float soil, airT, water, tilt, risk, pot, roomTemp;
  bool tiltInt;
  char hazard[12];
  int alert;
} d = {50,25,0,0,0,1,24.5,false,"NOMINAL",0};

float ema_soil=50, ema_water=0, ema_tilt=0;
#define EMA_A 0.25f

uint32_t lastSample=0, lastDisplay=0, bootMs=0;
#define LOG_SIZE 6
String sysLogs[LOG_SIZE];
uint8_t logHead = 0;
char slaveBuf[80];
uint8_t slaveIdx = 0;

// ── OPTIMIZED WEB UI (v5.2) ──────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>Geo-Sense v5.2 Pro</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
:root{--bg:#0a0a0c;--card:#141418;--accent:#10b981;--border:#222228;--text:#eee}
body{background:var(--bg);color:var(--text);font-family:sans-serif;margin:0;display:flex;height:100vh}
.sidebar{width:220px;background:var(--card);padding:25px;border-right:1px solid var(--border)}
.nav{padding:12px;margin-bottom:8px;border-radius:8px;cursor:pointer;color:#888;transition:0.2s}
.nav.active{background:rgba(16,185,129,0.1);color:var(--accent)}
.main{flex:1;padding:30px;overflow-y:auto}
.tab{display:none}.tab.active{display:block}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;margin-bottom:20px}
.risk-val{font-size:64px;font-weight:900;color:var(--accent);text-align:center}
.mesh-node{background:#1c1c24;padding:15px;border-radius:8px;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;border-left:4px solid var(--accent)}
.offline{border-left-color:#ef4444;opacity:0.5}
.chart-box{height:200px}
.btn{background:#25252b;border:1px solid #444;color:#fff;padding:12px;border-radius:6px;cursor:pointer;width:100%;margin-bottom:10px}
.btn:hover{background:#35353b}
.stat-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;margin-bottom:20px}
.stat-card{background:#1c1c24;padding:15px;border-radius:8px;text-align:center}
.stat-val{font-size:28px;font-weight:bold;color:var(--accent)}
.stat-label{font-size:12px;color:#666;margin-top:5px}
.mesh-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:15px}
.node-icon{font-size:24px;margin-right:10px}
</style></head><body>
<div class="sidebar">
  <h2 style="color:var(--accent)">GEOSENSE v5.2</h2>
  <div class="nav active" onclick="tab(0)">Analytics</div>
  <div class="nav" onclick="tab(1)">Command</div>
  <div class="nav" onclick="tab(2)">Mesh</div>
  <div class="nav" onclick="tab(3)">Room Temp</div>
</div>
<div class="main">
  <div id="t0" class="tab active">
    <div class="grid" style="display:grid;grid-template-columns:1fr 1fr;gap:20px">
      <div class="card"><div style="text-transform:uppercase;font-size:10px;color:#666">Risk Index</div><div class="risk-val" id="r-v">0.0</div><div class="chart-box"><canvas id="rc"></canvas></div></div>
      <div class="card"><h3>Live Sensors</h3><div style="font-size:24px;margin:10px 0">Water Level: <b id="w-v" style="color:#3b82f6">--</b>%</div><div style="font-size:24px;margin:10px 0">Slope Angle: <b id="s-v" style="color:#ef4444">--</b>%</div><div style="font-size:24px;margin:10px 0">Soil Moisture: <b id="m-v" style="color:#10b981">--</b>%</div><div class="chart-box"><canvas id="hc"></canvas></div></div>
    </div>
  </div>
  <div id="t1" class="tab">
    <div class="card"><h3>Manual Overrides</h3><button class="btn" onclick="ctrl('rel')">Toggle Relay</button><button class="btn" onclick="ctrl('servo')">Test Servo Flag</button><button class="btn" onclick="ctrl('reset')">Reset System</button></div>
    <div class="card"><h3>System Logs</h3><pre id="logs" style="color:#0f0;font-size:12px;white-space:pre-wrap"></pre></div>
  </div>
  <div id="t2" class="tab">
    <h3>Mesh Network Topology</h3>
    <div class="stat-grid">
      <div class="stat-card"><div class="stat-val" id="mesh-count">0</div><div class="stat-label">Active Nodes</div></div>
      <div class="stat-card"><div class="stat-val" id="mesh-signal">--</div><div class="stat-label">Avg Signal</div></div>
    </div>
    <div id="ml" class="mesh-grid"></div>
  </div>
  <div id="t3" class="tab">
    <h3>Room Temperature Analytics</h3>
    <div class="stat-grid">
      <div class="stat-card"><div class="stat-val" id="rt-cur">--</div><div class="stat-label">Current (°C)</div></div>
      <div class="stat-card"><div class="stat-val" id="rt-avg">--</div><div class="stat-label">Avg (°C)</div></div>
      <div class="stat-card"><div class="stat-val" id="rt-min">--</div><div class="stat-label">Min (°C)</div></div>
      <div class="stat-card"><div class="stat-val" id="rt-max">--</div><div class="stat-label">Max (°C)</div></div>
    </div>
    <div class="card"><div class="chart-box" style="height:300px"><canvas id="tc"></canvas></div></div>
  </div>
</div>
<script>
const cfg=(col,max,min=0)=>({type:'line',data:{labels:Array(30).fill(''),datasets:[{data:Array(30).fill(min),borderColor:col,fill:true,backgroundColor:col+'22',tension:0.3,pointRadius:1,pointHitRadius:10}]},options:{responsive:true,maintainAspectRatio:false,animation:false,plugins:{legend:{display:false},tooltip:{mode:'index',intersect:false}},scales:{x:{display:false},y:{min:min,suggestedMax:max,grid:{color:'#2a2a30'},ticks:{color:'#888'}}}}});
const charts={r:cfg('#10b981',9,0),h:cfg('#3b82f6',100,0),t:cfg('#f59e0b',40,15)};
let chartInstances={};
function tab(i){document.querySelectorAll('.tab').forEach((t,idx)=>{t.classList.toggle('active',idx===i);});document.querySelectorAll('.nav').forEach((n,idx)=>{n.classList.toggle('active',idx===i);});}
function updateChart(c,data){c.data.labels.shift();c.data.labels.push('');c.data.datasets[0].data.shift();c.data.datasets[0].data.push(data);c.draw();}
let rtData={min:999,max:-999,sum:0,count:0};
async function poll(){
  try{
    const r=await fetch('/data').then(res=>res.json());
    document.getElementById('r-v').innerText=r.risk.toFixed(1);
    document.getElementById('w-v').innerText=r.water.toFixed(1);
    document.getElementById('s-v').innerText=r.tilt.toFixed(1);
    document.getElementById('m-v').innerText=r.soil.toFixed(1);
    document.getElementById('logs').innerText=r.logs.join('\n');
    document.getElementById('mesh-count').innerText=r.meshActiveCount;
    document.getElementById('mesh-signal').innerText=r.meshAvgRssi+'dBm';
    document.getElementById('ml').innerHTML=r.mesh.map(n=>'<div class="mesh-node '+(n.active?'':'offline')+'"><div><span class="node-icon">'+(n.role==='Master'?'🌐':n.role==='Relay'?'📡':'🌡️')+'</span><b>'+n.id+'</b><br><small style="color:#888">'+n.role+'</small></div><div style="text-align:right"><div style="color:'+(n.rssi>-70?'#10b981':n.rssi>-85?'#f59e0b':'#ef4444')+'">'+n.rssi+'dBm</div><small style="color:#888">Hops:'+n.hops+'</small></div></div>').join('');
    document.getElementById('rt-cur').innerText=r.roomTemp.toFixed(1);
    rtData.min=Math.min(rtData.min,r.roomTemp);rtData.max=Math.max(rtData.max,r.roomTemp);rtData.sum+=r.roomTemp;rtData.count++;
    document.getElementById('rt-avg').innerText=(rtData.sum/rtData.count).toFixed(1);
    document.getElementById('rt-min').innerText=rtData.min.toFixed(1);
    document.getElementById('rt-max').innerText=rtData.max.toFixed(1);
    updateChart(chartInstances.r,r.risk);updateChart(chartInstances.h,r.water);updateChart(chartInstances.t,r.roomTemp);
  }catch(e){console.error('Poll error:',e);}
}
async function ctrl(t){try{await fetch('/ctrl?type='+t);}catch(e){console.error('Ctrl error:',e);}}
window.onload=function(){
  chartInstances.r=new Chart(document.getElementById('rc'),cfg('#10b981',9,0));
  chartInstances.h=new Chart(document.getElementById('hc'),cfg('#3b82f6',100,0));
  chartInstances.t=new Chart(document.getElementById('tc'),cfg('#f59e0b',40,15));
  setInterval(poll,1500);poll();
};
</script></body></html>
)rawliteral";

// ── LOGGING ──────────────────────────────────────────────────
void addLog(String m) {
  sysLogs[logHead] = "[" + String(millis()/1000) + "s] " + m;
  logHead = (logHead + 1) % LOG_SIZE;
}

// ── OPTIMIZED HANDLERS ───────────────────────────────────────
void handleData() {
  char buf[1400];
  int pos = 0;
  
  // Calculate mesh stats
  int activeCount = 0;
  int rssiSum = 0;
  for(int i=0; i<4; i++) {
    if(mesh[i].active) {
      activeCount++;
      rssiSum += mesh[i].rssi;
    }
  }
  int avgRssi = activeCount > 0 ? rssiSum / activeCount : 0;
  
  pos += snprintf(buf, sizeof(buf), 
    "{\"risk\":%.1f,\"water\":%.1f,\"tilt\":%.1f,\"soil\":%.1f,\"airT\":%.1f,\"roomTemp\":%.1f,",
    d.risk, d.water, d.tilt, d.soil, d.airT, d.roomTemp);
  
  pos += snprintf(buf + pos, sizeof(buf) - pos, "\"meshActiveCount\":%d,\"meshAvgRssi\":%d,", activeCount, avgRssi);
  
  // Add logs
  pos += snprintf(buf + pos, sizeof(buf) - pos, "\"logs\":[");
  for(int i=0; i<LOG_SIZE; i++) {
    int idx = (logHead + i) % LOG_SIZE;
    if(sysLogs[idx].length() > 0) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "\"%s\"%s", sysLogs[idx].c_str(), (i == LOG_SIZE-1 ? "" : ","));
    }
  }
  // Remove trailing comma if no logs, close array
  if(pos > 0 && buf[pos-1] == ',') pos--;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "],");
  
  // Add mesh array
  pos += snprintf(buf + pos, sizeof(buf) - pos, "\"mesh\":[");
  for(int i=0; i<4; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, 
      "{\"id\":\"%s\",\"role\":\"%s\",\"rssi\":%d,\"active\":%s,\"hops\":%d}%s",
      mesh[i].id, mesh[i].role, mesh[i].rssi, mesh[i].active?"true":"false", mesh[i].hops, (i==3?"":","));
  }
  snprintf(buf + pos, sizeof(buf) - pos, "]}");
  
  server.send(200, "application/json", buf);
}

void handleCtrl() {
  String type = server.arg("type");
  if(type == "rel") {
    digitalWrite(PIN_RELAY, !digitalRead(PIN_RELAY));
    addLog("Relay Toggled");
  }
  else if(type == "servo") {
    int cur = servo.read();
    servo.write(cur == 0 ? 135 : 0);
    addLog("Servo Tested");
  }
  else if(type == "reset") {
    addLog("System Reset Requested");
    for(int i=0; i<4; i++) {
      mesh[i].active = true;
      mesh[i].rssi = -60 + random(-10, 10);
    }
    addLog("Mesh Network Reset");
  }
  server.send(200, "text/plain", "OK");
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  esp_task_wdt_init(15, true); esp_task_wdt_add(NULL); Wire.begin(PIN_SDA, PIN_SCL);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, HIGH);
  if(oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { oled.clearDisplay(); oled.display(); }
  dht.begin(); servo.attach(PIN_SERVO); servo.write(0);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleData);
  server.on("/ctrl", handleCtrl);
  server.begin(); bootMs = millis();
  addLog("GeoSense v5.2 Pro Online");
}

void loop() {
  esp_task_wdt_reset(); server.handleClient();
  uint32_t now = millis();

  // ── NON-BLOCKING SLAVE SYNC ───────────────────────────────
  while(Serial2.available()) {
    char c = (char)Serial2.read();
    if(c=='\n'||c=='\r') {
      if(slaveIdx > 0) {
        slaveBuf[slaveIdx] = '\0';
        auto getF = [&](const char* k){ char* p=strstr(slaveBuf,k); return p?atof(p+strlen(k)):-999.0f; };
        float w=getF("W:"), s=getF("S:"), p=getF("P:");
        if(w > -999) d.water = w; if(s > -999) d.tilt = s; if(p > -999) d.pot = p;
        char* ip=strstr(slaveBuf,"I:"); if(ip) d.tiltInt = (atoi(ip+2)==1);
      }
      slaveIdx = 0;
    } else if(slaveIdx < 79) slaveBuf[slaveIdx++] = c;
  }

  // ── CORE ANALYSIS ──────────────────────────────────────────
  if(now - lastSample >= 3000) {
    lastSample = now;
    float t = dht.readTemperature(); if(!isnan(t)) d.airT = t;
    d.soil = constrain((float)map(analogRead(PIN_SOIL), 3200, 1200, 0, 100), 0, 100);
    ema_soil = EMA_A*d.soil + (1-EMA_A)*ema_soil;
    ema_water = EMA_A*d.water + (1-EMA_A)*ema_water;
    ema_tilt = EMA_A*d.tilt + (1-EMA_A)*ema_tilt;
    d.risk = constrain(((ema_soil*0.2 + ema_water*0.4 + ema_tilt*0.4)*d.pot/100.0f)*9.0f/100.0f, 0, 9);
    d.alert = (d.risk > 7 || d.tiltInt)?2:(d.risk > 4?1:0);
    if(d.tiltInt) { d.risk=9; strcpy(d.hazard, "LANDSLIDE"); }
    else if(ema_water>60) strcpy(d.hazard, "FLOOD");
    else if(ema_soil>70) strcpy(d.hazard, "DROUGHT"); else strcpy(d.hazard, "NOMINAL");
    Serial2.printf("CMD:%d\n", d.alert);

    // DUMMY ROOM TEMPERATURE GENERATION (Simulates realistic indoor temp)
    static float baseTemp = 24.5;
    static int tempDirection = 1;
    float tempVariation = (float)random(0, 100) / 100.0f;  // 0.00 to 0.99
    baseTemp += (tempDirection * 0.1f) + (tempVariation * 0.05f);
    if(baseTemp > 28.0f) tempDirection = -1;
    if(baseTemp < 22.0f) tempDirection = 1;
    d.roomTemp = baseTemp + (sin(millis() / 60000.0f) * 0.5f);  // Add slight sinusoidal variation

    // MESH SIMULATION
    for(int i=1; i<4; i++){
      mesh[i].rssi = -60 + random(-10,10);
      if(i==3) mesh[i].active = (d.risk < 8.5);
    }
  }

  // ── OLED (REDUCED FREQUENCY) ───────────────────────────────
  if(now - lastDisplay >= 1000) {
    lastDisplay = now;
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0); oled.printf("v5.2 MESH: %d NODES", (mesh[0].active+mesh[1].active+mesh[2].active+mesh[3].active));
    oled.setCursor(0,15); oled.printf("RISK: %.1f [%s]", d.risk, d.hazard);
    oled.setCursor(0,30); oled.printf("W:%d%% S:%d%% T:%dC", (int)d.water, (int)d.soil, (int)d.airT);
    oled.setCursor(0,45); oled.printf("IP: 192.168.4.1");
    oled.display();
  }
}
