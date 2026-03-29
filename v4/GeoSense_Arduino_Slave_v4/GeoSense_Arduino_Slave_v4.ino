// ================================================================
//  GEO-SENSE AFRICA v4.1 — ARDUINO UNO SLAVE NODE
//  Multi-Hazard Early Warning System — Edge Processing Unit
// ================================================================
//
//  ARCHITECTURE
//  ─────────────────────────────────────────────────────────────
//  Role: Raw sensor normalization, local alert control, watchdog safety
//  Interrupt: Hardware INT0 (D2) for landslide detection
//  Communication: Serial @ 9600 baud to ESP32 Master
//  Safety: AVR Hardware Watchdog (WDTO_4S)
//
//  SERIAL PROTOCOL
//  ─────────────────────────────────────────────────────────────
//  TX Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x
//    W = Water level % (0.0–100.0)
//    T = Soil temperature °C (Steinhart-Hart corrected)
//    S = Slope activity % (spike+decay from tilt events)
//    P = Potentiometer calibration (0.50×–2.00×)
//    I = ISR tilt flag (0 or 1)
//    L = Light intensity % (photoresistor)
//  RX Format: CMD:alert_level
//    0 = SAFE, 1 = CAUTION, 2 = CRITICAL
//
//  v4.1 OPTIMIZATIONS
//  ─────────────────────────────────────────────────────────────
//  [OPT] Consolidated timing macros with consistent naming
//  [OPT] Reduced magic numbers with named constants
//  [OPT] Optimized sensor reading with early-exit guards
//  [OPT] Improved EMA calculation efficiency
//  [OPT] Added sensor validation ranges
//  [FIX] lastTiltEventMs volatile — prevents stale cache
//  [FIX] Non-blocking buzzer — no delay() in critical path
//  [FIX] Serial buffer overflow — clean reset with log
// ================================================================

#include <avr/wdt.h>

// ════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ════════════════════════════════════════════════════════════

// Analog sensors
#define PIN_WATER_LEVEL     A1
#define PIN_THERMISTOR      A2
#define PIN_PHOTORESISTOR   A3
#define PIN_POTENTIOMETER   A4

// Digital sensors & outputs
#define PIN_TILT_SWITCH     2   // INT0 — hardware interrupt
#define PIN_BUZZER_ACTIVE   3   // Active buzzer (DC-driven)
#define PIN_BUZZER_PASSIVE  11  // Passive buzzer (PWM/tone)

// 7-segment display pins (common cathode, segments A-F)
#define SEG_A  4
#define SEG_B  5
#define SEG_C  6
#define SEG_D  12
#define SEG_E  13
#define SEG_F  A5

// ════════════════════════════════════════════════════════════
//  THERMISTOR CONFIGURATION (Steinhart-Hart B-parameter)
// ════════════════════════════════════════════════════════════

#define THERMISTOR_NOMINAL  10000.0f    // 10kΩ @ 25°C
#define SERIES_RESISTOR     10000.0f    // Voltage divider resistor
#define BCOEFFICIENT        3950.0f     // Beta coefficient (NTC 10k)
#define TEMP_NOMINAL        298.15f     // 25°C in Kelvin
#define THERMISTOR_MIN_ADC  10          // Min valid ADC reading
#define THERMISTOR_MAX_ADC  1020        // Max valid ADC reading
#define TEMP_MIN_VALID      -40.0f      // Min valid temperature °C
#define TEMP_MAX_VALID      125.0f      // Max valid temperature °C

// ════════════════════════════════════════════════════════════
//  7-SEGMENT DISPLAY (common cathode, segments A-F, G=LOW)
// ════════════════════════════════════════════════════════════

// Bit order: A B C D E F (MSB first), segment G tied LOW
static const uint8_t SEG_DIGITS[10] PROGMEM = {
  0b111111, // 0: A+B+C+D+E+F
  0b000110, // 1: B+C
  0b110011, // 2: A+B+D+E
  0b100111, // 3: A+B+C+D
  0b001110, // 4: B+C+F
  0b101101, // 5: A+C+D+F
  0b111101, // 6: A+C+D+E+F
  0b000111, // 7: A+B+C
  0b111111, // 8: A+B+C+D+E+F
  0b101111, // 9: A+B+C+D+F
};

