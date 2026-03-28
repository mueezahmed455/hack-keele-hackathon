// ============================================================
//  GEO-SENSE AFRICA — Multi-Hazard Early Warning System
//  Hardware: LAFVIN R3 (Arduino Uno compatible)
//  Kit: Soil Humidity, DHT11, Water Level, Obstacle, Tilt,
//       Potentiometer, Active Buzzer, RGB LED, 7-Seg Display,
//       Thermistor, Photoresistor
// ============================================================

#include <DHT.h>
// Install: Sketch > Include Library > Manage Libraries > "DHT sensor library" by Adafruit

// ── PIN DEFINITIONS ─────────────────────────────────────────

// Sensors (Analogue)
#define PIN_SOIL_HUMIDITY   A0   // Soil Humidity Sensor (signal pin)
#define PIN_WATER_LEVEL     A1   // Water Level Detection Sensor
#define PIN_THERMISTOR      A2   // Thermistor (soil temperature probe)
#define PIN_PHOTORESISTOR   A3   // Photoresistor (ambient light / solar)
#define PIN_POTENTIOMETER   A4   // Potentiometer (sensitivity calibration)

// Sensors (Digital)
#define PIN_DHT11           7    // DHT11 Temperature & Humidity
#define PIN_OBSTACLE        8    // Obstacle Avoidance Module (flood debris)
#define PIN_TILT_SWITCH     2    // Tilt Switch → Hardware Interrupt (INT0)

// Outputs
#define PIN_BUZZER_ACTIVE   3    // Active Buzzer (PWM capable)
#define PIN_LED_RED         9    // RGB LED — Red channel (PWM)
#define PIN_LED_GREEN       10   // RGB LED — Green channel (PWM)
#define PIN_LED_BLUE        11   // RGB LED — Blue channel (PWM)

// 7-Segment Display (common cathode, segments A–G + DP)
// Wired: A=4, B=5, C=6, D=12, E=13, F=A5, G=1(TX — avoid if Serial used)
// SIMPLIFIED: We use a 4-bit BCD approach — drive segments directly
#define SEG_A  4
#define SEG_B  5
#define SEG_C  6
#define SEG_D  12
#define SEG_E  13
#define SEG_F  A5
// NOTE: Pin 1 (TX) reserved for Serial — G segment tied LOW (segments A-F only)
// Digits 0-9 still readable with 6 segments

// Sensor power control (saves battery between readings)
#define PIN_SENSOR_POWER    -1   // Set to a digital pin if you wire a MOSFET switch
                                 // Leave -1 if sensors share 5V directly

// ── CONSTANTS ───────────────────────────────────────────────

#define DHT_TYPE            DHT11
#define SAMPLE_INTERVAL_MS  5000    // 5 seconds between readings
#define EMA_ALPHA           0.3f    // Exponential moving average factor
#define HYSTERESIS_COUNT    3       // Consecutive readings before alert fires
#define THERMISTOR_NOMINAL  10000   // Thermistor resistance at 25°C (10kΩ)
#define SERIES_RESISTOR     10000   // Series resistor value (10kΩ from kit)
#define BCOEFFICIENT        3950    // Beta coefficient for NTC thermistor
#define TEMP_NOMINAL        25      // Nominal temperature (°C)

// Risk thresholds (0–9 scale)
#define RISK_GREEN_MAX      3
#define RISK_AMBER_MAX      6
#define RISK_RED_MIN        7

// Sensor weights (must sum to 1.0)
#define W_SOIL              0.30f
#define W_WATER             0.35f
#define W_TILT              0.25f
#define W_TEMP              0.10f

// Alert frequencies
#define FREQ_LANDSLIDE      2500   // Hz — max urgency
#define FREQ_FLOOD          1800   // Hz — high urgency
#define FREQ_DROUGHT        800    // Hz — warning tone

// ── GLOBALS ─────────────────────────────────────────────────

DHT dht(PIN_DHT11, DHT_TYPE);

// Smoothed sensor values (EMA filtered, 0–100 scale)
float ema_soil    = 0.0f;
float ema_water   = 0.0f;
float ema_tilt    = 0.0f;
float ema_temp    = 0.0f;

// Hysteresis counter
int   alert_count = 0;

// Interrupt flag
volatile bool tilt_triggered = false;

// Previous risk for display refresh
int   last_risk   = -1;

// Timing
unsigned long last_sample_ms = 0;

