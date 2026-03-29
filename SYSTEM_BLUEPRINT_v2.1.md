# GEO-SENSE AFRICA v2.1 — SYSTEM BLUEPRINT
## Multi-Hazard Early Warning System (MHEWS) Architecture

This blueprint defines the technical specifications, data models, and visualization logic for the Geo-Sense Africa v2.1 platform.

---

## 1. SYSTEM HIERARCHY

### Layer 1: Perception (Sensors)
*   **Aqueous:** HC-SR04 (Ultrasound), Water Level Sensor (Conductivity), Obstacle IR (Debris).
*   **Geological:** Tilt Switch (Mercury/Ball), Soil Moisture (Capacitive/Resistive).
*   **Atmospheric:** DHT11 (Temp/Humidity), Photoresistor (Solar Intensity).
*   **Community:** HC-SR501 PIR (Evacuation Detection), TTP223B (Manual ACK).

### Layer 2: Edge Processing (Arduino Uno Slave)
*   **Role:** Raw Signal Normalization.
*   **Priority:** Hardware Interrupt (INT0) for Landslide detection.
*   **Outputs:** Local 7-Segment Display (Water Level 0-9), Local Buzzer.
*   **Upstream:** 9600 Baud Serial Packet Transmission.

### Layer 3: Fusion & Gateway (ESP32 Master)
*   **Role:** Multi-sensor Data Fusion, Risk Index Calculation.
*   **Smoothing:** Exponential Moving Average (EMA) with $\alpha=0.25$.
*   **Connectivity:** WiFi Access Point (AP), Web Server, I2C Display Driver.

---

## 2. DATA MODELS & COMMUNICATION

### A. Serial Protocol (Slave ──► Master)
*   **Format:** `W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x\n`
*   **Payload:**
    *   `W`: Water Level % (0.0 - 100.0)
    *   `T`: Soil Temperature °C
    *   `S`: Tilt Angle % (computed as slope)
    *   `P`: Potentiometer Sensitivity (0.50x - 2.00x)
    *   `I`: Landslide Interrupt (0 or 1)

### B. Risk Calculation Logic (Fusion Engine)
The system calculates a **Weighted Risk Index (0-9)** based on environmental precursors:

$$Risk = \left[ (S_{soil} \times 0.3) + (W_{water} \times 0.4) + (T_{tilt} \times 0.3) + P_{pir} + D_{debris} \right] \times K_{cal}$$

*   $P_{pir}$: PIR Motion Bonus (+10.0 if detected during high water).
*   $D_{debris}$: Obstacle Bonus (+8.0 if detected during high water).
*   $K_{cal}$: Sensitivity Multiplier from Potentiometer.

---

## 3. VISUALIZATION BLUEPRINT

### A. Display Interface Logic
| Display | Role | Refresh Rate | Visual Style |
|:---|:---|:---|:---|
| **OLED (I2C)** | Diagnostic Menu | 500ms | 4-Page Joystick Menu, Waveforms. |
| **LCD (I2C)** | Public Status | 500ms | High-contrast alpha-numeric status. |
| **7-Seg (Uno)** | Local Water Lvl | Real-time | Single digit depth gauge (0-9). |
| **Dot Matrix** | Alert Icon | 100ms | Dynamic symbol (Dot, Box, Flash). |
| **Web UI** | Remote Monitor | 2000ms | Chart.js dynamic line/bar graphs. |

### B. Unified Color Mapping
*   **NOMINAL (Green/Pulse):** All systems operational, Risk < 3.
*   **CAUTION (Yellow/Amber):** Precursors detected, Risk 4-6.
*   **CRITICAL (Red/Flash):** Evacuation recommended, Risk > 7 or Interrupt Trigger.

---

## 4. WEB DASHBOARD SPECIFICATIONS

*   **Endpoint:** `http://192.168.4.1/data`
*   **Response:** JSON format with 16 key environmental variables.
*   **Frontend Library:** `Chart.js` (CDN loaded).
*   **CSS Architecture:** Flexbox/Grid for mobile responsiveness.
*   **Failsafe:** Red pulsing banner triggered by `lsi` (Landslide Interrupt) flag.

---

## 5. HARDWARE PINOUTS (Unified)

### ESP32 Master
*   **I2C:** SDA (21), SCL (22)
*   **SPI:** MOSI (23), SCK (18), CS (15)
*   **Sensors:** DHT (4), Trig (5), Echo (18), PIR (19), Soil (36), Obs (39)
*   **Actuators:** Servo (13), Relay (12), LEDs (25, 26, 27, 33)

### Arduino Slave
*   **Analog:** Water (A1), Thermistor (A2), Photo (A3), Pot (A4), Seg-F (A5)
*   **Digital:** Tilt (2), BuzzerA (3), BuzzerP (11), Seg (4, 5, 6, 12, 13)

---
*GEO-SENSE AFRICA v2.1 | Technical Blueprint | March 2026*
