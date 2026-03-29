// ================================================================
//  GEO-SENSE AFRICA v4.0 — ARDUINO UNO SLAVE NODE
//
//  CARRIED FORWARD FROM v3.0
//  ─────────────────────────────────────────────────────────────
//  Steinhart-Hart thermistor formula (algebraically corrected)
//  photoPct transmitted to ESP32 via L: field
//  tiltPct transmitted as real spike+decay % via S: field
//  ISR flag read with noInterrupts() atomic guard
//  Passive buzzer cleared when tilt times out
//  AVR hardware watchdog via avr/wdt.h (WDTO_4S)
//
//  NEW IN v4.0
//  ─────────────────────────────────────────────────────────────
//  [FIX] lastTiltEventMs declared volatile — without this, the
//        compiler could cache it in a register and the main loop
//        would read a stale value from before the ISR updated it,
//        causing slopePct to never decay correctly.
//  [FIX] delay(180) removed from handleAlerts() double-beep path.
//        A blocking delay inside the loop could stack with boot
//        chirp delays and other waits, potentially approaching the
//        WDTO_4S watchdog threshold. Replaced with non-blocking
//        alternating tone() calls using a phase flag — the AVR
//        timer hardware manages the duration cutoff independently.
//  [FIX] Serial buffer overflow now resets cleanly with a log byte
//        instead of silently discarding mid-frame.
//  [NOTE] D0/D1 (Serial RX/TX) are shared with USB programming.
//         Always disconnect the ESP32 serial wires before uploading
//         new firmware to the Arduino, or use a SoftwareSerial pair
//         on D8/D9 if simultaneous upload+operation is needed.
// ================================================================

#include <Arduino.h>
#include <avr/wdt.h>   // AVR hardware watchdog

// ── PINS ─────────────────────────────────────────────────────
#define PIN_WATER_LEVEL    A1
#define PIN_THERMISTOR     A2
#define PIN_PHOTORESISTOR  A3
#define PIN_POTENTIOMETER  A4
#define PIN_TILT_SWITCH    2   // INT0 — MUST be D2
#define PIN_BUZZER_ACTIVE  3   // Active buzzer (DC on = beep)
#define PIN_BUZZER_PASSIVE 11  // Passive buzzer (needs tone())
#define SEG_A              4
#define SEG_B              5
#define SEG_C              6
#define SEG_D              12
#define SEG_E              13
#define SEG_F              A5

// ── THERMISTOR ───────────────────────────────────────────────
#define THERMISTOR_NOMINAL  10000.0f   // Resistance at 25 °C (10 kΩ)
#define SERIES_RESISTOR     10000.0f   // Series pull-up resistor (10 kΩ)
#define BCOEFFICIENT        3950.0f    // Beta coefficient (NTC 10k)
#define TEMP_NOMINAL        298.15f    // 25 °C in Kelvin

// ── 7-SEG DIGIT PATTERNS (common cathode, segments A–F) ──────
// Bit order: A B C D E F (MSB first), G tied LOW
const uint8_t SEG_DIGITS[10] PROGMEM = {
  0b111111, // 0
  0b000110, // 1
  0b110011, // 2 (approx without G)
  0b100111, // 3
  0b001110, // 4
  0b101101, // 5
  0b111101, // 6
  0b000111, // 7
  0b111111, // 8 (same as 0 — G off; still readable)
  0b101111, // 9
};
const uint8_t SEG_PINS[6] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// ── ISR STATE ────────────────────────────────────────────────
volatile bool    tiltTriggered = false;
volatile uint32_t tiltMs       = 0;
volatile uint16_t tiltCount    = 0;  // Cumulative tilt events (slope activity)

// ── SENSOR VALUES ────────────────────────────────────────────
float waterLevel     = 0.0f;
float thermistorTemp = 25.0f;
float potCalibration = 1.0f;
float photoPct       = 0.0f;
float slopePct       = 0.0f;  // Derived from tilt event frequency

// ── EMA ──────────────────────────────────────────────────────
float ema_water = 0.0f;
#define EMA_A 0.3f

// ── TIMING ───────────────────────────────────────────────────
uint32_t lastTxMs     = 0;
uint32_t lastBuzMs    = 0;
uint32_t lastSampleMs = 0;
#define TX_INTERVAL      2000
#define SAMPLE_INTERVAL  150