// ── 7-SEGMENT LOOKUP TABLE (common cathode, segments A-F) ───
//    Bit order: A B C D E F (G tied LOW)
//    0=segment OFF, 1=segment ON
//    Digits using only 6 segments (no G):
//    0→ABCDEF, 1→BC, 2→ABDEG≈ABDE, 3→ABCDG≈ABCD,
//    4→BCFG≈BCF, 5→ACDFG≈ACDF, 6→ACDEFG≈ACDEF,
//    7→ABC, 8→ABCDEF, 9→ABCDF

const byte SEG_DIGITS[10] = {
  0b111111, // 0: A B C D E F
  0b000110, // 1: B C
  0b110011, // 2: A B D E  (no G — approximation, readable)
  0b100111, // 3: A B C D  (no G)
  0b001110, // 4: B C F    (no G — shows as lower-right + left)
  0b101101, // 5: A C D F  (no G)
  0b111101, // 6: A C D E F (no G)
  0b000111, // 7: A B C
  0b111111, // 8: A B C D E F (same as 0 without G — acceptable)
  0b101111, // 9: A B C D F
};

const int SEG_PINS[6] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// ── INTERRUPT SERVICE ROUTINE ────────────────────────────────

void LANDSLIDE_ISR() {
  tilt_triggered = true;
}

// ── SETUP ────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== GEO-SENSE AFRICA MHEWS BOOT ==="));
  Serial.println(F("Initialising sensor array..."));

  // DHT11
  dht.begin();

  // Digital sensor pins
  pinMode(PIN_OBSTACLE, INPUT);

  // Tilt switch — hardware interrupt (falling = contact closure = slope moved)
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH), LANDSLIDE_ISR, FALLING);

  // Output pins
  pinMode(PIN_BUZZER_ACTIVE, OUTPUT);
  pinMode(PIN_LED_RED,       OUTPUT);
  pinMode(PIN_LED_GREEN,     OUTPUT);
  pinMode(PIN_LED_BLUE,      OUTPUT);

  // 7-Segment pins
  for (int i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Sensor power pin (optional)
  if (PIN_SENSOR_POWER >= 0) {
    pinMode(PIN_SENSOR_POWER, OUTPUT);
    digitalWrite(PIN_SENSOR_POWER, LOW);
  }

  // Boot animation: sweep 0 → 9 → 0 on 7-seg
  Serial.println(F("Self-test: 7-segment sweep..."));
  for (int d = 0; d <= 9; d++) { displayDigit(d); delay(80); }
  for (int d = 9; d >= 0; d--) { displayDigit(d); delay(80); }

  // Boot LED flash: R → G → B → White → OFF
  setRGB(255, 0,   0);   delay(200);
  setRGB(0,   255, 0);   delay(200);
  setRGB(0,   0,   255); delay(200);
  setRGB(255, 255, 255); delay(200);
  setRGB(0,   0,   0);

  // Boot buzzer chirp (2 short beeps)
  tone(PIN_BUZZER_ACTIVE, 1000, 100); delay(200);
  tone(PIN_BUZZER_ACTIVE, 1500, 100); delay(300);
  noTone(PIN_BUZZER_ACTIVE);

  displayDigit(0);
  Serial.println(F("GEO-SENSE ONLINE. Monitoring started."));
  Serial.println(F("--------------------------------------------------"));
}

// ── MAIN LOOP ────────────────────────────────────────────────

