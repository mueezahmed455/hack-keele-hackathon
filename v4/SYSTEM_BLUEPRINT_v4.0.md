# GEO-SENSE AFRICA v4.0 — SYSTEM BLUEPRINT
## Multi-Hazard Early Warning System (MHEWS) Architecture

This blueprint defines the technical specifications, data models, and visualization logic for the Geo-Sense Africa v4.0 platform.

---

## 1. SYSTEM HIERARCHY

### Layer 1: Perception (Sensors)
*   **Aqueous:** HC-SR04 (Ultrasound), Water Level Sensor (Conductivity), Obstacle IR (Debris).
*   **Geological:** Tilt Switch (Mercury/Ball), Soil Moisture (Capacitive/Resistive).
*   **Atmospheric:** DHT11 (Temp/Humidity), Photoresistor (Solar Intensity), Thermistor (Soil Temp).
*   **Community:** HC-SR501 PIR (Evacuation Detection), TTP223B (Manual ACK).

### Layer 2: Edge Processing (Arduino Uno Slave)
*   **Role:** Raw Signal Normalization, Local Alert Control.
*   **Priority:** Hardware Interrupt (INT0) for Landslide detection.
*   **Safety:** AVR Hardware Watchdog (WDTO_4S) via `avr/wdt.h`.
*   **Outputs:** Local 7-Segment Display (Water Level 0-9), Dual Buzzer Control (Active + Passive).
*   **Upstream:** 9600 Baud Serial Packet Transmission.

### Layer 3: Fusion & Gateway (ESP32 Master)
*   **Role:** Multi-sensor Data Fusion, Risk Index Calculation, WiFi Gateway.
*   **Smoothing:** Exponential Moving Average (EMA) with α=0.25.
*   **Connectivity:** WiFi Access Point (AP), Web Server, I2C Display Driver.

---

## 2. DATA MODELS & COMMUNICATION

### A. Serial Protocol (Slave ──► Master)
*   **Format:** `W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x\n`
*   **Payload:**
    *   `W`: Water Level % (0.0 - 100.0) — EMA smoothed
    *   `T`: Soil Temperature °C — Steinhart-Hart corrected
    *   `S`: Slope Activity % (0.0 - 100.0) — Spike+decay from tilt events
    *   `P`: Potentiometer Sensitivity (0.50 - 2.00) — Calibration multiplier
    *   `I`: Landslide Interrupt Flag (0 or 1) — Atomic ISR read
    *   `L`: Light Intensity % (0.0 - 100.0) — Photoresistor (NEW in v4.0)

### B. Command Protocol (Master ──► Slave)
*   **Format:** `CMD:alert_level\n`
*   **Payload:**
    *   `0`: SAFE — All clear, buzzers off
    *   `1`: CAUTION — Single 880 Hz chirp every 3s
    *   `2`: CRITICAL — Alternating 1800/2000 Hz beep (non-blocking)

### C. Risk Calculation Logic (Fusion Engine)
The system calculates a **Weighted Risk Index (0-9)** based on environmental precursors:

$$Risk = \left[ (S_{soil} \times 0.3) + (W_{water} \times 0.4) + (T_{tilt} \times 0.3) + P_{pir} + D_{debris} \right] \times K_{cal}$$

*   $S_{soil}$: EMA-smoothed soil moisture %
*   $W_{water}$: EMA-smoothed water level %
*   $T_{tilt}$: Slope activity % from Arduino (spike+decay)
*   $P_{pir}$: PIR Motion Bonus (+10.0 if detected during high water)
*   $D_{debris}$: Obstacle Bonus (+8.0 if detected during high water)
*   $K_{cal}$: Sensitivity Multiplier from Potentiometer (0.50 - 2.00)

---

## 3. ARDUINO SLAVE v4.0 ARCHITECTURE

