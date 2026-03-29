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
};

MeshNode mesh[3] = {
  {"GS-MASTER-01", "Gateway", -30, true, 0},
  {"GS-RELAY-02", "Relay", -65, true, 1},
  {"GS-EDGE-03", "Sensor", -82, false, 2}
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
:root{--bg:#0a0a0c;--card:#121216;--accent:#10b981;--border:#222228;--text:#e0e0e0}
body{background:var(--bg);color:var(--text);font-family:sans-serif;margin:0;display:flex}
.sidebar{width:240px;background:var(--card);height:100vh;padding:25px;border-right:1px solid var(--border)}
.main{flex:1;padding:30px;height:100vh;overflow-y:auto}
.nav{padding:12px;margin-bottom:10px;border-radius:8px;cursor:pointer;color:#888;transition:0.2s}
.nav:hover{background:#1a1a20;color:#fff}
.nav.active{background:rgba(16,185,129,0.1);color:var(--accent);border:1px solid rgba(16,185,129,0.2)}
.tab{display:none}.tab.active{display:block}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;margin-bottom:20px}
.risk-val{font-size:72px;font-weight:900;color:var(--accent);text-align:center;margin:10px 0}
.mesh-node{display:flex;justify-content:space-between;padding:15px;background:#1a1a20;border-radius:8px;margin-bottom:10px;border-left:4px solid #444}
.mesh-node.online{border-left-color:var(--accent)}
.btn{background:#25252b;border:1px solid #444;color:#fff;padding:12px;border-radius:6px;cursor:pointer;width:100%;margin-bottom:10px}
.btn:hover{background:var(--accent)}
.log-box{background:#000;color:#0f0;font-family:monospace;padding:15px;border-radius:8px;height:180px;overflow-y:auto;font-size:12px;white-space:pre-wrap}
</style></head><body>
<div class="sidebar">
  <h2 style="color:var(--accent)">GEOSENSE</h2>
  <div class="nav active" onclick="showTab(0)">Dashboard</div>
  <div class="nav" onclick="showTab(1)">Command</div>
  <div class="nav" onclick="showTab(2)">Mesh Net</div>
</div>
<div class="main">
  <div id="tab0" class="tab active">
    <h1>System Status</h1>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">
      <div class="card"><div class="risk-val" id="r-val">0.0</div><div style="text-align:center;color:#666">RISK INDEX / 9.0</div></div>
      <div class="card"><h3>Live Sensors</h3>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
          <div class="card" style="padding:10px">Water: <span id="w-val">--</span>%</div>
          <div class="card" style="padding:10px">Soil: <span id="s-val">--</span>%</div>
          <div class="card" style="padding:10px">Temp: <span id="t-val">--</span>C</div>
          <div class="card" style="padding:10px">Link: <span id="sig">--</span>dBm</div>
        </div>
      </div>
    </div>
    <div class="card"><canvas id="riskChart" height="100"></canvas></div>
  </div>
  <div id="tab1" class="tab">
    <h1>Manual Overrides</h1>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">
      <div class="card"><button class="btn" onclick="ctrl('rel')">Toggle Siren</button><button class="btn" onclick="ctrl('servo')">Test Flag</button></div>
      <div class="card"><h3>System Logs</h3><div class="log-box" id="logs"></div></div>
    </div>
  </div>
  <div id="tab2" class="tab">
    <h1>Mesh Topology</h1>
    <div class="card"><div id="mesh-list"></div></div>
  </div>
</div>
<script>
const charts={risk:new Chart(document.getElementById('riskChart'),{type:'line',data:{labels:[],datasets:[{data:[],borderColor:'#10b981',fill:true,tension:0.4}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:false}},scales:{x:{display:false},y:{suggestedMax:9,grid:{color:'#222'}}}}})};
function showTab(i){
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.nav').forEach(n=>n.classList.remove('active'));
  document.getElementById('tab'+i).classList.add('active');
  document.querySelectorAll('.nav')[i].classList.add('active');
}
async function poll(){
  try{
    const r=await fetch('/data').then(res=>res.json());
    document.getElementById('r-val').innerText=r.risk.toFixed(1);
    document.getElementById('w-val').innerText=r.water.toFixed(1);
    document.getElementById('s-val').innerText=r.soil.toFixed(1);
    document.getElementById('t-val').innerText=r.airT.toFixed(1);
    document.getElementById('sig').innerText=r.rssi;
    document.getElementById('logs').innerText=r.logs.join('\n');
    document.getElementById('mesh-list').innerHTML=r.mesh.map(n=>`<div class="mesh-node ${n.active?'online':''}"><div><b>${n.id}</b><br><small>${n.role}</small></div><div>RSSI: ${n.rssi}dBm</div></div>`).join('');
    const c=charts.risk;c.data.labels.push('');c.data.datasets[0].data.push(r.risk);if(c.data.labels.length>30){c.data.labels.shift();c.data.datasets[0].data.shift();}c.update('none');
  }catch(e){}
}
async function ctrl(t){await fetch(`/ctrl?type=${t}`);}
setInterval(poll,2000);poll();
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
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  
  json += "\"logs\":[";
  for(int i=0; i<LOG_SIZE; i++) {
    int idx = (logHead + i) % LOG_SIZE;
    if(sysLogs[idx].length() > 0) json += "\"" + sysLogs[idx] + "\"" + (i == LOG_SIZE-1 ? "" : ",");
  }
  json += "],";

  json += "\"mesh\":[";
  for(int i=0; i<3; i++) {
    json += "{\"id\":\""+mesh[i].id+"\",\"role\":\""+mesh[i].role+"\",\"rssi\":"+String(mesh[i].rssi)+",\"active\":"+(mesh[i].active?"true":"false")+"}";
    if(i<2) json += ",";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleCtrl() {
  String type = server.arg("type");
  if(type == "rel") { digitalWrite(PIN_RELAY, !digitalRead(PIN_RELAY)); addLog("Relay Toggled"); }
  else if(type == "servo") { servo.write(servo.read() == 0 ? 90 : 0); addLog("Servo Tested"); }
  server.send(200, "text/plain", "OK");
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  
  esp_task_wdt_init(10, true); esp_task_wdt_add(NULL);
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
    
    // Sim mesh noise
    mesh[1].rssi = -60 + random(-5,5);
    mesh[2].active = (d.risk < 8);
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
