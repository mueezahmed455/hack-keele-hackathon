// ================================================================
//  GEO-SENSE AFRICA v2.2 — ARDUINO UNO SLAVE NODE (PRODUCTION)
//  Role: Raw Sensor Reading, Tilt Interrupt, Serial to ESP32
//  Version: 2.2.0 - Security & Stability Hardened
// ================================================================

#include <Arduino.h>
#include <util/atomic.h>
#include <avr/wdt.h>

// ── VERSION CONFIGURATION ──────────────────────────────────────
#define FIRMWARE_VERSION "2.2.0"

// ── PIN DEFINITIONS ─────────────────────────────────────────
#define PIN_WATER_LEVEL    A1
#define PIN_THERMISTOR     A2
#define PIN_PHOTORESISTOR  A3
#define PIN_POTENTIOMETER  A4
#define PIN_TILT_SWITCH    2
#define PIN_BUZZER_ACTIVE  3
#define PIN_BUZZER_PASSIVE 11
#define SEG_A              4
#define SEG_B              5
#define SEG_C              6
#define SEG_D              12
#define SEG_E              13
#define SEG_F              A5

// ── THERMISTOR CONSTANTS ─────────────────────────────────────
#define THERMISTOR_NOMINAL  10000
#define SERIES_RESISTOR     10000
#define BCOEFFICIENT        3950
#define TEMP_NOMINAL        25

// ── SENSOR CALIBRATION CONSTANTS ────────────────────────────
#define WATER_LEVEL_MIN_ADC     0
#define WATER_LEVEL_MAX_ADC     700
#define POT_CALIBRATION_MIN     50
#define POT_CALIBRATION_MAX     200
#define THERMISTOR_MIN_READING  10

// ── ALERT TIMING CONSTANTS ──────────────────────────────────
#define ALERT_2_INTERVAL_MS     400
#define ALERT_2_FREQ            1800
#define ALERT_2_DURATION_MS     200
#define ALERT_1_INTERVAL_MS     3000
#define ALERT_1_FREQ            880
#define ALERT_1_DURATION_MS     150
#define TILT_ALARM_FREQ         2500
#define TILT_TIMEOUT_US         15000000UL  // 15 seconds in microseconds

// ── 7-SEG DIGIT PATTERNS (A-F) ──────────────────────────────
// Segment mapping: 0bGFEDCBA (bit 0 = A, bit 5 = F)
const uint8_t SEG_DIGITS[10] PROGMEM = {
  0b00111111, // 0: A,B,C,D,E,F
  0b00000110, // 1: B,C
  0b01011011, // 2: A,B,D,E,G
  0b01001111, // 3: A,B,C,D,G
  0b01100110, // 4: B,C,F,G
  0b01101101, // 5: A,C,D,F,G
  0b01111101, // 6: A,C,D,E,F,G
  0b00000111, // 7: A,B,C
  0b01111111, // 8: A,B,C,D,E,F,G
  0b01101111, // 9: A,B,C,D,F,G
};

// Store pin mapping in PROGMEM to save RAM
const uint8_t SEG_PINS[6] PROGMEM = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// ── GLOBALS (volatile for ISR safety) ───────────────────────
volatile bool tiltTriggered   = false;
volatile uint32_t tiltMs      = 0;

float waterLevel    = 0;
float thermistorTemp= 25.0;
float potCalibration= 1.0;
float photoPct      = 0;

float ema_water = 0;
#define EMA_A 0.3f

int cmdAlertLevel = 0;
uint32_t lastTxMs = 0;
uint32_t lastBuzMs = 0;
uint32_t lastSampleMs = 0;

#define TX_INTERVAL       2000
#define SAMPLE_INTERVAL   100

// ── SERIAL BUFFER FOR INCOMING DATA ─────────────────────────
#define SERIAL_BUF_SIZE 32
char serialBuffer[SERIAL_BUF_SIZE];
uint8_t serialIdx = 0;

// ── FUNCTION PROTOTYPES ─────────────────────────────────────
void readSensors();
void handleIncomingSerial();
void handleAlerts(uint32_t now);
void transmitToESP32();
void displayDigit(int d);
void safeTiltReset();

// ── ISR FOR TILT SWITCH ─────────────────────────────────────
// Note: Keep ISRs as short as possible - only set flags
// Using micros() for better timing resolution in ISR
void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs = micros();  // Use micros() for better resolution
}