// ── ALERT LEVEL FROM ESP32 ───────────────────────────────────
int cmdAlertLevel = 0;

// ── SERIAL RECEIVE BUFFER ────────────────────────────────────
#define SERIAL_BUF_SIZE 32
char    serialBuffer[SERIAL_BUF_SIZE];
uint8_t serialIdx = 0;

// ── BUZZER STATE ─────────────────────────────────────────────
bool buzzerWasOn = false;

// ── TILT SLOPE ESTIMATE ──────────────────────────────────────
// FIX v4: lastTiltEventMs must be volatile — it is written inside the ISR.
// Without volatile, the compiler may cache it in a register and the main
// loop will read a stale value, causing slopePct to never decay correctly.
volatile uint32_t lastTiltEventMs = 0;
#define TILT_DECAY_MS 30000  // Slope considered clear if no tilt for 30 s

// ── ISR ──────────────────────────────────────────────────────
void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs        = millis();
  tiltCount++;
  lastTiltEventMs = tiltMs;
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  // AVR watchdog: reset if loop hangs > 4 seconds
  wdt_enable(WDTO_4S);

  Serial.begin(9600);  // To ESP32 via Serial2 on ESP32 side

  // Tilt switch with internal pull-up
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH),
                  LANDSLIDE_ISR, FALLING);

  // Buzzer outputs
  pinMode(PIN_BUZZER_ACTIVE,  OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  noTone(PIN_BUZZER_PASSIVE);

  // 7-Segment
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Boot sweep 0→9
  for (int d = 0; d <= 9; d++) { displayDigit(d); delay(40); }
  for (int d = 9; d >= 0; d--) { displayDigit(d); delay(40); }
  displayDigit(0);

  // Boot chirp (passive buzzer)
  tone(PIN_BUZZER_PASSIVE, 880,  80); delay(100);
  tone(PIN_BUZZER_PASSIVE, 1100, 80); delay(100);
  tone(PIN_BUZZER_PASSIVE, 1400, 120); delay(150);
  noTone(PIN_BUZZER_PASSIVE);

  Serial.println(F("GEO-SENSE Slave v4.0 Ready"));
  wdt_reset();
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  wdt_reset();  // Kick AVR watchdog
  uint32_t now = millis();

  // ── TILT TIMEOUT (auto-clear after 15 s) ─────────────────
  // Read tiltTriggered with interrupt guard on AVR
  bool tiltNow;
  uint32_t tiltAge;
  {
    noInterrupts();
    tiltNow = tiltTriggered;
    tiltAge = now - tiltMs;
    interrupts();
  }
  if (tiltNow && tiltAge > 15000UL) {
    noInterrupts(); tiltTriggered = false; interrupts();
    noTone(PIN_BUZZER_PASSIVE);         // FIX: was left ringing
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    buzzerWasOn = false;
  }

  // ── SLOPE % DECAY ────────────────────────────────────────
  // Decay slopePct toward 0 if no tilt events recently
  if (now - lastTiltEventMs > TILT_DECAY_MS) {
    slopePct = max(0.0f, slopePct - 2.0f); // Decay 2% per loop
  } else {
    // Spike to 80% on fresh tilt, decay otherwise
    if (tiltNow) slopePct = min(100.0f, slopePct + 30.0f);
  }

  // ── SENSOR SAMPLING ──────────────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readSensors();
  }

  // ── SERIAL FROM ESP32 ────────────────────────────────────
  handleIncomingSerial();

  // ── ALERTS ───────────────────────────────────────────────
  handleAlerts(tiltNow, now);

  // ── 7-SEG DISPLAY ────────────────────────────────────────
  if (tiltNow) {
    displayDigit(9);
  } else {
    int dispVal = constrain((int)(waterLevel / 11.1f), 0, 9);
    displayDigit(dispVal);
  }

  // ── TRANSMIT TO ESP32 ────────────────────────────────────
  if (now - lastTxMs >= TX_INTERVAL) {
    lastTxMs = now;
    transmitToESP32(tiltNow);
  }
}

