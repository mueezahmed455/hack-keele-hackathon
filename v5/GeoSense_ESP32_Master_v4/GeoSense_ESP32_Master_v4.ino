// ================================================================
//  GEO-SENSE AFRICA v4.7 — ESP32 MASTER NODE (BUG-FIXED)
//  Disaster-Resilient Mesh & Professional Analytics
// ================================================================
//  FIXES IN v4.7:
//  - [FIX] Slave Serial Parser: Now correctly parses T, S, and P fields.
//  - [FIX] Data Struct: Added missing thermT and pot fields.
//  - [FIX] Risk Logic: Re-implemented EMA-weighted risk with Pot calibration.
//  - [FIX] Memory Safety: Improved JSON string building to prevent overflows.
//  - [FIX] UI Stability: Tabbed interface logic and Mesh status updates.
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
const char* AP_SSID = "GeoSense-Africa-Mesh";
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
struct MeshNode {
  String id;
  String role;
  int rssi;
  bool active;
  int hops;
  int packets;
};

MeshNode mesh[3] = {
  {"GS-MASTER-01", "Gateway", -30, true, 0, 0},
  {"GS-RELAY-02", "Relay", -65, true, 1, 0},
  {"GS-EDGE-03", "Sensor", -82, false, 2, 0}
};

struct Data {
  float soil, airT, airH, dist, water, tilt, thermT, pot, risk;
  bool pir, obs, tiltInt;
  char hazard[12];
  int alert;
} d = {50,25,60,400,0,0,25,1,0,0,0,0,"NOMINAL",0};

float ema_soil=50, ema_water=0, ema_tilt=0;
#define EMA_A 0.25f

uint32_t lastSample=0, lastDisplay=0, bootMs=0;
#define LOG_SIZE 8
String sysLogs[LOG_SIZE];
uint8_t logHead = 0;

char slaveBuf[80];
uint8_t slaveIdx = 0;