// ── SAFE TILT RESET (Atomic) ────────────────────────────────
void safeTiltReset() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    tiltTriggered = false;
  }
  noTone(PIN_BUZZER_PASSIVE);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // Tilt Switch with Internal Pull-up
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH), LANDSLIDE_ISR, FALLING);

  // Buzzer Outputs
  pinMode(PIN_BUZZER_ACTIVE, OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  noTone(PIN_BUZZER_PASSIVE);

  // 7-Segment Display Pins
  for (int i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Boot sequence - show digits 0-9
  for (int d = 0; d <= 9; d++) {
    displayDigit(d);
    delay(30);
  }
  displayDigit(0);

  // Initialize EMA with first reading
  readSensors();
  ema_water = waterLevel;

  // Enable Watchdog Timer (2 second timeout)
  // WDT will reset the board if loop() hangs
  wdt_enable(WDTO_2S);

  Serial.println(F("Arduino Slave v" FIRMWARE_VERSION " Ready"));
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  // Reset watchdog timer at start of each loop
  wdt_reset();

  uint32_t now = millis();

  // ── TILT TIMEOUT (Auto-reset after 15 seconds) ───────────
  // Use atomic block for safe volatile access
  bool localTiltTriggered;
  uint32_t localTiltUs;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    localTiltTriggered = tiltTriggered;
    localTiltUs = tiltMs;
  }

  if (localTiltTriggered && ((uint32_t)(micros() - localTiltUs) > TILT_TIMEOUT_US)) {
    safeTiltReset();
  }

  // ── SENSOR SAMPLING ──────────────────────────────────────
  if (now - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = now;
    readSensors();
  }

  // ── SERIAL COMMANDS FROM ESP32 ───────────────────────────
  handleIncomingSerial();

  // ── ALERTS & BUZZERS ─────────────────────────────────────
  handleAlerts(now);

  // ── 7-SEGMENT DISPLAY ────────────────────────────────────
  // Check tilt state again for display
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    localTiltTriggered = tiltTriggered;
  }
  
  if (!localTiltTriggered) {
    int dispVal = constrain((int)(waterLevel / 11.1f), 0, 9);
    displayDigit(dispVal);
  } else {
    displayDigit(9);  // Show 9 during tilt alarm
  }

  // ── TRANSMIT TO ESP32 ────────────────────────────────────
  if (now - lastTxMs >= TX_INTERVAL) {
    lastTxMs = now;
    transmitToESP32();
  }
}

// ── SENSOR READING FUNCTIONS ────────────────────────────────
void readSensors() {
  // Water Level with validation
  int rawWater = analogRead(PIN_WATER_LEVEL);
  int constrainedWater = constrain(rawWater, WATER_LEVEL_MIN_ADC, WATER_LEVEL_MAX_ADC);
  float wPct = (float)map(constrainedWater, WATER_LEVEL_MIN_ADC, WATER_LEVEL_MAX_ADC, 0, 100);
  ema_water = (EMA_A * wPct) + ((1.0f - EMA_A) * ema_water);
  waterLevel = constrain(ema_water, 0.0f, 100.0f);

  // Thermistor with validation
  int rawTherm = analogRead(PIN_THERMISTOR);
  if (rawTherm > THERMISTOR_MIN_READING && rawTherm < 1023) {
    float resist = SERIES_RESISTOR * ((1023.0f / rawTherm) - 1.0f);
    if (resist > 0.0f) {
      float sh = log(resist / THERMISTOR_NOMINAL) / BCOEFFICIENT;
      sh += 1.0f / (TEMP_NOMINAL + 273.15f);
      float temp = (1.0f / sh) - 273.15f;

      // Validate temperature range (-40 to 125 C)
      if (temp >= -40.0f && temp <= 125.0f) {
        thermistorTemp = temp;
      } else {
        // Invalid temperature - keep last known good value
      }
    }
  }

  // Potentiometer with validation
  int rawPot = analogRead(PIN_POTENTIOMETER);
  int constrainedPot = constrain(rawPot, 0, 1023);
  potCalibration = (float)map(constrainedPot, 0, 1023, POT_CALIBRATION_MIN, POT_CALIBRATION_MAX) / 100.0f;

  // Photoresistor (for ambient light monitoring)
  int rawPhoto = analogRead(PIN_PHOTORESISTOR);
  photoPct = (float)map(constrain(rawPhoto, 0, 1023), 0, 1023, 0, 100);
}

