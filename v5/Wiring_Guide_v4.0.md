# GEO-SENSE AFRICA v4.0 — COMPLETE WIRING GUIDE
## ESP32 Master + Arduino Uno Slave | Dual-Board Multi-Hazard Early Warning System

**Version:** 4.0 (Updated with v4.0 Arduino Slave)
**Last Updated:** March 2026
**Difficulty:** Intermediate
**Estimated Time:** 2-3 hours

---

## 📋 TABLE OF CONTENTS

1. [System Overview](#system-overview)
2. [Components Checklist](#components-checklist)
3. [Tools Required](#tools-required)
4. [ESP32 Master Wiring](#part-a--esp32-master-wiring)
5. [Arduino Slave Wiring](#part-b--arduino-uno-slave-wiring)
6. [Inter-Board Connection](#part-c--inter-board-communication)
7. [Power Distribution](#part-d--power-distribution)
8. [Physical Layout](#physical-station-layout)
9. [Step-by-Step Assembly](#step-by-step-assembly-guide)
10. [Testing & Verification](#testing--verification)
11. [Troubleshooting](#troubleshooting)

---

## 🔍 SYSTEM OVERVIEW

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GEO-SENSE AFRICA v4.0                               │
│                 Dual-Board Multi-Hazard Early Warning System                │
│                                                                             │
│   ┌─────────────────────────┐         ┌─────────────────────────┐          │
│   │    ESP32 MASTER NODE    │         │   ARDUINO UNO SLAVE     │          │
│   │  ┌───────────────────┐  │         │  ┌───────────────────┐  │          │
│   │  │ WiFi Web Server   │  │         │  │ Water Level (A1)  │  │          │
│   │  │ OLED 128x64 I2C   │  │         │  │ Thermistor (A2)   │  │          │
│   │  │ LCD 1602 I2C      │  │         │  │ Photoresistor(A3) │  │          │
│   │  │ Dot Matrix 8x8    │  │         │  │ Tilt Switch (D2)  │  │          │
│   │  │ DHT11 (Air T/H)   │  │         │  │ Active Buzzer(D3) │  │          │
│   │  │ HC-SR04 (Flood)   │  │         │  │ Passive Buzzer(D11)│ │          │
│   │  │ PIR Motion        │  │         │  │ 7-Segment Display │  │          │
│   │  │ Soil Moisture     │  │         │  │ Potentiometer(A4) │  │          │
│   │  │ SG90 Servo        │  │         │  │                   │  │          │
│   │  │ Relay (Siren)     │  │         │  │                   │  │          │
│   │  │ 4x Status LEDs    │  │         │  │                   │  │          │
│   │  │ Joystick UI       │  │         │  │                   │  │          │
│   │  │ Touch Sensor      │  │         │  │                   │  │          │
│   │  │ Obstacle IR       │  │         │  │                   │  │          │
│   │  └───────────────────┘  │         │  └───────────────────┘  │          │
│   └───────────┬─────────────┘         └───────────┬─────────────┘          │
│               │                                   │                         │
│               └───────── Serial2 (UART) ──────────┘                         │
│                    TX2 (GPIO17) ◄──► RX (D0)                                │
│                    RX2 (GPIO16) ◄──► TX (D1) [via voltage divider]          │
│                         GND ────────── GND (COMMON)                         │
│                                                                             │
│   WiFi Access Point: "GeoSense-Africa" | Password: "geosense2024"           │
│   Dashboard URL: http://192.168.4.1                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 COMPONENTS CHECKLIST

### Main Boards
| Item | Quantity | Notes |
|------|----------|-------|
| ESP32 Dev Module (DOIT ESP32 DEVKIT V1) | 1 | 30-pin or 36-pin version |
| Arduino Uno R3 (or compatible) | 1 | ATmega328P |

### Displays
| Item | Quantity | I2C Address | Notes |
|------|----------|-------------|-------|
| OLED 128x64 SSD1306 | 1 | 0x3C | 4-pin I2C (VCC, GND, SDA, SCL) |
| LCD 1602 with I2C backpack | 1 | 0x27 | 4-pin I2C (VCC, GND, SDA, SCL) |
| 7-Segment Display (common cathode) | 1 | N/A | Single digit, 7 pins (A-F + common) |
| MAX7219 8x8 Dot Matrix | 1 | N/A | 4-pin SPI (VCC, GND, DIN, CLK, CS) |

### Sensors - ESP32 Side
| Item | Quantity | Notes |
|------|----------|-------|
| DHT11 Temperature/Humidity | 1 | 3-pin or 4-pin |
| HC-SR04 Ultrasonic | 1 | For flood distance |
| HC-SR501 PIR Motion | 1 | 3-pin |
| Capacitive Soil Moisture v1.2 | 1 | 3-pin (analog output) |
| IR Obstacle Avoidance | 1 | 3-pin (digital output) |
| TTP223B Touch Sensor | 1 | 3-pin |
| KY-023 Joystick Module | 1 | 5-pin (VCC, GND, VRx, VRy, SW) |

### Sensors - Arduino Side (v4.0)
| Item | Quantity | Notes |
|------|----------|-------|
| Water Level Sensor | 1 | Analog output (A1) |
| Thermistor 10k NTC | 1 | With 10k resistor (A2) |
| Photoresistor (LDR) | 1 | With 10k resistor (A3) — NEW in v4.0 |
| Potentiometer 10k | 1 | For sensitivity calibration (A4) |
| Tilt Switch SW-520D | 1 | Digital (open/closed) (D2) |

### Outputs
| Item | Quantity | Notes |
|------|----------|-------|
| SG90 Micro Servo | 1 | 3-pin (brown=GND, red=5V, orange=SIG) |
| 1-Channel Relay Module | 1 | Active LOW trigger |
| Active Buzzer 5V | 1 | 2-pin (polarized, DC-driven) |
| Passive Buzzer 5V | 1 | 2-pin (non-polarized, needs tone()) |
| LED 5mm Green | 1 | Status indicator |
| LED 5mm Blue | 1 | Flood indicator |
| LED 5mm Yellow | 1 | Drought indicator |
| LED 5mm Red | 1 | Critical alert |

### Miscellaneous
| Item | Quantity | Notes |
|------|----------|-------|
| Resistors 220Ω | 8 | For LEDs and 7-segment (red-red-brown-gold) |
| Resistors 1kΩ | 4 | Voltage dividers (brown-black-red-gold) |
| Resistors 2kΩ | 2 | Voltage dividers (red-black-red-gold) |
| Resistors 10kΩ | 4 | Pull-ups (brown-black-orange-gold) |
| Breadboard 400-point | 2 | For component mounting |
| Jumper Wires M-M | 40 | Assorted colors |
| Jumper Wires M-F | 20 | For sensor connections |
| Jumper Wires F-F | 10 | For I2C bus |
| USB Cable Micro-USB | 2 | For ESP32 and Arduino |
| External 5V 2A Supply | 1 | Optional, for servo stability |

---

## 🛠️ TOOLS REQUIRED

- **Soldering iron** (for 7-segment resistor connections)
- **Wire strippers/cutters**
- **Multimeter** (for continuity and voltage checks)
- **Small Phillips screwdriver** (for terminal blocks)
- **Tweezers** (for small components)
- **Hot glue gun** (for securing components)
- **Cardboard/foam board** (for physical layout base)

---

## PART A — ESP32 MASTER WIRING

### Pin Reference Table (Quick Lookup)

| GPIO | Function | Type | Notes |
|------|----------|------|-------|
| GPIO21 | I2C SDA | Output | Shared with LCD/OLED |
| GPIO22 | I2C SCL | Output | Shared with LCD/OLED |
| GPIO4 | DHT11 DATA | Digital | 10k pull-up required |
| GPIO5 | HC-SR04 TRIG | Output | |
| GPIO18 | HC-SR04 ECHO | Input | **Voltage divider required** |
| GPIO19 | PIR OUT | Input | |
| GPIO13 | Servo PWM | Output | |
| GPIO12 | Relay IN | Output | Active LOW |
| GPIO14 | Touch SIG | Input | |
| GPIO34 | Joystick VRx | ADC | Input only |
| GPIO35 | Joystick VRy | ADC | Input only |
| GPIO32 | Joystick SW | Input | 10k pull-up |
| GPIO23 | Dot Matrix DIN | SPI MOSI | |
| GPIO26 | Dot Matrix CLK | SPI SCK | |
| GPIO15 | Dot Matrix CS | SPI SS | |
| GPIO25 | Green LED | Output | Via 220Ω |
| GPIO2 | Blue LED | Output | Via 220Ω |
| GPIO27 | Yellow LED | Output | Via 220Ω |
| GPIO33 | Red LED | Output | Via 220Ω |
| GPIO36 | Soil Signal | ADC | Input only |
| GPIO39 | Obstacle OUT | ADC | Input only |
| GPIO16 | Serial2 RX | Input | From Arduino TX (via divider) |
| GPIO17 | Serial2 TX | Output | To Arduino RX |

---

### A1 — I2C DISPLAY BUS (OLED + LCD)

**Both displays share the same I2C bus but have different addresses.**

```
┌─────────────────────────────────────────────────────────────────┐
│                        I2C BUS WIRING                           │
│                                                                 │
│   ESP32              OLED 0x3C              LCD 0x27           │
│   ┌─────┐           ┌──────────┐          ┌──────────┐        │
│   │GPIO21├─SDA──────┤SDA       │──────────┤SDA       │        │
│   │GPIO22├─SCL──────┤SCL       │──────────┤SCL       │        │
│   │ 3.3V├─VCC───────┤VCC       │          │VCC       │◄──5V   │
│   │  GND├─GND───────┤GND       │──────────┤GND       │        │
│   └─────┘           └──────────┘          └──────────┘        │
│                                                                 │
│   NOTE: OLED uses 3.3V, LCD uses 5V (both OK on same I2C)      │
└─────────────────────────────────────────────────────────────────┘
```

**Step-by-Step:**

1. Connect ESP32 GPIO21 to OLED SDA pin
2. Connect ESP32 GPIO22 to OLED SCL pin
3. Connect ESP32 3.3V to OLED VCC pin
4. Connect ESP32 GND to OLED GND pin
5. Connect OLED SDA to LCD SDA (daisy-chain)
6. Connect OLED SCL to LCD SCL (daisy-chain)
7. Connect Arduino 5V to LCD VCC
8. Connect LCD GND to common ground

**⚠️ IMPORTANT:** Verify I2C addresses:
- OLED default: **0x3C**
- LCD default: **0x27** (some use 0x3F)
- If displays don't work, run I2C scanner sketch to verify addresses

---

### A2 — DHT11 TEMPERATURE/HUMIDITY SENSOR

```
┌─────────────────────────────────────────────────────────────────┐
│                      DHT11 WIRING                               │
│                                                                 │
│   DHT11 (4-pin view, front)        ESP32                       │
│   ┌─────────────────┐                                          │
│   │  1  2  3  4     │                                          │
│   │  │  │  │  │     │                                          │
│   │  │  └──┼──┘     │                                          │
│   │  │     │        │                                          │
│   │  │     └────────┼──► GPIO4 (DATA)                          │
│   │  │              │                                          │
│   │  └──────────────┼──► 3.3V (Pin 1)                          │
│   │                 │                                          │
│   └─────────────────┼──► GND (Pin 4)                           │
│                     │                                          │
│   10kΩ Resistor:    │                                          │
│   GPIO4 ──┬──10kΩ──┬──► 3.3V                                   │
│           │        │                                          │
│          DATA     VCC                                         │
└─────────────────────────────────────────────────────────────────┘
```

**Pin Identification (DHT11 facing you, grille forward):**
- Pin 1 (left): VCC (3.3V)
- Pin 2: DATA (GPIO4)
- Pin 3: NC (not connected)
- Pin 4 (right): GND

**Steps:**
1. Connect DHT11 Pin 1 to ESP32 3.3V
2. Connect DHT11 Pin 2 to ESP32 GPIO4
3. Connect DHT11 Pin 4 to ESP32 GND
4. Solder 10kΩ resistor between Pin 1 (VCC) and Pin 2 (DATA)

---

### A3 — HC-SR04 ULTRASONIC FLOOD SENSOR

```
┌─────────────────────────────────────────────────────────────────┐
│                   HC-SR04 WIRING (with voltage divider)         │
│                                                                 │
│   HC-SR04                    ESP32                              │
│   ┌──────────────┐                                              │
│   │  +5V   TRIG  │──► GPIO5                                     │
│   │              │                                              │
│   │  ECHO  GND   │                                              │
│   │   │          │                                              │
│   │   └─────┬────┘                                              │
│   │         │                                                   │
│   │      ┌──┴──┐                                                │
│   │      │ 1kΩ │ (brown-black-red-gold)                         │
│   │      └──┬──┘                                                │
│   │         ├──────────────► GPIO18 (ECHO input)                │
│   │      ┌──┴──┐                                                │
│   │      │ 2kΩ │ (red-black-red-gold)                           │
│   │      └──┬──┘                                                │
│   │         │                                                   │
│   └─────────┴──────────────► GND                                │
│                                                                 │
│   VCC: Connect to 5V (Arduino or external)                      │
└─────────────────────────────────────────────────────────────────┘
```

**⚠️ CRITICAL:** The HC-SR04 ECHO pin outputs 5V logic, but ESP32 GPIO is 3.3V max!
**You MUST use a voltage divider or you will damage the ESP32.**

**Voltage Divider Calculation:**
```
V_out = V_in × (R2 / (R1 + R2))
V_out = 5V × (2kΩ / (1kΩ + 2kΩ)) = 5V × 0.667 = 3.33V ✓
```

**Steps:**
1. Connect HC-SR04 VCC to 5V
2. Connect HC-SR04 GND to GND
3. Connect HC-SR04 TRIG to ESP32 GPIO5
4. Connect HC-SR04 ECHO to 1kΩ resistor
5. Connect other end of 1kΩ to ESP32 GPIO18
6. Connect 2kΩ resistor from GPIO18 junction to GND

---

### A4 — HC-SR501 PIR MOTION SENSOR

```
┌─────────────────────────────────────────────────────────────────┐
│                      PIR SENSOR WIRING                          │
│                                                                 │
│   HC-SR501 (bottom view)         ESP32                          │
│   ┌──────────────────┐                                          │
│   │   ┌─────────┐    │                                          │
│   │   │  LENS   │    │                                          │
│   │   └─────────┘    │                                          │
│   │   ○ ○ ○          │                                          │
│   │   │ │ │          │                                          │
│   │   │ │ └──────────┼──► GND                                   │
│   │   │ └────────────┼──► OUT ──► GPIO19                        │
│   │   └──────────────┼──► VCC ──► 5V                            │
│   │                  │                                          │
│   │  [Trim Pots]     │                                          │
│   │   Left: Sensitivity  (CW = more sensitive)                  │
│   │   Right: Time delay (CCW = shorter ~3s)                     │
│   └──────────────────┘                                          │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect PIR VCC to 5V
2. Connect PIR GND to GND
3. Connect PIR OUT to ESP32 GPIO19
4. Adjust trim pots:
   - **Left pot:** Turn fully clockwise for max sensitivity
   - **Right pot:** Turn fully counter-clockwise for minimum delay

---

### A5 — SG90 SERVO MOTOR (WARNING FLAG)

```
┌─────────────────────────────────────────────────────────────────┐
│                      SERVO WIRING                               │
│                                                                 │
│   SG90 Servo (female connector)    ESP32/Power                   │
│   ┌───────────────────┐                                         │
│   │ Brown  Red  Orange│                                         │
│   │   │    │     │    │                                         │
│   │   │    │     └────┼────► GPIO13 (PWM)                       │
│   │   │    │          │                                         │
│   │   │    └─────────┼────► 5V (separate supply recommended)    │
│   │   │              │                                         │
│   │   └──────────────┼────► GND (common with ESP32)             │
│   │                  │                                         │
│   └──────────────────┘                                         │
│                                                                 │
│   SERVO POSITIONS:                                              │
│   0°   = All Clear (flag down)                                  │
│   90°  = Caution (flag mid-position)                            │
│   180° = Critical/Evacuate (flag fully raised)                  │
└─────────────────────────────────────────────────────────────────┘
```

**⚠️ IMPORTANT:** Servos can draw 500mA+ under load. Use a separate 5V supply if possible, or power from Arduino's 5V pin (not ESP32's 3.3V!).

**Steps:**
1. Connect servo Brown wire to GND
2. Connect servo Red wire to 5V
3. Connect servo Orange wire to ESP32 GPIO13

---

### A6 — 1-CHANNEL RELAY MODULE (SIREN CONTROL)

```
┌─────────────────────────────────────────────────────────────────┐
│                    RELAY MODULE WIRING                          │
│                                                                 │
│   Relay Module                   ESP32                          │
│   ┌──────────────────┐                                          │
│   │  VCC   IN   GND  │                                          │
│   │   │    │     │   │                                          │
│   │   │    │     └───┼────► GND                                 │
│   │   │    └─────────┼────► GPIO12                              │
│   │   └──────────────┼────► 5V                                  │
│   │                  │                                          │
│   │  COM   NO   NC   │                                          │
│   │   │    │     │   │                                          │
│   │   └────┘     │   │                                          │
│   │      │       │   │                                          │
│   │      └───┬───┘   │                                          │
│   │          │       │                                          │
│   └──────────┼───────┘                                          │
│              │                                                  │
│         External Siren/Lamp                                     │
│         (connect to COM + NO)                                   │
│                                                                 │
│   RELAY OPERATION:                                              │
│   IN = LOW  → Relay ON  → Siren ACTIVE                          │
│   IN = HIGH → Relay OFF → Siren OFF                             │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Relay VCC to 5V
2. Connect Relay GND to GND
3. Connect Relay IN to ESP32 GPIO12
4. Connect external siren/lamp to COM and NO terminals

---

### A7 — MAX7219 8x8 DOT MATRIX (SPI)

```
┌─────────────────────────────────────────────────────────────────┐
│                  DOT MATRIX WIRING (SPI)                        │
│                                                                 │
│   MAX7219 Module               ESP32                            │
│   ┌──────────────────┐                                          │
│   │  VCC   GND  DIN  │                                          │
│   │   │    │     │   │                                          │
│   │   │    │     └───┼────► GPIO23 (MOSI)                       │
│   │   │    │         │                                          │
│   │   │    └─────────┼────► GND                                 │
│   │   └──────────────┼────► 5V                                  │
│   │                  │                                          │
│   │  CLK   CS        │                                          │
│   │   │     │        │                                          │
│   │   └─────┼────────┼────► GPIO15 (CS)                         │
│   │         │        │                                          │
│   └─────────┼────────┘                                          │
│             │                                                   │
│             └─────────────► GPIO26 (CLK)                        │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Matrix VCC to 5V
2. Connect Matrix GND to GND
3. Connect Matrix DIN to ESP32 GPIO23
4. Connect Matrix CLK to ESP32 GPIO26
5. Connect Matrix CS to ESP32 GPIO15

---

### A8 — TTP223B TOUCH SENSOR (ALERT ACKNOWLEDGE)

```
┌─────────────────────────────────────────────────────────────────┐
│                    TOUCH SENSOR WIRING                          │
│                                                                 │
│   TTP223B                      ESP32                            │
│   ┌──────────────────┐                                          │
│   │  VCC   I/O  GND  │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► GND                                   │
│   │   │     └────────┼──► GPIO14                                │
│   │   └──────────────┼──► 3.3V                                  │
│   │                  │                                          │
│   │  [Touch Pad]     │                                          │
│   │   Touch to acknowledge alerts                               │
│   └──────────────────┘                                          │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Touch VCC to 3.3V
2. Connect Touch GND to GND
3. Connect Touch I/O to ESP32 GPIO14

---

### A9 — KY-023 JOYSTICK MODULE (MENU NAVIGATION)

```
┌─────────────────────────────────────────────────────────────────┐
│                    JOYSTICK WIRING                              │
│                                                                 │
│   KY-023 Joystick              ESP32                            │
│   ┌──────────────────┐                                          │
│   │  VCC   GND  VRx  │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► GPIO34 (ADC1)                         │
│   │   │     │        │                                          │
│   │   │     └────────┼──► GND                                   │
│   │   └──────────────┼──► 3.3V                                  │
│   │                  │                                          │
│   │  VRy   SW        │                                          │
│   │   │     │        │                                          │
│   │   └─────┼────────┼──► GPIO35 (ADC1)                         │
│   │         │        │                                          │
│   └─────────┼────────┘                                          │
│             │                                                   │
│             └─────────────► GPIO32 + 10kΩ pull-up to 3.3V       │
│                                                                 │
│   JOYSTICK FUNCTIONS:                                           │
│   X-axis (left/right): Cycle OLED menu pages                    │
│   Y-axis (up/down):    Not used (reserved)                      │
│   Push button:         Select/confirm                            │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Joystick VCC to 3.3V
2. Connect Joystick GND to GND
3. Connect Joystick VRx to ESP32 GPIO34
4. Connect Joystick VRy to ESP32 GPIO35
5. Connect Joystick SW to ESP32 GPIO32
6. Add 10kΩ pull-up resistor from GPIO32 to 3.3V

---

### A10 — SOIL MOISTURE SENSOR (CAPACITIVE)

```
┌─────────────────────────────────────────────────────────────────┐
│                 SOIL MOISTURE WIRING                            │
│                                                                 │
│   Capacitive Soil Sensor         ESP32                          │
│   ┌──────────────────┐                                          │
│   │  VCC   GND  AOUT │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► GPIO36 (ADC1)                         │
│   │   │     └────────┼──► GND                                   │
│   │   └──────────────┼──► 3.3V  (NOT 5V!)                       │
│   │                  │                                          │
│   │  [Probe]         │                                          │
│   │   Insert into soil                                          │
│   └──────────────────┘                                          │
│                                                                 │
│   ⚠️ WARNING: Use 3.3V NOT 5V! ESP32 ADC is 3.3V max.           │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Sensor VCC to ESP32 3.3V (NOT 5V!)
2. Connect Sensor GND to GND
3. Connect Sensor AOUT to ESP32 GPIO36

---

### A11 — IR OBSTACLE AVOIDANCE SENSOR (DEBRIS DETECT)

```
┌─────────────────────────────────────────────────────────────────┐
│              OBSTACLE SENSOR WIRING                             │
│                                                                 │
│   IR Obstacle Module             ESP32                          │
│   ┌──────────────────┐                                          │
│   │  VCC   OUT  GND  │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► GND                                   │
│   │   │     └────────┼──► GPIO39 (ADC1)                         │
│   │   └──────────────┼──► 3.3V                                  │
│   │                  │                                          │
│   │  [IR LED + PD]   │                                          │
│   │   Detects debris/obstacles                                  │
│   └──────────────────┘                                          │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Obstacle VCC to 3.3V
2. Connect Obstacle GND to GND
3. Connect Obstacle OUT to ESP32 GPIO39

---

## PART B — ARDUINO UNO SLAVE WIRING

### Pin Reference Table (v4.0)

| Pin | Function | Type | Notes |
|-----|----------|------|-------|
| A1 | Water Level | ADC | 0-700 → 0-100% |
| A2 | Thermistor | ADC | 10k NTC + 10k series |
| A3 | Photoresistor | ADC | 10k LDR + 10k series (NEW v4.0) |
| A4 | Potentiometer | ADC | Calibration 0.5x-2.0x |
| A5 | 7-Segment F | Output | Via 220Ω |
| D2 | Tilt Switch | Interrupt | FALLING edge, INT0 |
| D3 | Active Buzzer | Output | DC-driven |
| D4 | 7-Segment A | Output | Via 220Ω |
| D5 | 7-Segment B | Output | Via 220Ω |
| D6 | 7-Segment C | Output | Via 220Ω |
| D11 | Passive Buzzer | PWM | tone() frequency |
| D12 | 7-Segment D | Output | Via 220Ω |
| D13 | 7-Segment E | Output | Via 220Ω |
| D0 | Serial RX | UART | From ESP32 TX |
| D1 | Serial TX | UART | To ESP32 RX (via divider) |

⚠️ **NOTE:** D0/D1 are shared with USB programming. Disconnect ESP32 serial wires before uploading firmware.

---

### B1 — WATER LEVEL SENSOR

```
┌─────────────────────────────────────────────────────────────────┐
│               WATER LEVEL SENSOR WIRING                         │
│                                                                 │
│   Water Level Sensor             Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  VCC   GND  SIG  │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► A1 (Analog In 1)                      │
│   │   │     └────────┼──► GND                                   │
│   │   └──────────────┼──► 5V                                    │
│   │                  │                                          │
│   │  [Probe]         │                                          │
│   │   Immerse in water                                          │
│   └──────────────────┘                                          │
│                                                                 │
│   OUTPUT: 0-700 (analog) mapped to 0-100%                       │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Sensor VCC to 5V
2. Connect Sensor GND to GND
3. Connect Sensor SIG to Arduino A1

---

### B2 — THERMISTOR 10K NTC (SOIL TEMPERATURE)

```
┌─────────────────────────────────────────────────────────────────┐
│                THERMISTOR WIRING                                │
│                                                                 │
│   Thermistor 10k NTC + 10k Resistor    Arduino Uno              │
│   ┌──────────────────────────┐                                  │
│   │                          │                                  │
│   │   5V ──┬──[10k NTC]──┬──┼──► A2                            │
│   │        │             │  │                                  │
│   │        │          ┌──┴──┘                                  │
│   │        │          │ 10kΩ │ (series pull-up)                │
│   │        │          └──┬──┘                                  │
│   │        │             │                                     │
│   │        └─────────────┴──► GND                              │
│   │                          │                                  │
│   └──────────────────────────┘                                  │
│                                                                 │
│   FORMULA: Steinhart-Hart B-parameter                           │
│   1/T = 1/T0 + (1/B) × ln(R/R0)                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect thermistor one leg to 5V
2. Connect thermistor other leg to Arduino A2
3. Connect 10kΩ resistor from A2 to GND (voltage divider)

---

### B3 — PHOTORESISTOR (LIGHT INTENSITY) — NEW v4.0

```
┌─────────────────────────────────────────────────────────────────┐
│              PHOTORESISTOR WIRING                               │
│                                                                 │
│   LDR + 10k Resistor                 Arduino Uno                │
│   ┌──────────────────────────┐                                  │
│   │                          │                                  │
│   │   5V ──┬──[LDR]──────┬──┼──► A3                            │
│   │        │             │  │                                  │
│   │        │          ┌──┴──┘                                  │
│   │        │          │ 10kΩ │ (series pull-down)              │
│   │        │          └──┬──┘                                  │
│   │        │             │                                     │
│   │        └─────────────┴──► GND                              │
│   │                          │                                  │
│   └──────────────────────────┘                                  │
│                                                                 │
│   OUTPUT: 0-1023 mapped to 0-100% (transmitted as L: field)     │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect LDR one leg to 5V
2. Connect LDR other leg to Arduino A3
3. Connect 10kΩ resistor from A3 to GND (voltage divider)

---

### B4 — POTENTIOMETER (SENSITIVITY CALIBRATION)

```
┌─────────────────────────────────────────────────────────────────┐
│             POTENTIOMETER WIRING                                │
│                                                                 │
│   10k Potentiometer                Arduino Uno                  │
│   ┌──────────────────────────┐                                  │
│   │    ┌───────┐             │                                  │
│   │    │       │             │                                  │
│   │  ──┤ 10kΩ  ├───          │                                  │
│   │    │       │             │                                  │
│   │    └───┬───┘             │                                  │
│   │        │ (wiper)         │                                  │
│   │        └─────────────────┼──► A4                           │
│   │                          │                                  │
│   │   Left leg ──────────────┼──► 5V                           │
│   │   Right leg ─────────────┼──► GND                          │
│   └──────────────────────────┘                                  │
│                                                                 │
│   OUTPUT: 0-1023 mapped to 0.50x - 2.00x calibration            │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect pot left leg to 5V
2. Connect pot right leg to GND
3. Connect pot center (wiper) to Arduino A4

---

### B5 — TILT SWITCH (LANDSLIDE DETECTION)

```
┌─────────────────────────────────────────────────────────────────┐
│                TILT SWITCH WIRING                               │
│                                                                 │
│   SW-520D Tilt Switch              Arduino Uno                  │
│   ┌──────────────────┐                                          │
│   │   ┌─────────┐    │                                          │
│   │   │  TUBE   │    │                                          │
│   │   │  [oo]   │    │                                          │
│   │   └─────────┘    │                                          │
│   │      │   │       │                                          │
│   │      │   └───────┼──► GND                                   │
│   │      └───────────┼──► D2 (INT0)                            │
│   │                  │                                          │
│   │   [Internal]     │                                          │
│   │   Ball bearing   │                                          │
│   │   closes circuit │                                          │
│   └──────────────────┘                                          │
│                                                                 │
│   MODE: INPUT_PULLUP (internal pull-up enabled)                 │
│   TRIGGER: FALLING edge (tilt opens circuit)                    │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect tilt switch one leg to Arduino D2
2. Connect tilt switch other leg to GND
3. In code: `pinMode(D2, INPUT_PULLUP)`

---

### B6 — ACTIVE BUZZER (DC-DRIVEN)

```
┌─────────────────────────────────────────────────────────────────┐
│                ACTIVE BUZZER WIRING                             │
│                                                                 │
│   Active Buzzer 5V                 Arduino Uno                  │
│   ┌──────────────────┐                                          │
│   │   +       -      │                                          │
│   │   │       │      │                                          │
│   │   │       └──────┼──► GND                                   │
│   │   └──────────────┼──► D3                                    │
│   │                  │                                          │
│   │   [Internal]     │                                          │
│   │   Oscillator     │                                          │
│   │   DC HIGH = BEEP │                                          │
│   └──────────────────┘                                          │
│                                                                 │
│   OPERATION: digitalWrite(D3, HIGH) = ON                        │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect active buzzer + to Arduino D3
2. Connect active buzzer - to GND

---

### B7 — PASSIVE BUZZER (FREQUENCY CONTROL)

```
┌─────────────────────────────────────────────────────────────────┐
│               PASSIVE BUZZER WIRING                             │
│                                                                 │
│   Passive Buzzer 5V                Arduino Uno                  │
│   ┌──────────────────┐                                          │
│   │   +       -      │                                          │
│   │   │       │      │                                          │
│   │   │       └──────┼──► GND                                   │
│   │   └──────────────┼──► D11 (PWM)                            │
│   │                  │                                          │
│   │   [No Internal]  │                                          │
│   │   Needs tone()   │                                          │
│   │   freq = pitch   │                                          │
│   └──────────────────┘                                          │
│                                                                 │
│   OPERATION: tone(D11, frequency, duration)                     │
│   EXAMPLES:                                                     │
│   - Boot chirp: 880Hz, 1100Hz, 1400Hz                           │
│   - Tilt alarm: 2500Hz continuous                               │
│   - Critical: alternating 1800/2000Hz                           │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect passive buzzer + to Arduino D11
2. Connect passive buzzer - to GND

---

### B8 — 7-SEGMENT DISPLAY (COMMON CATHODE)

```
┌─────────────────────────────────────────────────────────────────┐
│              7-SEGMENT DISPLAY WIRING                           │
│                                                                 │
│   Common Cathode 7-Segment           Arduino Uno                │
│   ┌──────────────────────────┐                                  │
│   │       ┌─────┐            │                                  │
│   │     ──┤ A B  ├───         │                                  │
│   │    │  │  F  │   │         │                                  │
│   │    │  │  .G │   │         │                                  │
│   │    │  │ E D │   │         │                                  │
│   │     ──┤ C   ├───          │                                  │
│   │       └─────┘            │                                  │
│   │                          │                                  │
│   │  A ──┬──220Ω──┬──► D4    │                                  │
│   │  B ──┼──220Ω──┼──► D5    │                                  │
│   │  C ──┼──220Ω──┼──► D6    │                                  │
│   │  D ──┼──220Ω──┼──► D12   │                                  │
│   │  E ──┼──220Ω──┼──► D13   │                                  │
│   │  F ──┼──220Ω──┼──► A5    │                                  │
│   │  G ──┴──────────────► GND (always LOW)                      │
│   │  COM ───────────────► GND (common cathode)                  │
│   └──────────────────────────┘                                  │
│                                                                 │
│   SEGMENT PATTERN (common cathode, A-F only, G=LOW):            │
│   0 = 0b111111, 1 = 0b000110, 2 = 0b110011, etc.               │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect each segment pin (A-F) through 220Ω resistor to Arduino
2. Connect segment G directly to GND (always off)
3. Connect common cathode pins to GND
4. Pin mapping: A→D4, B→D5, C→D6, D→D12, E→D13, F→A5

---

## PART C — INTER-BOARD COMMUNICATION

### Serial2 Connection (ESP32 ↔ Arduino)

```
┌─────────────────────────────────────────────────────────────────┐
│              SERIAL2 WIRING (with voltage divider)              │
│                                                                 │
│   ESP32                    Arduino Uno                          │
│   ┌─────────┐             ┌───────────┐                        │
│   │GPIO17   │──TX2────────┤D0 (RX)    │                         │
│   │         │             │           │                        │
│   │GPIO16   │──RX2────┬───┤D1 (TX)    │                         │
│   │         │        │   │           │                        │
│   │         │     ┌──┴──┐│           │                        │
│   │         │     │ 1kΩ ││           │                        │
│   │         │     └──┬──┘│           │                        │
│   │         │        ├────           │                        │
│   │         │     ┌──┴──┐│           │                        │
│   │         │     │ 2kΩ ││           │                        │
│   │         │     └──┬──┘│           │                        │
│   │         │        │   │           │                        │
│   │GND      │────────┴───┤GND        │                         │
│   └─────────┘             └───────────┘                        │
│                                                                 │
│   BAUD RATE: 9600                                               │
│   PROTOCOL: 8N1 (8 data, no parity, 1 stop)                     │
│                                                                 │
│   ARDUINO TX FORMAT: W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x,L:xx.x    │
│   ESP32 CMD FORMAT:  CMD:alert_level                           │
└─────────────────────────────────────────────────────────────────┘
```

**⚠️ CRITICAL:** Arduino TX (D1) is 5V logic. ESP32 RX (GPIO16) is 3.3V max!
**You MUST use a voltage divider or you will damage the ESP32.**

**Steps:**
1. Connect ESP32 GPIO17 (TX2) to Arduino D0 (RX)
2. Connect Arduino D1 (TX) to 1kΩ resistor
3. Connect other end of 1kΩ to ESP32 GPIO16 (RX2)
4. Connect 2kΩ resistor from GPIO16 junction to GND
5. Connect ESP32 GND to Arduino GND (common ground)

**⚠️ UPLOAD NOTE:** D0/D1 are shared with USB programming. **Disconnect ESP32 serial wires before uploading Arduino firmware.**

---

## PART D — POWER DISTRIBUTION

```
┌─────────────────────────────────────────────────────────────────┐
│                    POWER DISTRIBUTION                           │
│                                                                 │
│   USB 5V 2A                 ESP32            Arduino            │
│   ┌─────────┐              ┌──────┐         ┌──────┐           │
│   │  Power  │──5V─────────►│ 5V   │         │ 5V   │           │
│   │  Supply │              │      │         │      │           │
│   │         │──GND────────►│ GND  │────────►│ GND  │           │
│   └─────────┘              └──────┘         └──────┘           │
│                              │                │                 │
│                              │                │                 │
│                       ┌──────┴────────────────┴──────┐         │
│                       │  Common Ground Bus           │         │
│                       │  (all GNDs connected)        │         │
│                       └──────────────────────────────┘         │
│                                                                 │
│   COMPONENT POWER:                                              │
│   - ESP32 sensors (DHT11, HC-SR04, PIR): 3.3V or 5V            │
│   - Arduino sensors (Water, Thermistor, LDR, Pot): 5V          │
│   - Servo: 5V (separate supply recommended for stability)      │
│   - Relay: 5V                                                   │
│   - Buzzers: 5V                                                 │
│   - LEDs: Via 220Ω from GPIO pins                              │
│   - Displays: OLED 3.3V, LCD 5V, 7-Seg from GPIO               │
└─────────────────────────────────────────────────────────────────┘
```

**Power Recommendations:**
1. Use USB 5V 2A supply or powered USB hub
2. Servo may cause brownouts — consider separate 5V 1A supply
3. All grounds MUST be connected together (common ground)

---

## PHYSICAL STATION LAYOUT

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         PHYSICAL ARRANGEMENT                            │
│                                                                         │
│   ┌───────────────────────────────────────────────────────────┐        │
│   │                    BACKBOARD (foam/core)                  │        │
│   │                                                           │        │
│   │   ┌─────────┐                           ┌─────────┐      │        │
│   │   │ ESP32   │                           │ Arduino │      │        │
│   │   │ Master  │◄────── Serial2 ──────────►│  Slave  │      │        │
│   │   │ Board   │                           │  Board  │      │        │
│   │   └────┬────┘                           └────┬────┘      │        │
│   │        │                                     │           │        │
│   │   ┌────┴────────────────────────────────────┬┘           │        │
│   │   │                                         │            │        │
│   │   │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────────┐   │            │        │
│   │   │  │OLED │ │LCD  │ │Dot  │ │ 7-Seg   │   │            │        │
│   │   │  │128x64│ │1602 │ │Matrix│ │Display  │   │            │        │
│   │   │  └─────┘ └─────┘ └─────┘ └─────────┘   │            │        │
│   │   │                                         │            │        │
│   │   │  ┌─────────────────────────────────┐   │            │        │
│   │   │  │      SENSOR ARRAY               │   │            │        │
│   │   │  │  DHT11  HC-SR04  PIR  Obstacle  │   │            │        │
│   │   │  └─────────────────────────────────┘   │            │        │
│   │   │                                         │            │        │
│   │   │  ┌──────────┐  ┌──────────────────┐    │            │        │
│   │   │  │  Servo   │  │  Water/Soil/LDR  │    │            │        │
│   │   │  │  (Flag)  │  │  Probes in tray  │    │            │        │
│   │   │  └──────────┘  └──────────────────┘    │            │        │
│   │   │                                         │            │        │
│   │   └─────────────────────────────────────────┘            │        │
│   │                                                           │        │
│   └───────────────────────────────────────────────────────────┘        │
│                                                                         │
│   FRONT VIEW:                                                           │
│   - Tilt switch mounted on movable cardboard wedge                      │
│   - Water/soil sensors in shallow tray                                  │
│   - Joystick + touch sensor for user interaction                        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## STEP-BY-STEP ASSEMBLY GUIDE

### Phase 1: Board Preparation (30 min)
1. Mount ESP32 and Arduino on separate breadboards
2. Add power rails (5V and GND) to both breadboards
3. Install voltage divider resistors for HC-SR04 and Serial2

### Phase 2: ESP32 Sensor Array (45 min)
1. Wire I2C bus (OLED + LCD)
2. Connect DHT11 with 10k pull-up
3. Install HC-SR04 with voltage divider
4. Wire PIR, soil moisture, obstacle sensors
5. Connect servo, relay, dot matrix
6. Wire status LEDs (4x) with 220Ω resistors
7. Install joystick and touch sensor

### Phase 3: Arduino Sensor Array (30 min)
1. Wire water level sensor to A1
2. Install thermistor with 10k series to A2
3. Wire photoresistor with 10k series to A3
4. Connect potentiometer to A4
5. Mount tilt switch to D2
6. Wire active buzzer to D3
7. Wire passive buzzer to D11
8. Connect 7-segment with 220Ω resistors

### Phase 4: Inter-Board Connection (15 min)
1. Connect ESP32 GPIO17 to Arduino D0
2. Connect Arduino D1 through voltage divider to ESP32 GPIO16
3. Connect common ground between boards

### Phase 5: Testing & Verification (30 min)
1. Power on both boards via USB
2. Verify ESP32 boot (LED chase, servo sweep)
3. Verify Arduino boot (7-segment 0-9 count, dual chirp)
4. Check Serial2 communication (monitor Serial output)
5. Test each sensor individually
6. Verify WiFi dashboard at http://192.168.4.1

---

## TESTING & VERIFICATION

### ESP32 Master Tests
```cpp
// Run I2C Scanner to verify OLED (0x3C) and LCD (0x27)
// Test DHT11: Should read 20-30°C, 40-60% RH
// Test HC-SR04: Should read 5-200cm distance
// Test PIR: Wave hand, check GPIO19 HIGH
// Test Soil: Dry = high value, Wet = low value
```

### Arduino Slave Tests
```cpp
// Test Water: 0-700 analog → 0-100%
// Test Thermistor: Should read 20-30°C
// Test LDR: Dark = 0%, Bright = 100%
// Test Tilt: Trigger interrupt, check D2 FALLING
// Test Buzzers: Active (DC), Passive (tone)
// Test 7-Segment: Should display 0-9 on boot
```

### System Integration Tests
1. **Flood Scenario:** Pour water → Blue LED, servo 90°
2. **Landslide Scenario:** Tilt board → Red LED, 2.5kHz alarm, servo 180°
3. **Drought Scenario:** Dry soil → Yellow LED, OLED page 2
4. **Alert Acknowledge:** Touch sensor → Buzzer silence, 60s grace

---

## TROUBLESHOOTING

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
| Arduino resets randomly | Watchdog timeout | Check loop blocking code; ensure wdt_reset() |
| Slope% stuck at 0 | Stale tilt timestamp | Verify v4.0: lastTiltEventMs is volatile |
| 7-segment wrong digit | Segment pin mismatch | Verify A-F mapping to D4,D5,D6,D12,D13,A5 |

---

## SAFETY WARNINGS

⚠️ **VOLTAGE DIVIDERS ARE MANDATORY**
- HC-SR04 Echo → ESP32: 1kΩ + 2kΩ
- Arduino TX → ESP32 RX: 1kΩ + 2kΩ
- 5V signals WILL damage ESP32 GPIO (3.3V max)

⚠️ **COMMON GROUND REQUIRED**
- All GNDs must be connected together
- ESP32, Arduino, sensors, servo, relay
- Floating ground causes erratic behavior

⚠️ **SERVO POWER SEPARATION**
- Servos draw 500mA+ under load
- Use separate 5V 1A supply if brownouts occur
- Add 100µF capacitor across servo power rails

⚠️ **UPLOAD PROCEDURE**
- Disconnect ESP32 serial wires before Arduino upload
- D0/D1 are shared with USB programming
- Alternatively, use SoftwareSerial on D8/D9

---

*GEO-SENSE AFRICA v4.0 | Complete Wiring Guide | March 2026*
