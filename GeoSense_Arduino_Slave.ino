// ================================================================
//  GEO-SENSE AFRICA v2.0 — ARDUINO UNO SLAVE NODE
//  Role: Raw Sensor Reading, Tilt Interrupt, Serial to ESP32
//
//  Sensors handled by Arduino:
//  - Water Level Sensor     → A1
//  - Thermistor (soil temp) → A2
//  - Potentiometer (cal.)   → A4
//  - Tilt Switch            → D2 (hardware interrupt INT0)
//  - Active Buzzer          → D3 (local emergency alert)
//  - 7-Segment Display      → D4–D6, D12, D13, A5
//  - Photoresistor          → A3 (solar/light level)
//  - Passive Buzzer         → D11 (melody/tones)
//
//  Serial Protocol to ESP32:
//  TX: "W:38.2,T:22.5,S:12.3,P:1.00,I:0\n"
//       W=water%, T=soilTemp, S=tilt%, P=potCal, I=interruptFlag
//  RX: "CMD:1\n"  (alert level override from ESP32)
// ================================================================

#include <DHT.h>  // Backup DHT if ESP32 DHT fails

// ── PIN DEFINITIONS ─────────────────────────────────────────
#define PIN_WATER_LEVEL    A1
#define PIN_THERMISTOR     A2
#define PIN_PHOTORESISTOR  A3
#define PIN_POTENTIOMETER  A4
#define PIN_TILT_SWITCH    2    // INT0 — MUST be D2
#define PIN_BUZZER_ACTIVE  3    // PWM
#define PIN_BUZZER_PASSIVE 11   // PWM (passive needs tone())
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

// ── 7-SEG DIGIT PATTERNS (A-F, no G) ────────────────────────
const byte SEG_DIGITS[10] = {
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
volatile unsigned long tiltMs = 0;

float waterLevel    = 0;
float thermistorTemp= 25.0;
float potCalibration= 1.0;
float photoPct      = 0;

float ema_water = 0, ema_tilt = 0;
#define EMA_A 0.3f

int   cmdAlertLevel   = 0;  // From ESP32
unsigned long lastTxMs = 0;
unsigned long lastBuzMs = 0;
#define TX_INTERVAL  2000

int   lastRisk = 0;

// Buzzer melody notes (alert tones)
int MELODY_WARN[]    = {880, 0, 880, 0};
int MELODY_CRITICAL[]= {2500, 0, 2500, 0, 2500};
int MELODY_BOOT[]    = {523, 659, 784, 1047};

// ── ISR ──────────────────────────────────────────────────────
void LANDSLIDE_ISR() {
  tiltTriggered = true;
  tiltMs = millis();
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600); // Communicates with ESP32

  // Tilt interrupt
  pinMode(PIN_TILT_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TILT_SWITCH),
                  LANDSLIDE_ISR, FALLING);

  // Outputs
  pinMode(PIN_BUZZER_ACTIVE,  OUTPUT);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  for (int i = 0; i < 6; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], LOW);
  }

  // Boot sequence
  playMelody(MELODY_BOOT, 4, 120);
  for (int d = 0; d <= 9; d++) { displayDigit(d); delay(70); }
  for (int d = 9; d >= 0; d--) { displayDigit(d); delay(70); }
  displayDigit(0);
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {

  // ── TILT INTERRUPT — highest priority ───────────────────
  if (tiltTriggered) {
    handleLandslideAlert();
    // Auto-clear after 15s if no new trigger
    if (millis() - tiltMs > 15000) {
      tiltTriggered = false;
      noTone(PIN_BUZZER_PASSIVE);
      digitalWrite(PIN_BUZZER_ACTIVE, LOW);
    }
  }

  // ── READ SENSORS ────────────────────────────────────────
  // Water Level
  int rawWater = analogRead(PIN_WATER_LEVEL);
  float wPct = constrain(map(rawWater, 0, 700, 0, 100), 0, 100);
  ema_water = EMA_A * wPct + (1.0f - EMA_A) * ema_water;
  waterLevel = ema_water;

  // Thermistor (Soil Temperature)
  int rawTherm = analogRead(PIN_THERMISTOR);
  if (rawTherm > 10) {
    float resist = SERIES_RESISTOR * ((1023.0f / rawTherm) - 1.0f);
    float sh = resist / THERMISTOR_NOMINAL;
    sh = log(sh);
    sh /= BCOEFFICIENT;
    sh += 1.0f / (TEMP_NOMINAL + 273.15f);
    thermistorTemp = (1.0f / sh) - 273.15f;
    thermistorTemp = constrain(thermistorTemp, -10, 80);
  }

  // Potentiometer calibration
  int rawPot = analogRead(PIN_POTENTIOMETER);
  potCalibration = map(rawPot, 0, 1023, 50, 200) / 100.0f;

  // Photoresistor (solar / light)
  int rawPhoto = analogRead(PIN_PHOTORESISTOR);
  photoPct = map(rawPhoto, 0, 1023, 0, 100);

  // ── RECEIVE COMMAND FROM ESP32 ───────────────────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd.startsWith("CMD:")) {
      cmdAlertLevel = cmd.substring(4).toInt();
    }
  }

  // ── LOCAL BUZZER BASED ON ESP32 COMMAND ─────────────────
  handleLocalAlert();

  // ── UPDATE 7-SEG DISPLAY ────────────────────────────────
  // Show water level 0-9 when no interrupt
  if (!tiltTriggered) {
    int dispVal = (int)(waterLevel / 11.1f); // 0-9
    dispVal = constrain(dispVal, 0, 9);
    displayDigit(dispVal);
    lastRisk = dispVal;
  }

  // ── TRANSMIT TO ESP32 ────────────────────────────────────
  if (millis() - lastTxMs >= TX_INTERVAL) {
    lastTxMs = millis();
    transmitToESP32();
  }

  delay(50);
}

