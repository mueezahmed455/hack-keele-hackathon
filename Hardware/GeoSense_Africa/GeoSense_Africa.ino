// ============================================================
//  GEO-SENSE AFRICA — FINAL OPTIMIZED & DEBUG VERSION (v1.2)
//  Hardware: LAFVIN R3 (Arduino Uno compatible)
//  
//  How to test:
//  1. Open Serial Monitor (9600 baud) BEFORE uploading
//  2. Upload this code
//  3. Watch the Serial Monitor — it will tell you exactly what is happening
//  4. Tilt the switch only when you want a landslide alert
//  
//  Dependencies: DHT sensor library by Adafruit
// ============================================================

#include <DHT.h>

// ── PIN DEFINITIONS ─────────────────────────────────────────
#define PIN_SOIL_HUMIDITY   A0
#define PIN_WATER_LEVEL     A1
#define PIN_THERMISTOR      A2
#define PIN_PHOTORESISTOR   A3
#define PIN_POTENTIOMETER   A4

#define PIN_DHT11           7
#define PIN_OBSTACLE        8
#define PIN_TILT_SWITCH     2     // INT0

#define PIN_BUZZER_ACTIVE   3
#define PIN_LED_RED         9
#define PIN_LED_GREEN       10
#define PIN_LED_BLUE        11

// 7-Segment (common cathode, G segment not connected)
#define SEG_A  4
#define SEG_B  5
#define SEG_C  6
#define SEG_D  12
#define SEG_E  13
#define SEG_F  A5

// ── CONSTANTS ───────────────────────────────────────────────
#define DHT_TYPE            DHT11
#define SAMPLE_INTERVAL_MS  2000      // Fast enough for real-time feel
#define EMA_ALPHA           0.3f
#define HYSTERESIS_COUNT    3
#define THERMISTOR_NOMINAL  10000
#define SERIES_RESISTOR     10000
#define BCOEFFICIENT        3950
#define TEMP_NOMINAL        25

#define RISK_GREEN_MAX      3
#define RISK_AMBER_MAX      6
#define RISK_RED_MIN        7

#define W_SOIL              0.30f
#define W_WATER             0.35f
#define W_TILT              0.25f
#define W_TEMP              0.10f

#define FREQ_LANDSLIDE      2500
#define FREQ_FLOOD          1800
#define FREQ_DROUGHT        800

// ── GLOBALS ─────────────────────────────────────────────────
DHT dht(PIN_DHT11, DHT_TYPE);

float ema_soil  = 0.0f;
float ema_water = 0.0f;
float ema_tilt  = 0.0f;
float ema_temp  = 0.0f;

int   alert_count = 0;
volatile bool tilt_triggered = false;
volatile unsigned long last_tilt_time = 0;   // For debounce

unsigned long last_sample_ms = 0;

// ── 7-SEGMENT LOOKUP TABLE ───────────────────────────────
const byte SEG_DIGITS[10] = {
  0b111111, // 0
  0b011000, // 1
  0b110110, // 2
  0b111100, // 3
  0b011001, // 4
  0b101101, // 5
  0b101111, // 6
  0b111000, // 7
  0b111111, // 8
  0b111101  // 9
};

const int SEG_PINS[6] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F};

// ── INTERRUPT SERVICE ROUTINE (with debounce) ─────────────
void LANDSLIDE_ISR() {
  unsigned long current = millis();
  if (current - last_tilt_time > 100) {        // 100ms debounce
    tilt_triggered = true;
    last_tilt_time = current;
  }
}

// ── SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.println(F("=== GEO-SENSE AFRICA FINAL v1.2 ==="));
  Serial.println(F("Serial debug ENABLED - watch this window!"));
  Serial.println(F("Initialising..."));

  dht.begin();

  pinMode(PIN_OBSTACLE, INPUT);
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH), LANDSLIDE_ISR, FALLING);

  pinMode(PIN_BUZZER_ACTIVE, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  for (int i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Boot animation
  for (int d = 0; d <= 9; d++) { displayDigit(d); delay(80); }
  for (int d = 9; d >= 0; d--) { displayDigit(d); delay(80); }

  setRGB(255,0,0);   delay(200);
  setRGB(0,255,0);   delay(200);
  setRGB(0,0,255);   delay(200);
  setRGB(255,255,255); delay(200);
  setRGB(0,0,0);

  tone(PIN_BUZZER_ACTIVE, 1000, 100); delay(200);
  tone(PIN_BUZZER_ACTIVE, 1500, 100); delay(300);
  noTone(PIN_BUZZER_ACTIVE);

  displayDigit(0);
  Serial.println(F("SYSTEM READY - Monitoring started"));
  Serial.println(F("--------------------------------------------------"));
}

// ── MAIN LOOP ─────────────────────────────────────────────
void loop() {
  // ── FULL SERIAL DEBUG AT START OF EVERY CYCLE ─────────
  Serial.println(F("=== LOOP CYCLE START ==="));
  Serial.print(F("tilt_triggered flag: ")); Serial.println(tilt_triggered ? F("YES") : F("NO"));

  // ── PRIORITY 1: LANDSLIDE ALERT ───────────────────────
  if (tilt_triggered) {
    Serial.println(F("!!! LANDSLIDE INTERRUPT DETECTED !!!"));
    triggerLandslideAlert();
    tilt_triggered = false;
    alert_count = 0;
    Serial.println(F("Landslide alert finished - returning to normal loop"));
    return;
  }

  // ── TIMED SENSOR SAMPLING ─────────────────────────────
  unsigned long now = millis();
  if (now - last_sample_ms < SAMPLE_INTERVAL_MS) return;
  last_sample_ms = now;

  // ── READ ALL SENSORS ──────────────────────────────────
  int raw_soil = analogRead(PIN_SOIL_HUMIDITY);
  float soil_pct = map(raw_soil, 1023, 300, 0, 100);
  soil_pct = constrain(soil_pct, 0, 100);

  int raw_water = analogRead(PIN_WATER_LEVEL);
  float water_pct = map(raw_water, 0, 700, 0, 100);
  water_pct = constrain(water_pct, 0, 100);

  bool obstacle_detected = (digitalRead(PIN_OBSTACLE) == LOW);
  if (obstacle_detected) water_pct = min(100.0f, water_pct + 15.0f);

  float air_temp = dht.readTemperature();
  float air_humidity = dht.readHumidity();
  if (isnan(air_temp)) air_temp = 28.0f;
  if (isnan(air_humidity)) air_humidity = 60.0f;

  int raw_therm = analogRead(PIN_THERMISTOR);
  float resistance = SERIES_RESISTOR * ((1023.0f / raw_therm) - 1.0f);
  float steinhart = log(resistance / THERMISTOR_NOMINAL);
  steinhart /= BCOEFFICIENT;
  steinhart += 1.0f / (TEMP_NOMINAL + 273.15f);
  float soil_temp = (1.0f / steinhart) - 273.15f;
  if (soil_temp < -20 || soil_temp > 80) soil_temp = 25.0f;

  float temp_delta = fabs(air_temp - soil_temp);
  float temp_pct = constrain(map((int)temp_delta, 0, 15, 0, 100), 0, 100);

  int raw_pot = analogRead(PIN_POTENTIOMETER);
  float sensitivity = map(raw_pot, 0, 1023, 50, 200) / 100.0f;

  bool current_tilt = (digitalRead(PIN_TILT_SWITCH) == LOW);
  float tilt_pct = current_tilt ? 100.0f : 0.0f;

  int raw_photo = analogRead(PIN_PHOTORESISTOR);
  float solar_pct = map(raw_photo, 0, 1023, 0, 100);

  // ── EMA FILTERING ─────────────────────────────────────
  ema_soil  = EMA_ALPHA * soil_pct  + (1.0f - EMA_ALPHA) * ema_soil;
  ema_water = EMA_ALPHA * water_pct + (1.0f - EMA_ALPHA) * ema_water;
  ema_tilt  = EMA_ALPHA * tilt_pct  + (1.0f - EMA_ALPHA) * ema_tilt;
  ema_temp  = EMA_ALPHA * temp_pct  + (1.0f - EMA_ALPHA) * ema_temp;

  // ── RISK CALCULATION ──────────────────────────────────
  float weighted = (ema_soil * W_SOIL) + (ema_water * W_WATER) +
                   (ema_tilt * W_TILT) + (ema_temp * W_TEMP);
  weighted = weighted * sensitivity;
  weighted = constrain(weighted, 0.0f, 100.0f);

  int risk_index = (int)map((int)weighted, 0, 100, 0, 9);

  // ── SERIAL DATA DUMP ──────────────────────────────────
  Serial.print(F("Soil Moisture: ")); Serial.print(ema_soil, 1); Serial.println(F("% (0=dry,100=wet)"));
  Serial.print(F("Water Level:   ")); Serial.print(ema_water, 1); Serial.println(F("%"));
  Serial.print(F("Tilt:          ")); Serial.println(current_tilt ? F("TILTED") : F("stable"));
  Serial.print(F("Temp Delta:    ")); Serial.print(temp_delta, 1); Serial.println(F("°C"));
  Serial.print(F("Sensitivity:   ")); Serial.print(sensitivity, 2); Serial.println(F("x"));
  Serial.print(F("WEIGHTED RISK: ")); Serial.print(weighted, 1); Serial.print(F("/100  →  INDEX: "));
  Serial.println(risk_index);

  // ── DISPLAY RISK ──────────────────────────────────────
  displayDigit(risk_index);

  // ── HYSTERESIS & HAZARD TYPE ──────────────────────────
  if (risk_index >= RISK_RED_MIN) alert_count++;
  else alert_count = max(0, alert_count - 1);

  bool flood_dominant   = (ema_water > ema_soil && ema_water > 60);
  bool drought_dominant = (ema_soil < 30.0f && ema_water < 30.0f);

  // ── OUTPUTS ───────────────────────────────────────────
  if (risk_index <= RISK_GREEN_MAX) {
    setRGB(0, 255, 0);
    noTone(PIN_BUZZER_ACTIVE);
    Serial.println(F("STATUS: NOMINAL ✓"));
  }
  else if (risk_index <= RISK_AMBER_MAX) {
    pulseAmber();
    if (drought_dominant) {
      tone(PIN_BUZZER_ACTIVE, FREQ_DROUGHT, 200);
      delay(200);
      noTone(PIN_BUZZER_ACTIVE);
    } else if (flood_dominant) {
      tone(PIN_BUZZER_ACTIVE, FREQ_FLOOD, 300);
      delay(300);
      noTone(PIN_BUZZER_ACTIVE);
    }
    Serial.println(F("STATUS: WARNING ⚠"));
  }
  else {
    if (alert_count >= HYSTERESIS_COUNT) {
      triggerCriticalAlert(flood_dominant, drought_dominant);
      Serial.println(F("STATUS: CRITICAL ALERT !!!"));
    } else {
      setRGB(255, 50, 0);
      Serial.print(F("STATUS: PRE-ALERT (count="));
      Serial.print(alert_count);
      Serial.println(F(")"));
    }
  }

  if (drought_dominant) {
    Serial.print(F("DROUGHT WARNING: Wilting risk = "));
    Serial.print(100.0f - ema_soil, 1);
    Serial.println(F("%"));
  }

  Serial.println(F("=== LOOP CYCLE END ===\n"));
}

// ── ALERT FUNCTIONS ───────────────────────────────────────
void triggerLandslideAlert() {
  Serial.println(F("!!! LANDSLIDE ALERT ACTIVE !!!"));
  displayDigit(9);

  for (int i = 0; i < 25; i++) {
    tone(PIN_BUZZER_ACTIVE, FREQ_LANDSLIDE);
    setRGB(255, 0, 0);
    delay(250);
    setRGB(0, 0, 0);
    delay(150);
  }
  noTone(PIN_BUZZER_ACTIVE);
  setRGB(0, 0, 0);
  displayDigit(9);
}

void triggerCriticalAlert(bool flood, bool drought) {
  noTone(PIN_BUZZER_ACTIVE);   // safety
  if (flood) {
    setRGB(0, 0, 255);
    tone(PIN_BUZZER_ACTIVE, FREQ_FLOOD, 500);
    delay(500);
    setRGB(0, 80, 180);
    delay(300);
  } else {
    setRGB(255, 0, 0);
    tone(PIN_BUZZER_ACTIVE, FREQ_LANDSLIDE, 300);
    delay(300);
    setRGB(0, 0, 0);
    delay(150);
    setRGB(255, 0, 0);
    delay(300);
  }
  noTone(PIN_BUZZER_ACTIVE);
}

void pulseAmber() {
  for (int b = 0; b <= 255; b += 20) {
    setRGB(b, (int)(b * 0.65f), 0);
    delay(4);
  }
  for (int b = 255; b >= 0; b -= 20) {
    setRGB(b, (int)(b * 0.65f), 0);
    delay(4);
  }
}

// ── HELPERS ───────────────────────────────────────────────
void setRGB(int r, int g, int b) {
  analogWrite(PIN_LED_RED,   r);
  analogWrite(PIN_LED_GREEN, g);
  analogWrite(PIN_LED_BLUE,  b);
}

void displayDigit(int d) {
  if (d < 0 || d > 9) d = 0;
  byte pattern = SEG_DIGITS[d];
  for (int i = 0; i < 6; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> (5 - i)) & 0x01);
  }
}

// ── END OF FILE ───────────────────────────────────────────
// GEO-SENSE AFRICA v1.2 — FINAL VERSION
// Open Serial Monitor (9600) to see exactly what the board is doing.
