// ================================================================
//  GEO-SENSE AFRICA v5.0 — ARDUINO UNO SLAVE NODE (FINAL)
//  Multi-Hazard Early Warning System — Edge Processing Unit
// ================================================================
//
//  SERIAL PROTOCOL (v5.0 CORE)
//  ─────────────────────────────────────────────────────────────
//  TX Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
//  Role: High-frequency sensor fusion & autonomous fail-safe alerts.
// ================================================================

#include <avr/wdt.h>

// ── PINS ─────────────────────────────────────────────────────
#define PIN_WATER_LEVEL     A1
#define PIN_THERMISTOR      A2
#define PIN_POTENTIOMETER   A4
#define PIN_TILT_SWITCH     2   // INT0
#define PIN_BUZZER_ACTIVE   3
#define PIN_BUZZER_PASSIVE  11

// ── CONFIG ───────────────────────────────────────────────────
#define THERMISTOR_NOMINAL  10000.0f
#define SERIES_RESISTOR     10000.0f
#define BCOEFFICIENT        3950.0f
#define TEMP_NOMINAL        298.15f

#define TX_INTERVAL         2000UL
#define SAMPLE_INTERVAL      150UL
#define EMA_ALPHA           0.3f

// ── STATE ────────────────────────────────────────────────────
volatile bool     tiltTriggered = false;
volatile uint32_t tiltMs        = 0;
volatile uint32_t lastTiltMs    = 0;

float waterLevel = 0.0f, thermistorTemp = 25.0f, potCalibration = 1.0f, slopePct = 0.0f, ema_water = 0.0f;
uint32_t lastTx = 0, lastSample = 0, lastBuz = 0;
int cmdAlert = 0;
bool buzzerOn = false;

char rxBuf[32];
uint8_t rxIdx = 0;

void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs = millis();
  lastTiltMs = tiltMs;
}

void setup() {
  wdt_enable(WDTO_4S);
  Serial.begin(9600);

  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH), LANDSLIDE_ISR, FALLING);

  pinMode(PIN_BUZZER_ACTIVE,  OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  noTone(PIN_BUZZER_PASSIVE);

  Serial.println(F("{\"node\":\"slave\",\"status\":\"online\",\"ver\":\"5.0\"}"));
}

void loop() {
  wdt_reset();
  const uint32_t now = millis();

  bool tiltNow;
  { noInterrupts(); tiltNow = tiltTriggered; interrupts(); }
  if (tiltNow && (now - tiltMs > 15000UL)) { noInterrupts(); tiltTriggered = false; interrupts(); }

  // Slope Decay
  if (now - lastTiltMs > 30000UL) slopePct = max(0.0f, slopePct - 2.0f);
  else if (tiltNow) slopePct = min(100.0f, slopePct + 30.0f);

  // Sampling
  if (now - lastSample >= SAMPLE_INTERVAL) {
    lastSample = now;
    float wRaw = (float)map(analogRead(PIN_WATER_LEVEL), 0, 700, 0, 100);
    ema_water = (EMA_ALPHA * constrain(wRaw,0,100)) + ((1.0f - EMA_ALPHA) * ema_water);
    waterLevel = ema_water;

    int tRaw = analogRead(PIN_THERMISTOR);
    if (tRaw > 10 && tRaw < 1010) {
      float res = SERIES_RESISTOR * ((1023.0f / (float)tRaw) - 1.0f);
      float invT = (1.0f / TEMP_NOMINAL) + (log(res / THERMISTOR_NOMINAL) / BCOEFFICIENT);
      thermistorTemp = (1.0f / invT) - 273.15f;
    }
    potCalibration = (float)map(analogRead(PIN_POTENTIOMETER), 0, 1023, 50, 200) / 100.0f;
  }

  // Serial RX
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      rxBuf[rxIdx] = '\0';
      if (strncmp(rxBuf, "CMD:", 4) == 0) cmdAlert = atoi(rxBuf + 4);
      rxIdx = 0;
    } else if (rxIdx < 31) rxBuf[rxIdx++] = c;
  }

  // Alerts (Autonomous Fail-safe)
  if (tiltNow) {
    if (!buzzerOn) { tone(PIN_BUZZER_PASSIVE, 2500); digitalWrite(PIN_BUZZER_ACTIVE, HIGH); buzzerOn = true; }
  } else {
    if (cmdAlert >= 2) {
      if (now - lastBuz > 450) { lastBuz = now; static bool p=0; tone(PIN_BUZZER_PASSIVE, (p=!p)?1800:2000, 180); buzzerOn=1; }
    } else if (cmdAlert == 1) {
      if (now - lastBuz > 3000) { lastBuz = now; tone(PIN_BUZZER_PASSIVE, 880, 120); buzzerOn=0; }
    } else if (buzzerOn) { noTone(PIN_BUZZER_PASSIVE); digitalWrite(PIN_BUZZER_ACTIVE, LOW); buzzerOn=0; }
  }

  // Transmit
  if (now - lastTx >= TX_INTERVAL) {
    lastTx = now;
    Serial.print(F("W:")); Serial.print(waterLevel,1);
    Serial.print(F(",T:")); Serial.print(thermistorTemp,1);
    Serial.print(F(",S:")); Serial.print(slopePct,1);
    Serial.print(F(",P:")); Serial.print(potCalibration,2);
    Serial.print(F(",I:")); Serial.println(tiltNow?1:0);
  }
}