### A. Interrupt Service Routine (ISR)
```cpp
void LANDSLIDE_ISR() {
  tiltTriggered   = true;
  tiltMs          = millis();
  tiltCount++;
  lastTiltEventMs = tiltMs;  // volatile — prevents stale cache
}
```
- **Trigger:** FALLING edge on D2 (INT0)
- **Atomic Guard:** `noInterrupts()` / `interrupts()` in main loop
- **Fix v4.0:** `lastTiltEventMs` declared `volatile` to prevent compiler caching

### B. Slope Activity Estimation
```cpp
// Decay slopePct toward 0 if no tilt events recently
if (now - lastTiltEventMs > TILT_DECAY_MS) {
  slopePct = max(0.0f, slopePct - 2.0f);  // Decay 2% per loop
} else {
  // Spike to 80% on fresh tilt, decay otherwise
  if (tiltNow) slopePct = min(100.0f, slopePct + 30.0f);
}
```
- **TILT_DECAY_MS:** 30000 (30 seconds)
- **Spike Rate:** +30% per fresh tilt event
- **Decay Rate:** -2% per loop cycle (~150ms)

### C. Thermistor Calculation (Steinhart-Hart)
```cpp
// B-parameter equation: 1/T = 1/T0 + (1/B) * ln(R/R0)
float resistance = SERIES_RESISTOR * ((1023.0f / (float)rawTherm) - 1.0f);
float lnR = logf(resistance / THERMISTOR_NOMINAL);
float invT = (1.0f / TEMP_NOMINAL) + (lnR / BCOEFFICIENT);
float tempK = 1.0f / invT;
float tempC = tempK - 273.15f;
```
- **THERMISTOR_NOMINAL:** 10000Ω @ 25°C
- **BCOEFFICIENT:** 3950 (NTC 10k)
- **SERIES_RESISTOR:** 10000Ω

### D. Alert Handling (Non-Blocking)
```cpp
// Critical: alternating 1800/2000 Hz beep — NO delay() inside loop
static bool beepPhase = false;
if (now - lastBuzMs > 450) {
  lastBuzMs = now;
  beepPhase = !beepPhase;
  tone(PIN_BUZZER_PASSIVE, beepPhase ? 1800 : 2000, 180);  // Non-blocking
  buzzerWasOn = true;
}
```
- **Fix v4.0:** Removed blocking `delay(180)` to prevent watchdog timeout
- **tone()** is non-blocking — AVR timer hardware manages duration cutoff

### E. Watchdog Timer (WDT)
```cpp
#include <avr/wdt.h>

void setup() {
  wdt_enable(WDTO_4S);  // Reset if loop hangs > 4 seconds
}

void loop() {
  wdt_reset();  // Kick watchdog every cycle
  // ... rest of loop
}
```
- **Timeout:** WDTO_4S (4 seconds)
- **Purpose:** System recovery from lockups

---

## 4. VISUALIZATION BLUEPRINT

### A. Display Interface Logic
| Display | Role | Refresh Rate | Visual Style |
|:---|:---|:---|:---|
| **OLED (I2C)** | Diagnostic Menu | 500ms | 4-Page Joystick Menu, Waveforms. |
| **LCD (I2C)** | Public Status | 500ms | High-contrast alpha-numeric status. |
| **7-Seg (Uno)** | Local Water Lvl | Real-time | Single digit depth gauge (0-9); shows "9" on tilt. |
| **Dot Matrix** | Alert Icon | 100ms | Dynamic symbol (Dot, Box, Flash). |
| **Web UI** | Remote Monitor | 2000ms | Chart.js dynamic line/bar graphs. |

### B. Unified Color Mapping
*   **NOMINAL (Green/Pulse):** All systems operational, Risk < 3.
*   **CAUTION (Yellow/Amber):** Precursors detected, Risk 4-6.
*   **CRITICAL (Red/Flash):** Evacuation recommended, Risk > 7 or Interrupt Trigger.

---

## 5. WEB DASHBOARD SPECIFICATIONS