static const uint8_t SEG_PINS[6] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// ════════════════════════════════════════════════════════════
//  ISR STATE (volatile — modified in interrupt context)
// ════════════════════════════════════════════════════════════

volatile bool    tiltTriggered   = false;
volatile uint32_t tiltMs         = 0;
volatile uint16_t tiltCount      = 0;     // Cumulative tilt events
volatile uint32_t lastTiltEventMs = 0;    // FIX: must be volatile

// ════════════════════════════════════════════════════════════
//  SENSOR VALUES
// ════════════════════════════════════════════════════════════

float waterLevel     = 0.0f;    // 0–100%
float thermistorTemp = 25.0f;   // °C
float potCalibration = 1.0f;    // 0.5×–2.0×
float photoPct       = 0.0f;    // 0–100%
float slopePct       = 0.0f;    // 0–100% (spike+decay)

// ════════════════════════════════════════════════════════════
//  EMA SMOOTHING
// ════════════════════════════════════════════════════════════

float ema_water = 0.0f;
#define EMA_ALPHA   0.3f
#define EMA_INV_ALPHA (1.0f - EMA_ALPHA)

// ════════════════════════════════════════════════════════════
//  TIMING CONSTANTS & STATE
// ════════════════════════════════════════════════════════════

#define TX_INTERVAL_MS      2000UL    // Transmit to ESP32
#define SAMPLE_INTERVAL_MS   150UL    // Sensor sampling
#define TILT_TIMEOUT_MS    15000UL    // Auto-clear tilt after 15s
#define TILT_DECAY_MS      30000UL    // Slope decay timeout
#define TILT_SPIKE_PCT      30.0f     // Slope spike on tilt
#define TILT_DECAY_PCT       2.0f     // Slope decay per tick
#define SLOPE_MAX          100.0f

static uint32_t lastTxMs     = 0;
static uint32_t lastBuzMs    = 0;
static uint32_t lastSampleMs = 0;

// ════════════════════════════════════════════════════════════
//  ALERT STATE
// ════════════════════════════════════════════════════════════

static int  cmdAlertLevel = 0;      // From ESP32
static bool buzzerWasOn   = false;

// ════════════════════════════════════════════════════════════
//  SERIAL RECEIVE BUFFER
// ════════════════════════════════════════════════════════════

#define SERIAL_BUF_SIZE 32
static char    serialBuffer[SERIAL_BUF_SIZE];
static uint8_t serialIdx = 0;

// ════════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ════════════════════════════════════════════════════════════

void    readSensors();
void    handleIncomingSerial();
void    handleAlerts(bool tiltNow, uint32_t now);
void    transmitToESP32(bool tiltNow);
void    displayDigit(int digit);
void    clearTiltState();

// ════════════════════════════════════════════════════════════
//  INTERRUPT SERVICE ROUTINE (ISR)
// ════════════════════════════════════════════════════════════

