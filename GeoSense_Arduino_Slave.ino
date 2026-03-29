// ================================================================
//  GEO-SENSE AFRICA v2.0 — ARDUINO UNO SLAVE NODE (OPTIMIZED)
//  Role: Raw Sensor Reading, Tilt Interrupt, Serial to ESP32
// ================================================================

#include <Arduino.h>

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

// ── 7-SEG DIGIT PATTERNS (A-F) ──────────────────────────────
const uint8_t SEG_DIGITS[10] PROGMEM = {
  0b111111, // 0
  0b000110, // 1
  0b110011, // 2
  0b100111, // 3
  0b001110, // 4
  0b101101, // 5
  0b111101, // 6
  0b000111, // 7
  0b111111, // 8
  0b101111, // 9
};
const int SEG_PINS[6] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// ── GLOBALS ─────────────────────────────────────────────────
volatile bool tiltTriggered   = false;
volatile uint32_t tiltMs      = 0;

float waterLevel    = 0;
float thermistorTemp= 25.0;
float potCalibration= 1.0;
float photoPct      = 0;

float ema_water = 0;
#define EMA_A 0.3f

int   cmdAlertLevel   = 0;
uint32_t lastTxMs     = 0;
uint32_t lastBuzMs    = 0;
uint32_t lastSampleMs = 0;
#define TX_INTERVAL      2000
#define SAMPLE_INTERVAL  100

// Serial buffer for incoming data
#define SERIAL_BUF_SIZE 32
char serialBuffer[SERIAL_BUF_SIZE];
uint8_t serialIdx = 0;

// ── ISR ──────────────────────────────────────────────────────
void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs = millis();
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
  
  Serial.println("Arduino Slave Ready");
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── TILT TIMEOUT (Auto-reset after 15 seconds) ───────────
  if (tiltTriggered && (now - tiltMs > 15000)) {
    tiltTriggered = false;
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
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
  if (!tiltTriggered) {
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

void readSensors() {
  // Water Level
  int rawWater = analogRead(PIN_WATER_LEVEL);
  float wPct = constrain(map(rawWater, 0, 700, 0, 100), 0, 100);
  ema_water = (EMA_A * wPct) + ((1.0f - EMA_A) * ema_water);
  waterLevel = ema_water;

  // Thermistor
  int rawTherm = analogRead(PIN_THERMISTOR);
  if (rawTherm > 10) {
    float resist = SERIES_RESISTOR * ((1023.0f / rawTherm) - 1.0f);
    float sh = log(resist / THERMISTOR_NOMINAL) / BCOEFFICIENT;
    sh += 1.0f / (TEMP_NOMINAL + 273.15f);
    thermistorTemp = (1.0f / sh) - 273.15f;
  }

  // Potentiometer
  potCalibration = map(analogRead(PIN_POTENTIOMETER), 0, 1023, 50, 200) / 100.0f;
  
  // Photoresistor
  photoPct = map(analogRead(PIN_PHOTORESISTOR), 0, 1023, 0, 100);
}

void handleIncomingSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      // End of command - parse it
      serialBuffer[serialIdx] = '\0';
      
      // Parse CMD:XX format
      if (strncmp(serialBuffer, "CMD:", 4) == 0) {
        cmdAlertLevel = atoi(serialBuffer + 4);
      }
      
      // Reset buffer
      serialIdx = 0;
    } else {
      // Add character to buffer (with overflow protection)
      if (serialIdx < SERIAL_BUF_SIZE - 1) {
        serialBuffer[serialIdx++] = c;
      }
    }
  }
}

void handleAlerts(uint32_t now) {
  if (tiltTriggered) {
    tone(PIN_BUZZER_PASSIVE, 2500);
    digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
    return;
  }

  if (cmdAlertLevel == 2) {
    if (now - lastBuzMs > 400) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 1800, 200);
    }
  } else if (cmdAlertLevel == 1) {
    if (now - lastBuzMs > 3000) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 880, 150);
    }
  } else {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  }
}

void transmitToESP32() {
  // Format: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x
  // Using snprintf for safe formatting
  char buffer[50];
  
  int w_int = (int)waterLevel;
  int w_frac = (int)abs((waterLevel - w_int) * 10) % 10;
  int t_int = (int)thermistorTemp;
  int t_frac = (int)abs((thermistorTemp - t_int) * 10) % 10;
  int p_int = (int)potCalibration;
  int p_frac = (int)abs((potCalibration - p_int) * 100) % 100;

  snprintf(buffer, sizeof(buffer), "W:%d.%d,T:%d.%d,S:0.0,P:%d.%02d,I:%d",
           w_int, w_frac, t_int, t_frac, p_int, p_frac, tiltTriggered ? 1 : 0);
  
  Serial.println(buffer);
}

void displayDigit(int d) {
  if (d < 0 || d > 9) d = 0;
  uint8_t pattern = pgm_read_byte(&SEG_DIGITS[d]);
  for (int i = 0; i < 6; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> (5 - i)) & 0x01);
  }
}
