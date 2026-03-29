// ================================================================
//  GEO-SENSE AFRICA v4.7 — ESP32 MASTER NODE (LEGACY STABLE)
//  Multi-Hazard Early Warning System — Single Page Edition
// ================================================================
//  FIXES IN v4.7:
//  - [FIX] Complete Slave Parser: Sync with v4.4 Arduino Slave (W, T, S, P, I)
//  - [FIX] Robust Risk Fusion: EMA-smoothed sensors + Terrain Cal
//  - [FIX] Heartbeat Sync: Reliable ESP32-Arduino CMD protocol
//  - [FIX] UI Refresh: Optimized Gauge logic for real-time response
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
const char* AP_SSID = "GeoSense-Africa-Legacy";
const char* AP_PASSWORD = "geosense2024";

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_DHT 4
#define PIN_TRIG 5
#define PIN_ECHO 18
#define PIN_SOIL 36
#define PIN_SERVO 13
#define PIN_RELAY 12
#define PIN_TOUCH 14
#define SLAVE_RX 16
#define SLAVE_TX 17

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHT11);
Servo servo;
WebServer server(80);

struct SensorData {
  float soil, airT, water, tilt, thermT, pot, risk;
  bool tiltInt;
  char hazard[12];
  int alert;
} d = {50,25,0,0,25,1,0,false,"NOMINAL",0};

float ema_soil=50, ema_water=0, ema_tilt=0;
#define EMA_A 0.25f

uint32_t lastSample=0, lastDisplay=0, bootMs=0;
char slaveBuf[80];
uint8_t slaveIdx = 0;

// ── HTML ─────────────────────────────────────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Geo-Sense Legacy v4.7</title>
<style>body{background:#0a0e1a;color:#f1f5f9;font-family:sans-serif;padding:20px;text-align:center}
.card{background:#111827;border:1px solid #1e293b;border-radius:12px;padding:20px;margin:10px auto;max-width:500px}
.risk{font-size:64px;font-weight:bold;color:#10b981}
.val{color:#3b82f6;font-weight:bold}</style></head>
<body><h1>GEO-SENSE LEGACY v4.7</h1>
<div class="card">RISK INDEX<br><span class="risk" id="risk">0.0</span>/9.0<br><small id="haz">NOMINAL</small></div>
<div class="card">
  Water: <span class="val" id="w">0</span>% | 
  Soil: <span class="val" id="s">0</span>% | 
  Temp: <span class="val" id="t">0</span>C
</div>
<script>async function poll(){try{const r=await fetch('/data').then(res=>res.json());
document.getElementById('risk').innerText=r.risk.toFixed(1);
document.getElementById('haz').innerText=r.hazard;
document.getElementById('w').innerText=r.water.toFixed(0);
document.getElementById('s').innerText=r.soil.toFixed(0);
document.getElementById('t').innerText=r.airT.toFixed(0);}catch(e){}}
setInterval(poll,2000);poll();</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200); Serial2.begin(9600, SERIAL_8N1, SLAVE_RX, SLAVE_TX);
  esp_task_wdt_init(10, true); esp_task_wdt_add(NULL); Wire.begin(PIN_SDA, PIN_SCL);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, HIGH);
  if(oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { oled.clearDisplay(); oled.display(); }
  lcd.init(); lcd.backlight(); dht.begin(); servo.attach(PIN_SERVO); servo.write(0);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
  server.on("/data", [](){
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"risk\":%.2f,\"hazard\":\"%s\",\"water\":%.1f,\"soil\":%.1f,\"airT\":%.1f}", d.risk, d.hazard, d.water, d.soil, d.airT);
    server.send(200, "application/json", buf);
  });
  server.begin(); bootMs = millis();
}

void loop() {
  esp_task_wdt_reset(); server.handleClient();
  uint32_t now = millis();

  while(Serial2.available()) {
    char c = (char)Serial2.read();
    if(c=='\n'||c=='\r') {
      if(slaveIdx>0) {
        slaveBuf[slaveIdx]='\0';
        auto getF = [&](const char* k){ char* p=strstr(slaveBuf,k); return p?atof(p+strlen(k)):-999.0f; };
        float w=getF("W:"), t=getF("T:"), s=getF("S:"), p=getF("P:");
        if(w > -999) d.water=w; if(t > -999) d.thermT=t; if(s > -999) d.tilt=s; if(p > -999) d.pot=p;
        char* ip=strstr(slaveBuf,"I:"); if(ip) d.tiltInt=(atoi(ip+2)==1);
      }
      slaveIdx=0;
    } else if(slaveIdx<79) slaveBuf[slaveIdx++]=c;
  }

  if(now - lastSample >= 3000) {
    lastSample = now;
    float temp = dht.readTemperature(); if(!isnan(temp)) d.airT = temp;
    d.soil = constrain((float)map(analogRead(PIN_SOIL), 3200, 1200, 0, 100), 0, 100);
    ema_soil = EMA_A*d.soil + (1-EMA_A)*ema_soil;
    ema_water = EMA_A*d.water + (1-EMA_A)*ema_water;
    ema_tilt = EMA_A*d.tilt + (1-EMA_A)*ema_tilt;
    float base = (ema_water*0.4 + ema_soil*0.2 + ema_tilt*0.4);
    d.risk = constrain((base * d.pot / 100.0f) * 9.0f / 100.0f, 0, 9);
    d.alert = (d.risk > 7 || d.tiltInt)?2:(d.risk > 4?1:0);
    if(d.tiltInt) { d.risk=9; strcpy(d.hazard, "LANDSLIDE"); }
    else if(ema_water>60) strcpy(d.hazard, "FLOOD"); else if(ema_soil>70) strcpy(d.hazard, "DROUGHT"); else strcpy(d.hazard, "SAFE");
    Serial2.printf("CMD:%d\n", d.alert);
    digitalWrite(PIN_RELAY, (d.alert==2)?LOW:HIGH);
    servo.write(d.alert==2?135:d.alert==1?80:0);
  }

  if(now - lastDisplay >= 500) {
    lastDisplay = now;
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0); oled.printf("R:%.1f [%s]", d.risk, d.hazard);
    oled.setCursor(0,15); oled.printf("W:%.0f S:%.0f T:%.0f", d.water, d.soil, d.airT);
    oled.display();
  }
}
