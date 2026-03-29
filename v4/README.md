# Geo-Sense Africa v4.0 — Dual-Board Multi-Hazard Early Warning System

[![Hackathon](https://img.shields.io/badge/Hackathon-GeoSense%20Africa-orange)]()
[![Platform](https://img.shields.io/badge/Platform-ESP32%20%2B%20Arduino%20Uno-blue)]()
[![License](https://img.shields.io/badge/License-MIT-green)]()

**Africa's last-mile disaster shield — built from two starter kits.**

Geo-Sense Africa is a **dual-board multi-hazard early warning system (MHEWS)** designed to detect floods, droughts, landslides, and environmental precursors in real-time. The system features a WiFi dashboard with live Chart.js visualizations, multiple display outputs (OLED, LCD, 7-segment, dot matrix), and automated alert mechanisms (servo flag, relay siren, buzzers, LEDs).

---

## 🌍 System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    GEO-SENSE AFRICA v4.0                        │
│         Dual-Board Multi-Hazard Early Warning System            │
│                                                                 │
│  ┌──────────────────────┐    Serial (9600)   ┌───────────────┐ │
│  │   ESP32 MASTER NODE  │◄──────────────────►│ ARDUINO SLAVE │ │
│  │                      │                    │               │ │
│  │  • WiFi Access Point │                    │ • Water Level │ │
│  │  • Web Dashboard     │                    │ • Thermistor  │ │
│  │  • OLED 128x64       │                    │ • Tilt Switch │ │
│  │  • LCD 1602 I2C      │                    │ • 7-Segment   │ │
│  │  • Dot Matrix 8x8    │                    │ • Active Buzzer││
│  │  • HC-SR04 (Flood)   │                    │ • Passive Buzzer││
│  │  • DHT11 (Air)       │                    │ • Photoresistor││
│  │  • PIR Motion        │                    │ • Potentiometer││
│  │  • Soil Moisture     │                    │               │ │
│  │  • SG90 Servo        │                    │               │ │
│  │  • Relay (Siren)     │                    │               │ │
│  │  • 4x Status LEDs    │                    │               │ │
│  └──────────────────────┘                    └───────────────┘ │
│           │                                                     │
│      WiFi: GeoSense-Africa                                      │
│      Pass: geosense2024                                         │
│      URL:  http://192.168.4.1                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🎯 Key Features

### Hazard Detection
| Hazard | Sensors | Detection Method | Alert Threshold |
|--------|---------|------------------|-----------------|
| **Flood** | Water Level, HC-SR04, Obstacle IR | River level + surface distance | >60% water level |
| **Drought** | Soil Moisture, DHT11, Thermistor | Soil dryness + air temp + evapotranspiration | >70% soil moisture |
| **Landslide** | Tilt Switch (Hardware Interrupt) | Geological slope instability | Any tilt trigger |
| **Motion/Intrusion** | PIR Sensor | Human/animal movement detection | Motion detected |

### Real-Time Dashboard
- **Live Updates**: AJAX polling every 2 seconds (no page refresh)
- **Chart.js Visualizations**: Risk progression, flood trends, soil moisture history
- **Responsive Design**: Mobile-first, works on phones/tablets/laptops
- **Critical Alerts**: Pulsing red banner for emergency scenarios
- **System Uptime**: Live counter showing operational duration

### Multi-Display Output
| Display | Type | Purpose |
|---------|------|---------|
| OLED 128x64 | I2C SSD1306 | Risk gauge, menu navigation |
| LCD 1602 | I2C HD44780 | Quick status readout |
| 7-Segment | Direct drive | Water level digit (Arduino) |
| Dot Matrix 8x8 | SPI MAX7219 | Scrolling alert indicator |

### Alert Mechanisms
- **Visual**: 4x LEDs (Green/Blue/Yellow/Red) for hazard type
- **Audible**: Active + Passive buzzers with tone patterns
  - Active buzzer: DC-driven for continuous alarm
  - Passive buzzer: PWM tone() for frequency-controlled alerts
- **Mechanical**: SG90 servo flag (0°=safe, 180°=evacuate)
- **Electrical**: Relay module for external siren/lamp
- **Digital**: WiFi dashboard push notifications

---

## 🏗️ Architecture

### Master-Slave Communication

```
ESP32 (Master)                          Arduino Uno (Slave)
┌─────────────────┐                     ┌─────────────────┐
│  WiFi Server    │                     │  Raw Sensors    │
│  Display Mgmt   │◄──── Serial2 ─────►│  Interrupts     │
│  Risk Compute   │      (9600 baud)    │  Buzzer Control │
│  Output Control │                     │  7-Seg Display  │
└─────────────────┘                     └───────────────┬─┘
         │                                              │
         └───────────────┬──────────────────────────────┘
                         │
                  Shared Ground
```

### Data Flow

1. **Sensor Acquisition** (Every 100-300ms)
   - ESP32 reads: DHT11, Soil, HC-SR04, PIR, Obstacle
   - Arduino reads: Water, Thermistor, Tilt, Photoresistor, Pot

2. **Serial Fusion** (Every 2s)
   - Arduino transmits: `W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x`
     - **W** = Water level % (0.0–100.0)
     - **T** = Soil temperature °C (Steinhart-Hart corrected)
     - **S** = Slope activity % (spike+decay from tilt events)
     - **P** = Potentiometer calibration (0.50×–2.00×)
     - **I** = ISR tilt flag (0 or 1)
     - **L** = Light intensity % (photoresistor, NEW in v4.0)
   - ESP32 sends commands: `CMD:alert_level`

3. **Risk Computation** (Every 3s)
   ```
   Weighted Risk = (Soil × 0.3) + (Water × 0.4) + (Tilt × 0.3)
                 + PIR_bonus(10) + Obstacle_bonus(8)
                 × Pot_Calibration
   ```

4. **Output Update** (Continuous)
   - LEDs, Servo, Relay, Displays, Web Dashboard

### Risk Index Algorithm

```cpp
// Exponential Moving Average smoothing
ema_soil  = (0.25 × soil) + (0.75 × ema_soil)
ema_water = (0.25 × water) + (0.75 × ema_water)

// Weighted fusion
weighted = (ema_soil × 0.3) + (ema_water × 0.4) + (tilt × 0.3)
if (PIR_motion)     weighted += 10
if (Obstacle)       weighted += 8

// Calibration & normalization
weighted = constrain(weighted × potCalibration, 0, 100)
riskIndex = (weighted / 100) × 9.0  // Scale to 0-9

// Hazard classification
if (water > 60)     hazard = "FLOOD"
else if (soil > 70) hazard = "DROUGHT"
else                hazard = "NOMINAL"

// Alert levels
if (tilt_interrupt) alert = CRITICAL (2)
else if (risk > 6)  alert = WARNING (2)
else if (risk > 3)  alert = CAUTION (1)
else                alert = SAFE (0)
```

---

## 🔌 Hardware Setup

### ESP32 Master Pinout

| Component | Pin | Type | Notes |
|-----------|-----|------|-------|
| **I2C Bus** | | | |
| OLED SDA/SCL | GPIO21/22 | I2C | Address 0x3C |
| LCD SDA/SCL | GPIO21/22 | I2C | Address 0x27 |
| **Sensors** | | | |
| DHT11 | GPIO4 | Digital | 10k pull-up |
| HC-SR04 Trig | GPIO5 | Output | |
| HC-SR04 Echo | GPIO18 | Input | **Voltage divider required** |
| PIR Motion | GPIO19 | Input | HC-SR501 |
| Soil Moisture | GPIO36 | ADC | ADC1 only |
| Obstacle IR | GPIO39 | ADC | ADC1 only |
| **Outputs** | | | |
| SG90 Servo | GPIO13 | PWM | 5V separate |
| Relay | GPIO12 | Output | Active LOW |
| Dot Matrix DIN | GPIO23 | SPI (MOSI) | |
| Dot Matrix CLK | GPIO26 | SPI (SCK) | |
| Dot Matrix CS | GPIO15 | SPI (SS) | |
| Green LED | GPIO25 | Output | 220Ω |
| Blue LED | GPIO2 | Output | 220Ω |
| Yellow LED | GPIO27 | Output | 220Ω |
| Red LED | GPIO33 | Output | 220Ω |
| **UI** | | | |
| TTP223 Touch | GPIO14 | Input | |
| Joystick X | GPIO34 | ADC | |
| Joystick Y | GPIO35 | ADC | |
| Joystick SW | GPIO32 | Input | 10k pull-up |
| **Serial** | | | |
| Slave RX | GPIO16 | UART2 | Via voltage divider |
| Slave TX | GPIO17 | UART2 | Via voltage divider |

### Arduino Slave Pinout (v4.0)

| Component | Pin | Type | Notes |
|-----------|-----|------|-------|
| **Sensors** | | | |
| Water Level | A1 | ADC | 0–700 mapped to 0–100% |
| Thermistor | A2 | ADC | 10k NTC with Steinhart-Hart |
| Photoresistor | A3 | ADC | Light intensity % (transmitted as L:) |
| Potentiometer | A4 | ADC | Calibration 0.5× to 2.0× |
| Tilt Switch | D2 | Interrupt | FALLING edge, INT0 |
| **Outputs** | | | |
| Active Buzzer | D3 | Output | DC-driven (HIGH = on) |
| Passive Buzzer | D11 | PWM | tone() frequency control |
| 7-Segment A-F | D4,D5,D6,D12,D13,A5 | Output | 220Ω each, common cathode |
| **Serial** | | | |
| TX to ESP32 | D1 | UART | **Voltage divider required** |
| RX from ESP32 | D0 | UART | CMD:alert_level |

### Critical Voltage Dividers

**HC-SR04 Echo (5V → 3.3V):**
```
Echo ──► 1kΩ ──► GPIO18
                  │
                 2kΩ
                  │
                 GND
```

**Arduino TX to ESP32 (5V → 3.3V):**
```
Arduino TX ──► 1kΩ ──► ESP32 GPIO16
                        │
                       2kΩ
                        │
                       GND
```

⚠️ **WARNING**: Without voltage dividers, 5V signals will damage the ESP32!

---

## 📦 Required Libraries

### ESP32 (Install via Arduino IDE Library Manager)
```
Adafruit SSD1306
Adafruit GFX Library
LiquidCrystal I2C
DHT sensor library (Adafruit)
Adafruit Unified Sensor
ESP32Servo
MDMAX72XX
```

### Arduino Uno
```
No external libraries required (uses avr/wdt.h built-in)
```

### Board Setup
1. Open Arduino IDE → File → Preferences
2. Add Board Manager URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Board Manager → Search "esp32" → Install
4. Select: **ESP32 Dev Module**

---

## 🚀 Deployment

### Flash ESP32 Master
1. Open `GeoSense_ESP32_Master.ino` in Arduino IDE
2. Select board: **ESP32 Dev Module**
3. Select port: (auto-detected or COM3/COM4)
4. Click Upload
5. Wait for boot sequence (LED chase, servo sweep)

### Flash Arduino Slave
1. Open `GeoSense_Arduino_Slave_v4.ino` in Arduino IDE
2. Select board: **Arduino Uno**
3. Select port: (auto-detected)
4. Click Upload
5. Watch 7-segment count 0-9 on boot

⚠️ **NOTE**: D0/D1 (Serial RX/TX) are shared with USB programming. Always disconnect the ESP32 serial wires before uploading new firmware to the Arduino, or use a SoftwareSerial pair on D8/D9 if simultaneous upload+operation is needed.

### Access WiFi Dashboard
1. Power on both boards
2. On phone/laptop, connect to WiFi: **GeoSense-Africa**
3. Password: **geosense2024**
4. Open browser: **http://192.168.4.1**
5. Dashboard updates every 2 seconds automatically

---

## 🎮 Demo Scenarios

### Scenario 1: Flood Detection (1:15)
1. Pour water into tray with water level sensor
2. Move hand toward HC-SR04 (reduces distance)
3. **Observe**: Blue LED, servo to 90°, dashboard flood chart spikes

### Scenario 2: Landslide Emergency (2:00)
1. Tilt the cardboard wedge (>15° angle)
2. **Observe**:
   - Passive buzzer screams (2.5kHz continuous)
   - Active buzzer ON (DC-driven)
   - Red LED flashes
   - Servo slams to 180°
   - Dashboard: Pulsing red banner "LANDSLIDE TRIGGERED! EVACUATE NOW"
   - 7-segment displays "9" (tilt indicator)

### Scenario 3: Drought Monitoring (2:45)
1. Navigate OLED to Page 2 using joystick
2. Show soil moisture + temperature readings
3. **Observe**: Dashboard soil trend graph, Yellow LED if dry

### Scenario 4: Alert Acknowledge (3:30)
1. Press TTP223 touch sensor
2. **Observe**: Buzzer silences, relay resets for 60s grace period

---

## 🐛 Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| ESP32 crashes on boot | Missing libraries | Install all 7 libraries |
| OLED blank | Wrong I2C address | Try 0x3D instead of 0x3C |
| LCD shows garbage | Wrong I2C address | Try 0x3F instead of 0x27 |
| No Serial2 data | Wrong TX/RX wiring | Verify RX=16, TX=17; check voltage divider |
| Servo jitters | Power noise | Add 100µF cap across 5V/GND near servo |
| HC-SR04 reads 0 | Echo voltage high | Verify 1kΩ+2kΩ divider |
| PIR always triggered | Sensitivity max | Turn left trim pot anticlockwise |
| WiFi not visible | Boot incomplete | Wait 5 seconds after power-on |
| ESP32 damaged | No voltage divider | **CRITICAL** — add 1kΩ+2kΩ on Arduino TX |
| Arduino resets randomly | Watchdog timeout | Check loop blocking code; ensure wdt_reset() called |
| Slope% stuck at 0 | Stale tilt timestamp | Verify v4.0 fix: lastTiltEventMs is volatile |

---

## 📊 Dashboard API

### GET /data
Returns real-time sensor fusion data:

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

### GET /
Returns the complete HTML dashboard with embedded Chart.js visualizations.

---

## 💰 Bill of Materials

| Item | Quantity | Est. Cost (USD) |
|------|----------|-----------------|
| ESP32 Dev Module | 1 | $8 |
| Arduino Uno R3 | 1 | $10 |
| OLED 128x64 I2C | 1 | $4 |
| LCD 1602 I2C | 1 | $5 |
| DHT11 | 1 | $3 |
| HC-SR04 | 1 | $2 |
| Water Level Sensor | 1 | $3 |
| Soil Moisture Sensor | 1 | $2 |
| PIR Motion (HC-SR501) | 1 | $3 |
| Tilt Switch | 1 | $1 |
| SG90 Servo | 1 | $3 |
| Relay Module 1CH | 1 | $3 |
| Dot Matrix 8x8 (MAX7219) | 1 | $4 |
| 7-Segment Display | 1 | $1 |
| LEDs (assorted) | 4 | $1 |
| Buzzers (active+passive) | 2 | $2 |
| Sensors (photoresistor, thermistor, pot) | 3 | $3 |
| Joystick Module | 1 | $2 |
| TTP223 Touch | 1 | $2 |
| Resistors, wires, breadboards | - | $10 |
| **Total** | | **~$72** |

---

## 📈 System Specifications

| Parameter | Value |
|-----------|-------|
| **Power** | 5V 2A (USB or external) |
| **WiFi Range** | ~50m (open space) |
| **Sensor Update** | 100-300ms |
| **Dashboard Refresh** | 2s |
| **Risk Computation** | 3s |
| **Alert Response** | <1s |
| **Operating Temp** | 0-50°C |
| **Max Clients** | 5 simultaneous |
| **Watchdog Timeout** | 4s (AVR hardware WDT) |

---

## 🤝 Contributing

This project was built for the Hack Keele Hackathon. Feel free to:
- Fork and modify for your own hazard detection needs
- Add new sensors (rain, gas, vibration)
- Implement MQTT/LoRaWAN for long-range communication
- Add cloud logging (Thingspeak, Blynk, AWS IoT)
- Create mobile apps (Android/iOS)

---

## 📄 License

MIT License — Built with ❤️ for Africa's climate resilience

---

## 📝 Version History

### v4.0 (Current)
- **FIX**: `lastTiltEventMs` declared `volatile` — prevents stale value caching causing slopePct decay failure
- **FIX**: Removed blocking `delay(180)` from handleAlerts() double-beep path — prevents watchdog timeout
- **FIX**: Serial buffer overflow now resets cleanly with log byte instead of silent discard
- **NEW**: Photoresistor (LDR) transmitted to ESP32 via `L:` field in serial protocol
- **NEW**: AVR hardware watchdog via `avr/wdt.h` (WDTO_4S)
- **NOTE**: D0/D1 serial pins shared with USB — disconnect ESP32 wires before Arduino upload

### v3.0
- Steinhart-Hart thermistor formula (algebraically corrected)
- photoPct transmitted to ESP32 via L: field
- tiltPct transmitted as real spike+decay % via S: field
- ISR flag read with noInterrupts() atomic guard
- Passive buzzer cleared when tilt times out

### v2.0
- Initial dual-board architecture
- Master-slave serial communication
- WiFi dashboard with Chart.js