void LANDSLIDE_ISR() {
  tiltTriggered   = true;
  tiltMs          = millis();
  tiltCount++;
  lastTiltEventMs = tiltMs;
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════

void setup() {
  // AVR Watchdog: reset if loop hangs > 4 seconds
  wdt_enable(WDTO_4S);

  // Serial communication to ESP32
  Serial.begin(9600);

  // Tilt switch with internal pull-up (INT0)
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH),
                  LANDSLIDE_ISR, FALLING);

  // Buzzer outputs
  pinMode(PIN_BUZZER_ACTIVE,  OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  noTone(PIN_BUZZER_PASSIVE);

  // 7-segment display pins
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Boot sequence: 7-segment sweep 0→9→0
  for (int d = 0; d <= 9; d++) { displayDigit(d); delay(40); }
  for (int d = 9; d >= 0; d--) { displayDigit(d); delay(40); }
  displayDigit(0);

  // Boot chirp (passive buzzer)
  tone(PIN_BUZZER_PASSIVE, 880,  80); delay(100);
  tone(PIN_BUZZER_PASSIVE, 1100, 80); delay(100);
  tone(PIN_BUZZER_PASSIVE, 1400, 120); delay(150);
  noTone(PIN_BUZZER_PASSIVE);

  Serial.println(F("GEO-SENSE Slave v4.1 Ready"));
  wdt_reset();
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════

void loop() {
  wdt_reset();  // Kick watchdog every cycle
  const uint32_t now = millis();

  // ── TILT TIMEOUT (auto-clear after 15s) ───────────────────
  bool tiltNow;
  uint32_t tiltAge;
  {
    noInterrupts();
    tiltNow = tiltTriggered;
    tiltAge = now - tiltMs;
    interrupts();
  }

  if (tiltNow && tiltAge > TILT_TIMEOUT_MS) {
    clearTiltState();
  }

  // ── SLOPE PERCENTAGE DECAY ────────────────────────────────
  // Decay slopePct toward 0 if no tilt events recently
  if (now - lastTiltEventMs > TILT_DECAY_MS) {
    slopePct = max(0.0f, slopePct - TILT_DECAY_PCT);
  } else if (tiltNow) {
    slopePct = min(SLOPE_MAX, slopePct + TILT_SPIKE_PCT);
  }

  // ── SENSOR SAMPLING (time-gated) ──────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    readSensors();
  }

  // ── SERIAL FROM ESP32 ─────────────────────────────────────
  handleIncomingSerial();

  // ── ALERT HANDLING ────────────────────────────────────────
  handleAlerts(tiltNow, now);

  // ── 7-SEGMENT DISPLAY ─────────────────────────────────────
  displayDigit(tiltNow ? 9 : constrain((int)(waterLevel / 11.1f), 0, 9));

  // ── TRANSMIT TO ESP32 (time-gated) ────────────────────────
  if (now - lastTxMs >= TX_INTERVAL_MS) {
    lastTxMs = now;
    transmitToESP32(tiltNow);
  }
}

// ════════════════════════════════════════════════════════════
//  CLEAR TILT STATE (atomic)
// ════════════════════════════════════════════════════════════

void clearTiltState() {
  noInterrupts();
  tiltTriggered = false;
  interrupts();
  noTone(PIN_BUZZER_PASSIVE);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  buzzerWasOn = false;
}

// ════════════════════════════════════════════════════════════
//  READ SENSORS
// ════════════════════════════════════════════════════════════

void readSensors() {
  // ── WATER LEVEL (0–700 ADC → 0–100%) ─────────────────────
  int rawWater = analogRead(PIN_WATER_LEVEL);
  float wPct   = constrain((float)map(rawWater, 0, 700, 0, 100), 0.0f, 100.0f);
  ema_water    = (EMA_ALPHA * wPct) + (EMA_INV_ALPHA * ema_water);
  waterLevel   = ema_water;

  // ── THERMISTOR (Steinhart-Hart B-parameter) ───────────────
  int rawTherm = analogRead(PIN_THERMISTOR);
  if (rawTherm > THERMISTOR_MIN_ADC && rawTherm < THERMISTOR_MAX_ADC) {
    // Voltage divider: R_therm = R_series × (1023/raw - 1)
    float resistance = SERIES_RESISTOR * ((1023.0f / (float)rawTherm) - 1.0f);
    // B-parameter: 1/T = 1/T0 + (1/B) × ln(R/R0)
    float lnR = logf(resistance / THERMISTOR_NOMINAL);
    float invT = (1.0f / TEMP_NOMINAL) + (lnR / BCOEFFICIENT);
    float tempK = 1.0f / invT;
    float tempC = tempK - 273.15f;
    if (tempC > TEMP_MIN_VALID && tempC < TEMP_MAX_VALID) {
      thermistorTemp = tempC;
    }
  }

  // ── POTENTIOMETER (0.5× to 2.0× calibration) ──────────────
  int rawPot     = analogRead(PIN_POTENTIOMETER);
  potCalibration = (float)map(rawPot, 0, 1023, 50, 200) / 100.0f;

  // ── PHOTORESISTOR (0–100% light intensity) ────────────────
  int rawPhoto = analogRead(PIN_PHOTORESISTOR);
  photoPct     = constrain((float)map(rawPhoto, 0, 1023, 0, 100), 0.0f, 100.0f);
}