// ── SERIAL HANDLING WITH BUFFER OVERFLOW PROTECTION ─────────
void handleIncomingSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      // End of command - parse it
      serialBuffer[serialIdx] = '\0';

      // Parse CMD:XX format with validation
      if (serialIdx >= 5 && strncmp(serialBuffer, "CMD:", 4) == 0) {
        char* endPtr = nullptr;
        long val = strtol(serialBuffer + 4, &endPtr, 10);
        // Validate alert level range (0-2) and proper parsing
        if (endPtr != serialBuffer + 4 && val >= 0 && val <= 2) {
          cmdAlertLevel = (int)val;
        } else {
          Serial.print("Invalid CMD value: ");
          Serial.println(serialBuffer);
        }
      }

      // Reset buffer
      serialIdx = 0;
    } else {
      // Add character to buffer (with overflow protection)
      if (serialIdx < SERIAL_BUF_SIZE - 1) {
        serialBuffer[serialIdx++] = c;
        serialBuffer[serialIdx] = '\0';  // Maintain null termination
      } else {
        // Buffer overflow - discard and reset
        serialIdx = 0;
        serialBuffer[0] = '\0';
      }
    }
  }
}

// ── ALERT HANDLING ──────────────────────────────────────────
void handleAlerts(uint32_t now) {
  // Check tilt state atomically
  bool localTiltTriggered;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    localTiltTriggered = tiltTriggered;
  }

  // Tilt alarm takes priority
  if (localTiltTriggered) {
    tone(PIN_BUZZER_PASSIVE, TILT_ALARM_FREQ);
    digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
    return;
  }

  // Handle ESP32 command-based alerts
  if (cmdAlertLevel == 2) {
    // Critical alert - beep every 400ms
    if (now - lastBuzMs > ALERT_2_INTERVAL_MS) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, ALERT_2_FREQ, ALERT_2_DURATION_MS);
    }
  } else if (cmdAlertLevel == 1) {
    // Caution alert - beep every 3 seconds
    if (now - lastBuzMs > ALERT_1_INTERVAL_MS) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, ALERT_1_FREQ, ALERT_1_DURATION_MS);
    }
  } else {
    // No alert - silence buzzers
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  }
}

// ── TRANSMIT DATA TO ESP32 ──────────────────────────────────
void transmitToESP32() {
  char buffer[48];
  
  // Helper to split float into int/frac for snprintf (Uno safe)
  auto fmtFloat = [](float val, int& i, int& f, int mult) {
    i = (int)val;
    f = (int)(abs(val - i) * mult);
  };

  int w_i, w_f, t_i, t_f, p_i, p_f;
  fmtFloat(constrain(waterLevel, 0, 100), w_i, w_f, 10);
  fmtFloat(constrain(thermistorTemp, -40, 125), t_i, t_f, 10);
  fmtFloat(constrain(potCalibration, 0.1, 10), p_i, p_f, 100);

  // W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
  snprintf(buffer, sizeof(buffer), "W:%d.%d,T:%d.%d,S:0.0,P:%d.%02d,I:%d", 
           w_i, w_f, t_i, t_f, p_i, p_f, tiltTriggered ? 1 : 0);

  // Calculate XOR Checksum
  uint8_t checksum = 0;
  for (int i = 0; buffer[i] != '\0'; i++) {
    checksum ^= (uint8_t)buffer[i];
  }

  // Final Packet: DATA*CHECKSUM (HEX)
  Serial.print(buffer);
  Serial.print("*");
  if (checksum < 0x10) Serial.print('0'); // Padding
  Serial.println(checksum, HEX);
}

// ── 7-SEGMENT DISPLAY ───────────────────────────────────────
void displayDigit(int d) {
  if (d < 0 || d > 9) d = 0;

  // Read pattern from PROGMEM
  uint8_t pattern = pgm_read_byte(&SEG_DIGITS[d]);

  // Update all segment pins
  for (int i = 0; i < 6; i++) {
    uint8_t pin = pgm_read_byte(&SEG_PINS[i]);
    digitalWrite(pin, (pattern >> (5 - i)) & 0x01);
  }
}
