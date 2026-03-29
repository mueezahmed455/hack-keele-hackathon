// ================================================================
//  GEO-SENSE AFRICA v4.3 — ARDUINO UNO SLAVE NODE
//  Multi-Hazard Early Warning System — Edge Processing Unit
// ================================================================
//
//  SERIAL PROTOCOL (CORE ESSENTIALS + TELEMETRY)
//  ─────────────────────────────────────────────────────────────
//  TX Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
//  Local Debug: [SLAVE] Water: XX% | Soil Temp: XXC | Slope: XX% | Pot: X.XX | Tilt: X
//  Packet Log:  [TX] W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
// ================================================================

#include <avr/wdt.h>

// ════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ════════════════════════════════════════════════════════════
#define PIN_WATER_LEVEL     A1
#define PIN_THERMISTOR      A2
#define PIN_POTENTIOMETER   A4
#define PIN_TILT_SWITCH     2   // INT0
#define PIN_BUZZER_ACTIVE   3
#define PIN_BUZZER_PASSIVE  11

// ════════════════════════════════════════════════════════════
//  CONFIGURATION
// ════════════════════════════════════════════════════════════
#define THERMISTOR_NOMINAL  10000.0f
#define SERIES_RESISTOR     10000.0f
#define BCOEFFICIENT        3950.0f
#define TEMP_NOMINAL        298.15f

#define TX_INTERVAL_MS      2000UL
#define DEBUG_INTERVAL_MS   2000UL
#define SAMPLE_INTERVAL_MS   150UL
#define EMA_ALPHA           0.3f

// ════════════════════════════════════════════════════════════
//  STATE
// ════════════════════════════════════════════════════════════
volatile bool     tiltTriggered = false;
volatile uint32_t tiltMs        = 0;
volatile uint32_t lastTiltEventMs = 0;

float waterLevel     = 0.0f;
float thermistorTemp = 25.0f;
float potCalibration = 1.0f;
float slopePct       = 0.0f;
float ema_water      = 0.0f;

static uint32_t lastTxMs     = 0;
static uint32_t lastDebugMs  = 0;
static uint32_t lastSampleMs = 0;
static uint32_t lastBuzMs    = 0;
static int      cmdAlertLevel = 0;
static bool     buzzerWasOn   = false;

char serialBuffer[32];
uint8_t serialIdx = 0;

// ════════════════════════════════════════════════════════════
//  ISR
// ════════════════════════════════════════════════════════════
void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs = millis();
  lastTiltEventMs = tiltMs;
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  wdt_enable(WDTO_4S);
  Serial.begin(9600);

  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH), LANDSLIDE_ISR, FALLING);

  pinMode(PIN_BUZZER_ACTIVE,  OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  noTone(PIN_BUZZER_PASSIVE);

  Serial.println(F("[SYS] GEO-SENSE Slave v4.3 Ready"));
  wdt_reset();
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  wdt_reset();
  const uint32_t now = millis();

  // ── TILT LOGIC ────────────────────────────────────────────
  bool tiltNow;
  {
    noInterrupts();
    tiltNow = tiltTriggered;
    interrupts();
  }

  if (tiltNow && (now - tiltMs > 15000UL)) {
    noInterrupts(); tiltTriggered = false; interrupts();
  }

  // ── SLOPE DECAY ───────────────────────────────────────────
  if (now - lastTiltEventMs > 30000UL) {
    slopePct = max(0.0f, slopePct - 2.0f);
  } else if (tiltNow) {
    slopePct = min(100.0f, slopePct + 30.0f);
  }

  // ── SENSORS ───────────────────────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    
    int rawW = analogRead(PIN_WATER_LEVEL);
    float wPct = constrain((float)map(rawW, 0, 700, 0, 100), 0.0f, 100.0f);
    ema_water = (EMA_ALPHA * wPct) + ((1.0f - EMA_ALPHA) * ema_water);
    waterLevel = ema_water;

    int rawT = analogRead(PIN_THERMISTOR);
    if (rawT > 10 && rawT < 1010) {
      float res = SERIES_RESISTOR * ((1023.0f / (float)rawT) - 1.0f);
      float invT = (1.0f / TEMP_NOMINAL) + (log(res / THERMISTOR_NOMINAL) / BCOEFFICIENT);
      thermistorTemp = (1.0f / invT) - 273.15f;
    }

    int rawP = analogRead(PIN_POTENTIOMETER);
    potCalibration = (float)map(rawP, 0, 1023, 50, 200) / 100.0f;
  }

  // ── SERIAL RX ─────────────────────────────────────────────
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer[serialIdx] = '\0';
      if (strncmp(serialBuffer, "CMD:", 4) == 0) cmdAlertLevel = atoi(serialBuffer + 4);
      serialIdx = 0;
    } else if (serialIdx < 31) serialBuffer[serialIdx++] = c;
    else serialIdx = 0;
  }

  // ── ALERTS ────────────────────────────────────────────────
  handleAlerts(tiltNow, now);

  // ── TELEMETRY & TRANSMIT ──────────────────────────────────
  if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
    lastDebugMs = now;
    Serial.print(F("[SLAVE] Water: ")); Serial.print((int)waterLevel);
    Serial.print(F("% | Soil Temp: ")); Serial.print((int)thermistorTemp);
    Serial.print(F("C | Slope: "));      Serial.print((int)slopePct);
    Serial.print(F("% | Pot: "));        Serial.print(potCalibration);
    Serial.print(F(" | Tilt: "));       Serial.println(tiltNow ? 1 : 0);
  }

  if (now - lastTxMs >= TX_INTERVAL_MS) {
    lastTxMs = now;
    transmitToESP32(tiltNow);
  }
}

void handleAlerts(bool tiltNow, uint32_t now) {
  if (tiltNow) {
    if (!buzzerWasOn) {
      tone(PIN_BUZZER_PASSIVE, 2500);
      digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
      buzzerWasOn = true;
    }
    return;
  }

  if (cmdAlertLevel >= 2) {
    if (now - lastBuzMs > 450) {
      lastBuzMs = now;
      static bool phase = false;
      phase = !phase;
      tone(PIN_BUZZER_PASSIVE, phase ? 1800 : 2000, 180);
      buzzerWasOn = true;
    }
  } else if (cmdAlertLevel == 1) {
    if (now - lastBuzMs > 3000) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 880, 120);
      buzzerWasOn = false;
    }
  } else if (buzzerWasOn) {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    buzzerWasOn = false;
  }
}

void transmitToESP32(bool tiltNow) {
  char packet[48];
  int w_i = (int)waterLevel;
  int w_f = (int)fabsf((waterLevel - w_i) * 10.0f) % 10;
  int t_i = (int)thermistorTemp;
  int t_f = (int)fabsf((thermistorTemp - t_i) * 10.0f) % 10;
  int s_i = (int)slopePct;
  int s_f = (int)fabsf((slopePct - s_i) * 10.0f) % 10;
  int p_i = (int)potCalibration;
  int p_f = (int)fabsf((potCalibration - p_i) * 100.0f) % 100;

  snprintf(packet, sizeof(packet), "W:%d.%d,T:%d.%d,S:%d.%d,P:%d.%02d,I:%d",
           w_i, w_f, t_i, t_f, s_i, s_f, p_i, p_f, tiltNow ? 1 : 0);

  Serial.print(F("[TX] "));
  Serial.println(packet);
}