// ════════════════════════════════════════════════════════════
//  HANDLE INCOMING SERIAL (from ESP32)
// ════════════════════════════════════════════════════════════

void handleIncomingSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer[serialIdx] = '\0';
      if (strncmp(serialBuffer, "CMD:", 4) == 0) {
        cmdAlertLevel = atoi(serialBuffer + 4);
      }
      serialIdx = 0;
    } else if (serialIdx < SERIAL_BUF_SIZE - 1) {
      serialBuffer[serialIdx++] = c;
    } else {
      serialIdx = 0;  // Overflow — discard and resync
    }
  }
}

// ════════════════════════════════════════════════════════════
//  HANDLE ALERTS (non-blocking)
// ════════════════════════════════════════════════════════════

void handleAlerts(bool tiltNow, uint32_t now) {
  // ── TILT OVERRIDE (continuous 2.5 kHz alarm) ──────────────
  if (tiltNow) {
    if (!buzzerWasOn) {
      tone(PIN_BUZZER_PASSIVE, 2500);
      digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
      buzzerWasOn = true;
    }
    return;
  }

  // ── CLEAR BUZZER when tilt clears and no critical alert ───
  if (buzzerWasOn && cmdAlertLevel < 2) {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    buzzerWasOn = false;
  }

  // ── CRITICAL ALERT (alternating 1800/2000 Hz) ─────────────
  if (cmdAlertLevel >= 2) {
    static bool beepPhase = false;
    if (now - lastBuzMs > 450) {
      lastBuzMs = now;
      beepPhase = !beepPhase;
      // tone() is non-blocking — timer hardware handles duration
      tone(PIN_BUZZER_PASSIVE, beepPhase ? 1800 : 2000, 180);
      buzzerWasOn = true;
    }
  }
  // ── WARNING ALERT (single 880 Hz chirp every 3s) ──────────
  else if (cmdAlertLevel == 1) {
    if (now - lastBuzMs > 3000) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 880, 120);
      buzzerWasOn = false;
    }
  }
  // ── ALL CLEAR ─────────────────────────────────────────────
  else if (buzzerWasOn) {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    buzzerWasOn = false;
  }
}

// ════════════════════════════════════════════════════════════
//  TRANSMIT TO ESP32
//  Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x
// ════════════════════════════════════════════════════════════

void transmitToESP32(bool tiltNow) {
  // Integer-based formatting (avoids sprintf float on AVR)
  char buf[64];

  int w_i = (int)waterLevel;
  int w_f = (int)fabsf((waterLevel     - w_i) * 10.0f) % 10;
  int t_i = (int)thermistorTemp;
  int t_f = (int)fabsf((thermistorTemp - t_i) * 10.0f) % 10;
  int s_i = (int)slopePct;
  int s_f = (int)fabsf((slopePct       - s_i) * 10.0f) % 10;
  int p_i = (int)potCalibration;
  int p_f = (int)fabsf((potCalibration - p_i) * 100.0f) % 100;
  int l_i = (int)photoPct;
  int l_f = (int)fabsf((photoPct       - l_i) * 10.0f) % 10;

  snprintf(buf, sizeof(buf),
    "W:%d.%d,T:%d.%d,S:%d.%d,P:%d.%02d,I:%d,L:%d.%d",
    w_i, w_f, t_i, t_f, s_i, s_f, p_i, p_f,
    tiltNow ? 1 : 0, l_i, l_f);

  Serial.println(buf);
}

// ════════════════════════════════════════════════════════════
//  7-SEGMENT DISPLAY
// ════════════════════════════════════════════════════════════

void displayDigit(int digit) {
  if (digit < 0 || digit > 9) digit = 0;
  uint8_t pattern = pgm_read_byte(&SEG_DIGITS[digit]);
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> (5 - i)) & 0x01);
  }
}