// ── TRANSMIT DATA TO ESP32 ───────────────────────────────────
void transmitToESP32() {
  Serial.print("W:");    Serial.print(waterLevel, 1);
  Serial.print(",T:");   Serial.print(thermistorTemp, 1);
  Serial.print(",S:");   Serial.print(0.0, 1);    // Tilt % (0 unless ISR)
  Serial.print(",P:");   Serial.print(potCalibration, 2);
  Serial.print(",I:");   Serial.println(tiltTriggered ? 1 : 0);
}

// ── LANDSLIDE ALERT ──────────────────────────────────────────
void handleLandslideAlert() {
  displayDigit(9);
  // Active buzzer at 2500Hz
  tone(PIN_BUZZER_PASSIVE, 2500);
  digitalWrite(PIN_BUZZER_ACTIVE, HIGH);
}

// ── LOCAL ALERT FROM ESP32 COMMAND ──────────────────────────
void handleLocalAlert() {
  if (tiltTriggered) return; // ISR takes over

  unsigned long now = millis();

  if (cmdAlertLevel == 2) {
    // Critical: fast beep pattern
    if (now - lastBuzMs > 400) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 1800, 200);
    }
  } else if (cmdAlertLevel == 1) {
    // Warning: single chirp every 3 seconds
    if (now - lastBuzMs > 3000) {
      lastBuzMs = now;
      tone(PIN_BUZZER_PASSIVE, 880, 150);
    }
  } else {
    noTone(PIN_BUZZER_PASSIVE);
    digitalWrite(PIN_BUZZER_ACTIVE, LOW);
  }
}

// ── PLAY MELODY ──────────────────────────────────────────────
void playMelody(int* notes, int len, int dur) {
  for (int i = 0; i < len; i++) {
    if (notes[i] > 0) tone(PIN_BUZZER_PASSIVE, notes[i], dur);
    else              noTone(PIN_BUZZER_PASSIVE);
    delay(dur + 20);
  }
  noTone(PIN_BUZZER_PASSIVE);
}

// ── 7-SEGMENT DISPLAY ────────────────────────────────────────
void displayDigit(int d) {
  if (d < 0 || d > 9) d = 0;
  byte pattern = SEG_DIGITS[d];
  for (int i = 0; i < 6; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> (5 - i)) & 0x01);
  }
}