*   **Endpoint:** `http://192.168.4.1/data`
*   **Response:** JSON format with 16+ key environmental variables:
    ```json
    {
      "risk": 7.42,
      "hazard": "FLOOD",
      "dist": 45.3,
      "water": 78.5,
      "soil": 32.1,
      "airT": 28.4,
      "airH": 65.2,
      "soilT": 26.8,
      "tilt": 12.3,
      "pot": 1.15,
      "pir": false,
      "obs": false,
      "lsi": false,
      "relay": true,
      "servoPos": 90,
      "light": 45.2
    }
    ```
*   **Frontend Library:** `Chart.js` (CDN loaded).
*   **CSS Architecture:** Flexbox/Grid for mobile responsiveness.
*   **Failsafe:** Red pulsing banner triggered by `lsi` (Landslide Interrupt) flag.

---

## 6. HARDWARE PINOUTS (Unified)

### ESP32 Master
*   **I2C:** SDA (GPIO21), SCL (GPIO22)
*   **SPI:** MOSI (GPIO23), SCK (GPIO26), CS (GPIO15)
*   **Sensors:** DHT (GPIO4), Trig (GPIO5), Echo (GPIO18), PIR (GPIO19), Soil (GPIO36), Obs (GPIO39)
*   **Actuators:** Servo (GPIO13), Relay (GPIO12), LEDs (GPIO25, GPIO2, GPIO27, GPIO33)
*   **UI:** Touch (GPIO14), Joystick X/Y (GPIO34/35), Joystick SW (GPIO32)
*   **Serial2:** RX (GPIO16), TX (GPIO17)

### Arduino Slave (v4.0)
*   **Analog:** Water (A1), Thermistor (A2), Photoresistor (A3), Potentiometer (A4), Seg-F (A5)
*   **Digital:** Tilt (D2/INT0), Active Buzzer (D3), Passive Buzzer (D11/PWM), Segments (D4, D5, D6, D12, D13)
*   **Serial:** TX (D1), RX (D0) — shared with USB programming

---

## 7. SAFETY & RELIABILITY

### A. Voltage Level Shifting
*   **HC-SR04 Echo → ESP32:** 5V to 3.3V via 1kΩ + 2kΩ divider
*   **Arduino TX → ESP32 RX:** 5V to 3.3V via 1kΩ + 2kΩ divider
*   **WARNING:** Direct 5V connection damages ESP32 GPIO

### B. Watchdog Protection
*   **AVR WDT:** 4-second timeout (WDTO_4S)
*   **Kick Interval:** Every loop cycle via `wdt_reset()`
*   **Blocking Code:** Avoid `delay()` > 100ms in main loop

### C. Serial Upload Consideration
*   **D0/D1 Conflict:** Serial pins shared with USB programming
*   **Procedure:** Disconnect ESP32 serial wires before Arduino firmware upload
*   **Alternative:** Use SoftwareSerial on D8/D9 for simultaneous operation

---

## 8. VERSION HISTORY

### v4.0 (Current)
- **FIX:** `lastTiltEventMs` declared `volatile` — prevents stale value caching
- **FIX:** Removed blocking `delay(180)` from handleAlerts() — prevents WDT timeout
- **FIX:** Serial buffer overflow resets cleanly with log byte
- **NEW:** Photoresistor transmitted via `L:` field
- **NEW:** AVR hardware watchdog (WDTO_4S)
- **NOTE:** D0/D1 serial pins shared with USB — disconnect before upload

### v3.0
- Steinhart-Hart thermistor formula corrected
- Photoresistor transmitted to ESP32
- TiltPct as spike+decay % via S: field
- ISR flag read with noInterrupts() atomic guard
- Passive buzzer cleared when tilt times out

### v2.1
- Initial blueprint documentation
- Risk calculation algorithm defined

---

*GEO-SENSE AFRICA v4.0 | Technical Blueprint | March 2026*