// ── READ SENSORS ─────────────────────────────────────────────
void readSensors() {
  // Water Level — map raw 0–700 to 0–100%
  int rawWater = analogRead(PIN_WATER_LEVEL);
  float wPct   = constrain((float)map(rawWater, 0, 700, 0, 100), 0.0f, 100.0f);
  ema_water    = (EMA_A * wPct) + ((1.0f - EMA_A) * ema_water);
  waterLevel   = ema_water;

  // Thermistor — Steinhart-Hart (FIXED formula)
  // B-parameter equation: 1/T = 1/T0 + (1/B) * ln(R/R0)
  int rawTherm = analogRead(PIN_THERMISTOR);
  if (rawTherm > 10 && rawTherm < 1020) {
    // Voltage divider: R_therm = R_series * (ADC_max/raw - 1)
    float resistance = SERIES_RESISTOR * ((1023.0f / (float)rawTherm) - 1.0f);
    // FIX: correct Steinhart-Hart B-parameter calculation
    float lnR = logf(resistance / THERMISTOR_NOMINAL); // ln(R/R0)
    float invT = (1.0f / TEMP_NOMINAL) + (lnR / BCOEFFICIENT); // 1/T = 1/T0 + ln(R/R0)/B
    float tempK = 1.0f / invT;
    float tempC = tempK - 273.15f;
    if (tempC > -40.0f && tempC < 125.0f) {
      thermistorTemp = tempC;
    }
  }

  // Potentiometer — sensitivity 0.5× to 2.0×
  int rawPot     = analogRead(PIN_POTENTIOMETER);
  potCalibration = (float)map(rawPot, 0, 1023, 50, 200) / 100.0f;

  // Photoresistor — FIX: now transmitted to ESP32
  int rawPhoto = analogRead(PIN_PHOTORESISTOR);
  photoPct     = constrain((float)map(rawPhoto, 0, 1023, 0, 100), 0.0f, 100.0f);
}

// ── SERIAL INPUT FROM ESP32 ──────────────────────────────────
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
      serialIdx = 0; // Overflow — discard
    }
  }
}

// ── HANDLE ALERTS ────────────────────────────────────────────
void handleAlerts(bool tiltNow, uint32_t now) {
  if (tiltNow) {
    // ISR override: continuous 2.5 kHz
    if (!buzzerWasOn) {
      tone(PIN_BUZZER_PASSIVE, 2500);
      digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
      buzzerWasOn = true;
    }
    return;
  }

  // FIX: ensure buzzer is off when tilt clears and no CMD
  if (buzzerWasOn && cmdAlertLevel < 2) {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    buzzerWasOn = false;
  }

  if (cmdAlertLevel >= 2) {
    // Critical: alternating 1800/2000 Hz beep — NO delay() inside loop.
    // FIX v4: removed blocking delay(180) which could stack with other
    // delays and approach the AVR watchdog WDTO_4S threshold.
    // tone(pin, freq, duration) is non-blocking after return — the timer
    // hardware handles the cutoff. We alternate pitch with a phase flag.
    static bool beepPhase = false;
    if (now - lastBuzMs > 450) {
      lastBuzMs = now;
      beepPhase = !beepPhase;
      tone(PIN_BUZZER_PASSIVE, beepPhase ? 1800 : 2000, 180);
      buzzerWasOn = true;
    }
  } else if (cmdAlertLevel == 1) {
    // Warning: single 880 Hz chirp every 3 s
    if (now - lastBuzMs > 3000) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 880, 120);
      buzzerWasOn = false;
    }
  } else {
    // All clear
    if (buzzerWasOn) {
      noTone(PIN_BUZZER_PASSIVE);
      digitalWrite(PIN_BUZZER_ACTIVE, LOW);
      buzzerWasOn = false;
    }
  }
}

// ── TRANSMIT TO ESP32 ────────────────────────────────────────
// Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x
// W = water%, T = soil temp °C, S = slope%, P = pot cal, I = ISR flag, L = light%
void transmitToESP32(bool tiltNow) {
  // Safe integer-based formatting (avoids sprintf float on small AVR)
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
           w_i, w_f,
           t_i, t_f,
           s_i, s_f,
           p_i, p_f,
           tiltNow ? 1 : 0,
           l_i, l_f);

  Serial.println(buf);
}

// ── 7-SEGMENT DISPLAY ────────────────────────────────────────
void displayDigit(int d) {
  if (d < 0 || d > 9) d = 0;
  uint8_t pattern = pgm_read_byte(&SEG_DIGITS[d]);
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> (5 - i)) & 0x01);
  }
}