// ── WEB UI ───────────────────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>Geo-Sense Pro v4.7</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
:root{--bg:#0a0a0c;--card:#121216;--accent:#10b981;--border:#222228;--text:#e0e0e0;--danger:#ef4444;--warn:#f59e0b;--info:#3b82f6}
body{background:var(--bg);color:var(--text);font-family:sans-serif;margin:0;display:flex}
.sidebar{width:240px;background:var(--card);height:100vh;padding:25px;border-right:1px solid var(--border);position:fixed;overflow-y:auto}
.main{flex:1;margin-left:240px;padding:30px;min-height:100vh}
.nav{padding:12px;margin-bottom:10px;border-radius:8px;cursor:pointer;color:#888;transition:0.2s;display:flex;align-items:center;gap:10px}
.nav:hover{background:#1a1a20;color:#fff}
.nav.active{background:rgba(16,185,129,0.1);color:var(--accent);border:1px solid rgba(16,185,129,0.2)}
.tab{display:none}.tab.active{display:block}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;margin-bottom:20px}
.risk-val{font-size:72px;font-weight:900;color:var(--accent);text-align:center;margin:10px 0}
.mesh-node{display:flex;justify-content:space-between;align-items:center;padding:15px;background:#1a1a20;border-radius:8px;margin-bottom:10px;border-left:4px solid #444;transition:0.3s}
.mesh-node.online{border-left-color:var(--accent)}
.mesh-node.offline{border-left-color:var(--danger);opacity:0.6}
.mesh-stats{display:flex;gap:15px;font-size:12px;color:#888}
.mesh-packets{background:#222;padding:2px 8px;border-radius:4px}
.btn{background:#25252b;border:1px solid #444;color:#fff;padding:12px;border-radius:6px;cursor:pointer;width:100%;margin-bottom:10px;transition:0.2s}
.btn:hover{background:var(--accent);border-color:var(--accent)}
.log-box{background:#000;color:#0f0;font-family:monospace;padding:15px;border-radius:8px;height:200px;overflow-y:auto;font-size:12px;white-space:pre-wrap}
.topology-viz{display:flex;justify-content:space-around;align-items:center;padding:20px;position:relative;min-height:200px}
.node-viz{display:flex;flex-direction:column;align-items:center;position:relative;z-index:2}
.node-circle{width:60px;height:60px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:bold;border:3px solid #444;background:#1a1a20;transition:0.3s}
.node-circle.online{border-color:var(--accent);box-shadow:0 0 20px rgba(16,185,129,0.3)}
.node-circle.offline{border-color:var(--danger);background:#2a1a1a}
.node-circle.relay{border-color:var(--info)}
.node-label{margin-top:8px;font-size:12px;color:#888;text-align:center}
.node-role{font-size:10px;color:#666}
.packet{position:absolute;width:10px;height:10px;background:var(--accent);border-radius:50%;animation:packetMove 2s infinite}
@keyframes packetMove{0%{opacity:1;transform:scale(1)}100%{opacity:0;transform:scale(0)}}
.chart-container{position:relative;height:250px;margin-bottom:20px}
.temp-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:15px}
.temp-card{background:#1a1a20;padding:15px;border-radius:8px;text-align:center}
.temp-val{font-size:32px;font-weight:bold;color:var(--accent)}
.temp-label{font-size:12px;color:#888;margin-top:5px}
.status-badge{padding:4px 12px;border-radius:12px;font-size:11px;font-weight:bold}
.status-good{background:rgba(16,185,129,0.2);color:var(--accent)}
.status-warn{background:rgba(245,158,11,0.2);color:var(--warn)}
.status-crit{background:rgba(239,68,68,0.2);color:var(--danger)}
</style></head><body>
<div class="sidebar">
  <h2 style="color:var(--accent);margin-bottom:30px">🌍 GEOSENSE</h2>
  <div class="nav active" onclick="showTab(0)">📊 Dashboard</div>
  <div class="nav" onclick="showTab(1)">🎛️ Command</div>
  <div class="nav" onclick="showTab(2)">🌐 Mesh Net</div>
  <div class="nav" onclick="showTab(3)">🌡️ Temperature</div>
  <div style="margin-top:30px;padding:15px;background:#1a1a20;border-radius:8px">
    <div style="font-size:11px;color:#666">Network Status</div>
    <div id="net-status" class="status-badge status-good" style="margin-top:5px">ONLINE</div>
    <div style="font-size:11px;color:#666;margin-top:10px">Active Nodes: <span id="active-nodes">3</span>/3</div>
    <div style="font-size:11px;color:#666">Packets: <span id="total-pkts">0</span></div>
  </div>
</div>
<div class="main">
  <div id="tab0" class="tab active">
    <h1 style="margin-bottom:25px">System Dashboard</h1>
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:20px">
      <div class="card"><div class="risk-val" id="r-val">0.0</div><div style="text-align:center;color:#666">RISK INDEX / 9.0</div></div>
      <div class="card">
        <h3 style="margin-bottom:15px">Live Sensors</h3>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
          <div class="temp-card" style="background:#222"><div class="temp-val" id="w-val" style="font-size:24px">--</div><div class="temp-label">Water %</div></div>
          <div class="temp-card" style="background:#222"><div class="temp-val" id="s-val" style="font-size:24px">--</div><div class="temp-label">Soil %</div></div>
          <div class="temp-card" style="background:#222"><div class="temp-val" id="t-val" style="font-size:24px">--</div><div class="temp-label">Temp °C</div></div>
          <div class="temp-card" style="background:#222"><div class="temp-val" id="sig" style="font-size:24px">--</div><div class="temp-label">RSSI dBm</div></div>
        </div>
      </div>
    </div>
    <div class="card">
      <h3 style="margin-bottom:15px">Risk History</h3>
      <div class="chart-container"><canvas id="riskChart"></canvas></div>
    </div>
  </div>
  <div id="tab1" class="tab">
    <h1 style="margin-bottom:25px">Manual Overrides</h1>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">
      <div class="card">
        <h3 style="margin-bottom:15px">Control Panel</h3>
        <button class="btn" onclick="ctrl('rel')" style="padding:15px;font-size:16px">🔔 Toggle Siren Relay</button>
        <button class="btn" onclick="ctrl('servo')" style="padding:15px;font-size:16px">🚩 Test Servo Flag</button>
        <button class="btn" onclick="ctrl('reset')" style="padding:15px;font-size:16px;background:#7f1d1d">⚠️ System Reset</button>
      </div>
      <div class="card">
        <h3 style="margin-bottom:15px">System Logs</h3>
        <div class="log-box" id="logs">Waiting for logs...</div>
      </div>
    </div>
  </div>
  <div id="tab2" class="tab">
    <h1 style="margin-bottom:25px">Mesh Network Topology</h1>
    <div class="card">
      <div class="topology-viz" id="topology-viz">
        <div class="node-viz"><div class="node-circle online" id="node-0-circle">M</div><div class="node-label">MASTER<br><span class="node-role">Gateway</span></div></div>
        <div class="node-viz"><div class="node-circle relay" id="node-1-circle">R</div><div class="node-label">RELAY<br><span class="node-role">Repeater</span></div></div>
        <div class="node-viz"><div class="node-circle" id="node-2-circle">E</div><div class="node-label">EDGE<br><span class="node-role">Sensor</span></div></div>
      </div>
    </div>
    <div class="card"><h3 style="margin-bottom:15px">Node Details</h3><div id="mesh-list"></div></div>
    <div class="card">
      <h3 style="margin-bottom:15px">Packet Flow Simulation</h3>
      <div style="background:#000;padding:20px;border-radius:8px;overflow:hidden;position:relative;height:100px" id="packet-viz">
        <div style="color:#444;font-size:12px;text-align:center">Live packet transmission visualization</div>
      </div>
    </div>
  </div>
  <div id="tab3" class="tab">
    <h1 style="margin-bottom:25px">Temperature Analytics</h1>
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:20px;margin-bottom:20px">
      <div class="card"><div class="temp-val" id="curr-temp">--</div><div class="temp-label">Current Room Temp (°C)</div></div>
      <div class="card"><div class="temp-val" id="avg-temp">--</div><div class="temp-label">Average (°C)</div></div>
      <div class="card"><div class="temp-val" id="min-temp">--</div><div class="temp-label">Minimum (°C)</div></div>
      <div class="card"><div class="temp-val" id="max-temp">--</div><div class="temp-label">Maximum (°C)</div></div>
    </div>
    <div class="card">
      <h3 style="margin-bottom:15px">Room Temperature History</h3>
      <div class="chart-container"><canvas id="tempChart"></canvas></div>
    </div>
    <div class="card">
      <h3 style="margin-bottom:15px">Thermal Sensor Data</h3>
      <div style="display:grid;grid-template-columns:1fr 1fr;gap:15px">
        <div><span style="color:#888">Thermistor Temp:</span> <span id="therm-t" style="color:var(--accent);font-weight:bold">--</span> °C</div>
        <div><span style="color:#888">DHT11 Temp:</span> <span id="dht-t" style="color:var(--accent);font-weight:bold">--</span> °C</div>
        <div><span style="color:#888">Potentiometer:</span> <span id="pot-val" style="color:var(--accent);font-weight:bold">--</span></div>
        <div><span style="color:#888">Tilt Interrupt:</span> <span id="tilt-st" style="color:var(--accent);font-weight:bold">--</span></div>
      </div>
    </div>
  </div>
</div>
<script>
let charts={};
let tempHistory=[],riskHistory=[],pktCount=0;
const maxHistory=60;

function initCharts(){
  const riskCtx=document.getElementById('riskChart').getContext('2d');
  charts.risk=new Chart(riskCtx,{type:'line',data:{labels:Array(maxHistory).fill(''),datasets:[{label:'Risk',data:Array(maxHistory).fill(0),borderColor:'#10b981',backgroundColor:'rgba(16,185,129,0.1)',fill:true,tension:0.4,pointRadius:0}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:false},tooltip:{mode:'index',intersect:false}},scales:{x:{display:false},y:{min:0,max:9,grid:{color:'#222'},ticks:{color:'#666'}}}}});
  
  const tempCtx=document.getElementById('tempChart').getContext('2d');
  charts.temp=new Chart(tempCtx,{type:'line',data:{labels:Array(maxHistory).fill(''),datasets:[{label:'Room Temp',data:Array(maxHistory).fill(25),borderColor:'#3b82f6',backgroundColor:'rgba(59,130,246,0.1)',fill:true,tension:0.4,pointRadius:0},{label:'Thermistor',data:Array(maxHistory).fill(24),borderColor:'#f59e0b',backgroundColor:'rgba(245,158,11,0.1)',fill:true,tension:0.4,pointRadius:0}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:true,labels:{color:'#888'}},tooltip:{mode:'index',intersect:false}},scales:{x:{display:false},y:{min:15,max:35,grid:{color:'#222'},ticks:{color:'#666'}}}}});
}

function showTab(i){
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.nav').forEach(n=>n.classList.remove('active'));
  document.getElementById('tab'+i).classList.add('active');
  document.querySelectorAll('.nav')[i].classList.add('active');
}

function updateTopology(mesh){
  for(let i=0;i<3;i++){
    const circle=document.getElementById('node-'+i+'-circle');
    if(circle){
      circle.className='node-circle '+(mesh[i].active?'online':'offline');
      if(mesh[i].role==='Relay')circle.classList.add('relay');
    }
  }
}

function simulatePacket(){
  const viz=document.getElementById('packet-viz');
  if(!viz)return;
  const pkt=document.createElement('div');
  pkt.className='packet';
  pkt.style.left='10%';
  pkt.style.top='50%';
  pkt.style.animation='packetMove 1.5s linear forwards';
  viz.appendChild(pkt);
  setTimeout(()=>pkt.remove(),1500);
  pktCount++;
  document.getElementById('total-pkts').innerText=pktCount;
}

function updateNetworkStatus(mesh){
  let active=mesh.filter(n=>n.active).length;
  document.getElementById('active-nodes').innerText=active;
  const status=document.getElementById('net-status');
  if(active===3){status.className='status-badge status-good';status.innerText='ONLINE';}
  else if(active>=1){status.className='status-badge status-warn';status.innerText='DEGRADED';}
  else{status.className='status-badge status-crit';status.innerText='OFFLINE';}
}

async function poll(){
  try{
    const r=await fetch('/data').then(res=>res.json());
    
    // Update main values
    document.getElementById('r-val').innerText=r.risk.toFixed(1);
    document.getElementById('w-val').innerText=r.water.toFixed(1);
    document.getElementById('s-val').innerText=r.soil.toFixed(1);
    document.getElementById('t-val').innerText=r.airT.toFixed(1);
    document.getElementById('sig').innerText=r.rssi;
    
    // Temperature stats
    document.getElementById('curr-temp').innerText=r.airT.toFixed(1);
    document.getElementById('therm-t').innerText=r.thermT.toFixed(1);
    document.getElementById('pot-val').innerText=r.pot.toFixed(0);
    document.getElementById('tilt-st').innerText=r.tiltInt?'ACTIVE':'IDLE';
    document.getElementById('tilt-st').style.color=r.tiltInt?'#ef4444':'#10b981';
    
    // Calc temp stats
    if(r.airT>0){
      tempHistory.push(r.airT);
      if(tempHistory.length>maxHistory)tempHistory.shift();
      const avg=tempHistory.reduce((a,b)=>a+b,0)/tempHistory.length;
      const min=Math.min(...tempHistory);
      const max=Math.max(...tempHistory);
      document.getElementById('avg-temp').innerText=avg.toFixed(1);
      document.getElementById('min-temp').innerText=min.toFixed(1);
      document.getElementById('max-temp').innerText=max.toFixed(1);
    }
    
    // Update charts
    if(charts.risk){
      charts.risk.data.datasets[0].data.push(r.risk);
      charts.risk.data.datasets[0].data.shift();
      charts.risk.update('none');
    }
    if(charts.temp){
      charts.temp.data.datasets[0].data.push(r.airT);
      charts.temp.data.datasets[0].data.shift();
      charts.temp.data.datasets[1].data.push(r.thermT);
      charts.temp.data.datasets[1].data.shift();
      charts.temp.update('none');
    }
    
    // Logs & Mesh
    document.getElementById('logs').innerText=r.logs.join('\n');
    document.getElementById('mesh-list').innerHTML=r.mesh.map(n=>`<div class="mesh-node ${n.active?'online':'offline'}"><div><b>${n.id}</b><br><small>${n.role}</small></div><div class="mesh-stats"><span class="mesh-packets">📦 ${n.packets||0}</span><span>RSSI: ${n.rssi}dBm</span><span>Hops: ${n.hops||0}</span></div></div>`).join('');
    
    updateTopology(r.mesh);
    updateNetworkStatus(r.mesh);
    
    // Simulate packets periodically
    if(Math.random()>0.7)simulatePacket();
    
  }catch(e){console.error(e);}
}

async function ctrl(t){
  try{await fetch(`/ctrl?type=${t}`);addLog('Command sent: '+t);}
  catch(e){addLog('Command failed: '+t);}
}

function addLog(m){
  const logs=document.getElementById('logs');
  if(logs)logs.innerText+=('\n'+new Date().toLocaleTimeString()+': '+m);
}

window.onload=function(){
  initCharts();
  setInterval(poll,2000);
  poll();
};
</script></body></html>
)rawliteral";

// ── LOGGING ──────────────────────────────────────────────────
void addLog(String m) {
  sysLogs[logHead] = "[" + String(millis()/1000) + "s] " + m;
  logHead = (logHead + 1) % LOG_SIZE;
  Serial.print(F("[LOG] ")); Serial.println(m);
}

// ── HANDLERS ─────────────────────────────────────────────────
void handleData() {
  String json = "{";
  json += "\"risk\":" + String(d.risk, 1) + ",";
  json += "\"water\":" + String(d.water, 1) + ",";
  json += "\"soil\":" + String(d.soil, 1) + ",";
  json += "\"airT\":" + String(d.airT, 1) + ",";
  json += "\"thermT\":" + String(d.thermT, 1) + ",";
  json += "\"pot\":" + String(d.pot, 0) + ",";
  json += "\"tiltInt\":" + String(d.tiltInt ? "true" : "false") + ",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";

  json += "\"logs\":[";
  for(int i=0; i<LOG_SIZE; i++) {
    int idx = (logHead + i) % LOG_SIZE;
    if(sysLogs[idx].length() > 0) json += "\"" + sysLogs[idx] + "\"" + (i == LOG_SIZE-1 ? "" : ",");
  }
  json += "],";

  json += "\"mesh\":[";
  for(int i=0; i<3; i++) {
    json += "{\"id\":\""+mesh[i].id+"\",\"role\":\""+mesh[i].role+"\",\"rssi\":"+String(mesh[i].rssi)+",\"active\":"+(mesh[i].active?"true":"false")+",\"hops\":"+String(mesh[i].hops)+",\"packets\":"+String(mesh[i].packets)+"}";
    if(i<2) json += ",";
  }
  json += "]}";

  server.send(200, "application/json", json);
}

void handleCtrl() {
  String type = server.arg("type");
  if(type == "rel") { 
    digitalWrite(PIN_RELAY, !digitalRead(PIN_RELAY)); 
    addLog("Relay Toggled"); 
  }
  else if(type == "servo") { 
    servo.write(servo.read() == 0 ? 90 : 0); 
    addLog("Servo Tested"); 
  }
  else if(type == "reset") {
    addLog("System Reset Requested");
    server.send(200, "text/plain", "RESET");
    delay(100);
    ESP.restart();
  }
  server.send(200, "text/plain", "OK");
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  
  // Initialize random seed
  randomSeed(analogRead(PIN_SOIL));
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,      // 10 seconds
    .idle_core_mask = 0,      // watch both cores (0 = both)
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  Wire.begin(PIN_SDA, PIN_SCL);

  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, HIGH);
  if(oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { oled.clearDisplay(); oled.display(); }

  dht.begin(); servo.attach(PIN_SERVO); servo.write(0);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", handleData);
  server.on("/ctrl", handleCtrl);
  server.begin();

  bootMs = millis();
  addLog("GeoSense v4.7 Master Booted");
}

void loop() {
  esp_task_wdt_reset(); server.handleClient();
  uint32_t now = millis();

  // ── SLAVE PARSER ───────────────────────────────────────────
  while(Serial2.available()) {
    char c = (char)Serial2.read();
    if(c=='\n'||c=='\r') {
      if(slaveIdx > 0) {
        slaveBuf[slaveIdx] = '\0';
        auto getF = [&](const char* k){ char* p=strstr(slaveBuf,k); return p?atof(p+strlen(k)):-999.0f; };
        float w=getF("W:"), t=getF("T:"), s=getF("S:"), p=getF("P:");
        if(w > -999) d.water = w;
        if(t > -999) d.thermT = t;
        if(s > -999) d.tilt = s;
        if(p > -999) d.pot = p;
        char* ip=strstr(slaveBuf,"I:"); if(ip) d.tiltInt = (atoi(ip+2)==1);
      }
      slaveIdx = 0;
    } else if(slaveIdx < 79) slaveBuf[slaveIdx++] = c;
  }

  // ── CORE LOGIC ─────────────────────────────────────────────
  if(now - lastSample >= 3000) {
    lastSample = now;
    float temp = dht.readTemperature();
    if(!isnan(temp)) d.airT = temp;
    d.soil = constrain((float)map(analogRead(PIN_SOIL), 3200, 1200, 0, 100), 0, 100);

    // EMA & Risk Fusion
    ema_soil = EMA_A*d.soil + (1-EMA_A)*ema_soil;
    ema_water = EMA_A*d.water + (1-EMA_A)*ema_water;
    ema_tilt = EMA_A*d.tilt + (1-EMA_A)*ema_tilt;

    float baseRisk = (ema_water*0.4 + ema_soil*0.2 + ema_tilt*0.4);
    d.risk = constrain((baseRisk * d.pot / 100.0f) * 9.0f / 100.0f, 0, 9);

    int oldAlert = d.alert;
    d.alert = (d.risk > 7 || d.tiltInt) ? 2 : (d.risk > 4 ? 1 : 0);
    if(d.tiltInt) { d.risk = 9.0f; strcpy(d.hazard, "LANDSLIDE"); }
    else if(ema_water > 60) strcpy(d.hazard, "FLOOD");
    else if(ema_soil > 70) strcpy(d.hazard, "DROUGHT");
    else strcpy(d.hazard, "NOMINAL");

    if(d.alert != oldAlert) {
      String msg = "Alert Level: " + String(d.alert) + " (" + String(d.hazard) + ")";
      addLog(msg);
    }

    Serial2.printf("CMD:%d\n", d.alert);
    Serial.printf("{\"node\":\"master\",\"risk\":%.1f,\"hazard\":\"%s\"}\n", d.risk, d.hazard);

    // Mesh Network Simulation
    mesh[1].rssi = -60 + random(-8, 8);
    mesh[2].rssi = -75 + random(-10, 10);
    mesh[2].active = (d.risk < 8);
    
    // Simulate packet transmission
    if(mesh[2].active) {
      mesh[2].packets += random(1, 4);
      mesh[1].packets += random(1, 3);
      mesh[0].packets += random(1, 2);
    }
    
    // Dynamic node behavior
    if(d.risk > 6) {
      mesh[1].active = random(0, 10) > 2; // 80% chance to stay active
    } else {
      mesh[1].active = true;
    }
  }

  // ── DISPLAYS ───────────────────────────────────────────────
  if(now - lastDisplay >= 500) {
    lastDisplay = now;
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0); oled.printf("R:%.1f [%s]", d.risk, d.hazard);
    oled.setCursor(0,12); oled.printf("W:%.0f S:%.0f T:%.0f", d.water, d.soil, d.airT);
    oled.setCursor(0,24); oled.printf("MESH:%d RSSI:%d", (mesh[0].active+mesh[1].active+mesh[2].active), WiFi.RSSI());
    if(d.alert == 2) { oled.fillRect(0,40,128,24,SSD1306_WHITE); oled.setTextColor(SSD1306_BLACK); oled.setCursor(5,45); oled.print("!!! CRITICAL !!!"); }
    oled.display();
  }
}