void loop() {

  // ── PRIORITY 1: TILT INTERRUPT (LANDSLIDE) ──────────────
  if (tilt_triggered) {
    triggerLandslideAlert();
    // Hold alert for 10 seconds then reset flag
    delay(10000);
    tilt_triggered = false;
    noTone(PIN_BUZZER_ACTIVE);
    alert_count = 0;
    return; // Skip normal processing this cycle
  }

  // ── PRIORITY 2: TIMED SENSOR SAMPLING ───────────────────
  unsigned long now = millis();
  if (now - last_sample_ms < SAMPLE_INTERVAL_MS) return;
  last_sample_ms = now;

  // Power sensors if using MOSFET switch
  if (PIN_SENSOR_POWER >= 0) {
    digitalWrite(PIN_SENSOR_POWER, HIGH);
    delay(150); // Settle time
  }

  // ── READ SENSORS ────────────────────────────────────────

  // 1. Soil Moisture (inverted: high raw = dry)
  int raw_soil = analogRead(PIN_SOIL_HUMIDITY);
  float soil_pct = map(raw_soil, 1023, 300, 0, 100); // Wet ~300, Dry ~1023
  soil_pct = constrain(soil_pct, 0, 100);

  // 2. Water Level (direct: high raw = high water)
  int raw_water = analogRead(PIN_WATER_LEVEL);
  float water_pct = map(raw_water, 0, 700, 0, 100);
  water_pct = constrain(water_pct, 0, 100);

  // 3. Obstacle sensor (debris = flood indicator bonus)
  bool obstacle_detected = (digitalRead(PIN_OBSTACLE) == LOW);
  if (obstacle_detected) water_pct = min(100.0f, water_pct + 15.0f); // Debris boosts flood risk

  // 4. DHT11 — Air Temperature & Humidity
  float air_temp = dht.readTemperature();
  float air_humidity = dht.readHumidity();
  if (isnan(air_temp)) air_temp = 28.0;    // Fallback for failed read
  if (isnan(air_humidity)) air_humidity = 60.0;

  // 5. Thermistor — Soil Temperature (Steinhart-Hart simplified)
  int raw_therm = analogRead(PIN_THERMISTOR);
  float resistance = SERIES_RESISTOR * ((1023.0 / raw_therm) - 1.0);
  float steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= BCOEFFICIENT;
  steinhart += 1.0 / (TEMP_NOMINAL + 273.15);
  float soil_temp = (1.0 / steinhart) - 273.15;

  // 6. Temperature Delta (air vs soil → evapotranspiration proxy)
  float temp_delta = abs(air_temp - soil_temp);
  float temp_pct   = constrain(map((int)temp_delta, 0, 15, 0, 100), 0, 100);

  // 7. Tilt angle (potentiometer as sensitivity scaler)
  int   raw_pot    = analogRead(PIN_POTENTIOMETER);
  float sensitivity = map(raw_pot, 0, 1023, 50, 200) / 100.0f; // 0.5× to 2.0×
  // Tilt itself comes from interrupt; here we use pot-scaled value as continuous proxy
  // For display: pot position shows local terrain calibration
  float tilt_pct   = 0.0f; // Set by interrupt only; 0 in normal mode

  // 8. Photoresistor — ambient light (solar health indicator)
  int raw_photo = analogRead(PIN_PHOTORESISTOR);
  float solar_pct = map(raw_photo, 0, 1023, 0, 100);

  // Power sensors OFF
  if (PIN_SENSOR_POWER >= 0) {
    digitalWrite(PIN_SENSOR_POWER, LOW);
  }

  // ── EXPONENTIAL MOVING AVERAGE (noise filter) ───────────
  ema_soil  = EMA_ALPHA * soil_pct  + (1.0f - EMA_ALPHA) * ema_soil;
  ema_water = EMA_ALPHA * water_pct + (1.0f - EMA_ALPHA) * ema_water;
  ema_tilt  = EMA_ALPHA * tilt_pct  + (1.0f - EMA_ALPHA) * ema_tilt;
  ema_temp  = EMA_ALPHA * temp_pct  + (1.0f - EMA_ALPHA) * ema_temp;

  // ── WEIGHTED RISK FORMULA ───────────────────────────────
  float weighted = (ema_soil  * W_SOIL) +
                   (ema_water * W_WATER) +
                   (ema_tilt  * W_TILT) +
                   (ema_temp  * W_TEMP);

  // Apply terrain sensitivity (potentiometer calibration)
  weighted = weighted * sensitivity;
  weighted = constrain(weighted, 0.0f, 100.0f);

  // Map to 0–9 for 7-segment display
  int risk_index = (int)map((int)weighted, 0, 100, 0, 9);

  // ── SERIAL MONITOR OUTPUT ───────────────────────────────
  Serial.println(F("--------------------------------------------------"));
  Serial.print(F("Air Temp:    ")); Serial.print(air_temp, 1); Serial.println(F(" °C"));
  Serial.print(F("Air Humidity:")); Serial.print(air_humidity, 1); Serial.println(F(" %"));
  Serial.print(F("Soil Temp:   ")); Serial.print(soil_temp, 1); Serial.println(F(" °C"));
  Serial.print(F("Soil Moisture:")); Serial.print(ema_soil, 1); Serial.println(F(" % (norm)"));
  Serial.print(F("Water Level: ")); Serial.print(ema_water, 1); Serial.println(F(" % (norm)"));
  Serial.print(F("Temp Delta:  ")); Serial.print(temp_delta, 1); Serial.println(F(" °C (ET proxy)"));
  Serial.print(F("Solar Light: ")); Serial.print(solar_pct, 1); Serial.println(F(" %"));
  Serial.print(F("Obstacle:    ")); Serial.println(obstacle_detected ? F("DETECTED (debris)") : F("Clear"));
  Serial.print(F("Sensitivity: ")); Serial.print(sensitivity, 2); Serial.println(F("x (pot cal.)"));
  Serial.print(F("Weighted:    ")); Serial.print(weighted, 1); Serial.println(F(" / 100"));
  Serial.print(F("RISK INDEX:  ")); Serial.print(risk_index); Serial.println(F(" / 9"));

  // ── DISPLAY RISK INDEX ──────────────────────────────────
  displayDigit(risk_index);

  // ── HYSTERESIS: Alert fires only after N consecutive high readings
  if (risk_index >= RISK_RED_MIN) {
    alert_count++;
  } else {
    alert_count = max(0, alert_count - 1); // Decay slowly
  }

  // ── DETERMINE HAZARD TYPE FOR ALERTS ───────────────────
  bool flood_dominant   = (ema_water > ema_soil && ema_water > 60);
  bool drought_dominant = (ema_soil  > 70 && ema_water < 30);

  // ── SET OUTPUTS ─────────────────────────────────────────
  if (risk_index <= RISK_GREEN_MAX) {
    // GREEN — Nominal
    setRGB(0, 255, 0);
    noTone(PIN_BUZZER_ACTIVE);
    Serial.println(F("STATUS: NOMINAL ✓"));

  } else if (risk_index <= RISK_AMBER_MAX) {
    // AMBER — Warning: pulse LED
    pulseAmber();
    if (drought_dominant) {
      // Short chirp for drought warning
      tone(PIN_BUZZER_ACTIVE, FREQ_DROUGHT, 200);
      delay(200);
      noTone(PIN_BUZZER_ACTIVE);
    } else if (flood_dominant) {
      tone(PIN_BUZZER_ACTIVE, FREQ_FLOOD, 300);
      delay(300);
      noTone(PIN_BUZZER_ACTIVE);
    }
    Serial.println(F("STATUS: WARNING ⚠"));

  } else {
    // RED — Critical alert (if hysteresis threshold met)
    if (alert_count >= HYSTERESIS_COUNT) {
      triggerCriticalAlert(flood_dominant, drought_dominant);
      Serial.println(F("STATUS: CRITICAL ALERT !!!"));
    } else {
      setRGB(255, 50, 0); // Orange-red while building up
      Serial.print(F("STATUS: PRE-ALERT (count=")); Serial.print(alert_count); Serial.println(F(")"));
    }
  }

  // Log drought-specific analysis
  if (drought_dominant) {
    float wilting_risk = (ema_soil / 100.0f) * 100.0f;
    Serial.print(F("DROUGHT: Wilting point risk = "));
    Serial.print(wilting_risk, 1);
    Serial.println(F("% — monitor Maize/Teff fields"));
  }
}

// ── ALERT FUNCTIONS ──────────────────────────────────────────

void triggerLandslideAlert() {
  Serial.println(F("!! HARDWARE INTERRUPT: LANDSLIDE DETECTED !!"));
  Serial.println(F("!! EVACUATE IMMEDIATELY !!"));
  displayDigit(9);

  // Continuous siren at 2500 Hz with LED pulsing RED
  for (int cycle = 0; cycle < 20; cycle++) {
    tone(PIN_BUZZER_ACTIVE, FREQ_LANDSLIDE);
    setRGB(255, 0, 0);
    delay(300);
    setRGB(0, 0, 0);
    delay(100);
  }
  // Then continuous tone
  tone(PIN_BUZZER_ACTIVE, FREQ_LANDSLIDE);
  setRGB(255, 0, 0);
}

void triggerCriticalAlert(bool flood, bool drought) {
  if (flood) {
    // BLUE PULSE — flood
    setRGB(0, 0, 255);
    tone(PIN_BUZZER_ACTIVE, FREQ_FLOOD);
    delay(500);
    setRGB(0, 50, 150);
    delay(200);
  } else {
    // RED FLASH — general critical
    setRGB(255, 0, 0);
    tone(PIN_BUZZER_ACTIVE, FREQ_LANDSLIDE);
    delay(300);
    setRGB(0, 0, 0);
    delay(100);
    setRGB(255, 0, 0);
  }
}

void pulseAmber() {
  // Dim amber pulse — warning but not panic
  for (int b = 0; b <= 255; b += 15) {
    setRGB(b, (int)(b * 0.65), 0);
    delay(5);
  }
  for (int b = 255; b >= 0; b -= 15) {
    setRGB(b, (int)(b * 0.65), 0);
    delay(5);
  }
}

// ── HELPER FUNCTIONS ─────────────────────────────────────────

void setRGB(int r, int g, int b) {
  // Common CATHODE RGB LED: HIGH = ON
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

// ── END OF FILE ───────────────────────────────────────────────
// GEO-SENSE AFRICA v1.0 | Hackathon Build
// Dependencies: DHT sensor library by Adafruit
// ─────────────────────────────────────────────────────────────
