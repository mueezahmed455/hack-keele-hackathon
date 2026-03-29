# GEO-SENSE AFRICA v2.0 — COMPLETE WIRING GUIDE
## ESP32 Master + Arduino Uno Slave | Dual-Board Multi-Hazard Early Warning System

**Version:** 2.0 (Updated with corrected pin assignments)  
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
│                         GEO-SENSE AFRICA v2.0                               │
│                 Dual-Board Multi-Hazard Early Warning System                │
│                                                                             │
│   ┌─────────────────────────┐         ┌─────────────────────────┐          │
│   │    ESP32 MASTER NODE    │         │   ARDUINO UNO SLAVE     │          │
│   │  ┌───────────────────┐  │         │  ┌───────────────────┐  │          │
│   │  │ WiFi Web Server   │  │         │  │ Water Level       │  │          │
│   │  │ OLED 128x64 I2C   │  │         │  │ Thermistor        │  │          │
│   │  │ LCD 1602 I2C      │  │         │  │ Tilt Switch (INT) │  │          │
│   │  │ Dot Matrix 8x8    │  │         │  │ 7-Segment Display │  │          │
│   │  │ DHT11 (Air T/H)   │  │         │  │ Active Buzzer     │  │          │
│   │  │ HC-SR04 (Flood)   │  │         │  │ Passive Buzzer    │  │          │
│   │  │ PIR Motion        │  │         │  │ Photoresistor     │  │          │
│   │  │ Soil Moisture     │  │         │  │ Potentiometer     │  │          │
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
│                    RX2 (GPIO16) ◄──► TX (D1)                                │
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
| 7-Segment Display (common cathode) | 1 | N/A | Single digit, 7 pins |
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

### Sensors - Arduino Side
| Item | Quantity | Notes |
|------|----------|-------|
| Water Level Sensor | 1 | Analog output |
| Thermistor 10k NTC | 1 | With 10k resistor |
| Photoresistor (LDR) | 1 | With 10k resistor |
| Potentiometer 10k | 1 | For sensitivity calibration |
| Tilt Switch SW-520D | 1 | Digital (open/closed) |

### Outputs
| Item | Quantity | Notes |
|------|----------|-------|
| SG90 Micro Servo | 1 | 3-pin (brown=GND, red=5V, orange=SIG) |
| 1-Channel Relay Module | 1 | Active LOW trigger |
| Active Buzzer 5V | 1 | 2-pin (polarized) |
| Passive Buzzer 5V | 1 | 2-pin (non-polarized) |
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
| GPIO26 | Dot Matrix CLK | SPI SCK | **Updated from GPIO18** |
| GPIO15 | Dot Matrix CS | SPI SS | |
| GPIO25 | Green LED | Output | Via 220Ω |
| GPIO2 | Blue LED | Output | Via 220Ω (**Updated from GPIO26**) |
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
│             └─────────────► GPIO26 (CLK)  [UPDATED PIN]         │
│                                                                 │
│   NOTE: CLK moved from GPIO18 to GPIO26 to avoid HC-SR04 conflict│
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
│   Push button:         Select/confirm (not used in v2.0)        │
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
│   │  [IR LED + Receiver]                                        │
│   │   Detects floating debris                                   │
│   └──────────────────┘                                          │
│                                                                 │
│   OUTPUT: LOW when obstacle detected                            │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Obstacle VCC to 3.3V
2. Connect Obstacle GND to GND
3. Connect Obstacle OUT to ESP32 GPIO39

---

### A12 — STATUS LEDs (4 HAZARD CHANNELS)

```
┌─────────────────────────────────────────────────────────────────┐
│                    LED ARRAY WIRING                             │
│                                                                 │
│   ESP32              220Ω Resistors         LEDs                │
│   ┌─────┐           ┌──────────┐          ┌────────┐           │
│   │GPIO25├──────────┤ 220Ω     ├──────────┤ GREEN  │ GND       │
│   │GPIO2 ├──────────┤ 220Ω     ├──────────┤ BLUE   │ GND       │
│   │GPIO27├──────────┤ 220Ω     ├──────────┤ YELLOW │ GND       │
│   │GPIO33├──────────┤ 220Ω     ├──────────┤ RED    │ GND       │
│   └─────┘           └──────────┘          └────────┘           │
│                                                                 │
│   LED INDICATIONS:                                              │
│   GREEN  = All Clear (Risk 0-3)                                 │
│   YELLOW = Drought Warning (Risk 4-6)                           │
│   RED    = Critical Alert (Risk 7-9)                            │
│   BLUE   = Flood Detected (hazard type indicator)               │
│                                                                 │
│   LED Polarity: Long leg = Anode (+), Short leg = Cathode (-)   │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect ESP32 GPIO25 to 220Ω resistor, then to GREEN LED anode
2. Connect ESP32 GPIO2 to 220Ω resistor, then to BLUE LED anode
3. Connect ESP32 GPIO27 to 220Ω resistor, then to YELLOW LED anode
4. Connect ESP32 GPIO33 to 220Ω resistor, then to RED LED anode
5. Connect all LED cathodes (short legs) to GND

---

## PART B — ARDUINO UNO SLAVE WIRING

### Pin Reference Table (Quick Lookup)

| Pin | Function | Type | Notes |
|-----|----------|------|-------|
| A1 | Water Level | ADC | Analog sensor |
| A2 | Thermistor | ADC | Voltage divider |
| A3 | Photoresistor | ADC | Voltage divider |
| A4 | Potentiometer | ADC | Calibration |
| A5 | 7-Seg Segment F | Output | Via 220Ω |
| D2 | Tilt Switch | Interrupt | FALLING edge |
| D3 | Active Buzzer | Output | |
| D4 | 7-Seg Segment A | Output | Via 220Ω |
| D5 | 7-Seg Segment B | Output | Via 220Ω |
| D6 | 7-Seg Segment C | Output | Via 220Ω |
| D11 | Passive Buzzer | PWM | |
| D12 | 7-Seg Segment D | Output | Via 220Ω |
| D13 | 7-Seg Segment E | Output | Via 220Ω |
| D0 | Serial TX | Output | To ESP32 (via divider) |
| D1 | Serial RX | Input | From ESP32 |

---

### B1 — WATER LEVEL SENSOR

```
┌─────────────────────────────────────────────────────────────────┐
│                 WATER LEVEL SENSOR WIRING                       │
│                                                                 │
│   Water Level Sensor             Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  VCC   GND  SIG  │                                          │
│   │   │     │     │  │                                          │
│   │   │     │     └──┼──► A1                                    │
│   │   │     └────────┼──► GND                                   │
│   │   └──────────────┼──► 5V                                    │
│   │                  │                                          │
│   │  [Probe]         │                                          │
│   │   Immerse in water tray                                     │
│   └──────────────────┘                                          │
│                                                                 │
│   OUTPUT: 0-700 (dry to submerged)                              │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Water Sensor VCC to 5V
2. Connect Water Sensor GND to GND
3. Connect Water Sensor SIG to Arduino A1

---

### B2 — THERMISTOR (SOIL TEMPERATURE)

```
┌─────────────────────────────────────────────────────────────────┐
│                   THERMISTOR WIRING                             │
│                                                                 │
│   Thermistor 10k NTC             Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │     ┌───┐        │                                          │
│   │     │ ──┼────────┼──► 5V                                    │
│   │     │   │        │                                          │
│   │     │   ├────────┼──► A2                                    │
│   │     │   │        │                                          │
│   │     └───┼────────┼──► GND                                   │
│   │         │        │                                          │
│   │       10kΩ       │                                          │
│   │         │        │                                          │
│   └─────────┴────────┘                                          │
│                                                                 │
│   STEINHART-HART EQUATION (in code):                            │
│   T = 1/(1/T0 + 1/B × ln(R/R0)) - 273.15                        │
│   Where: T0=25°C, B=3950, R0=10kΩ                               │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect thermistor leg 1 to 5V
2. Connect thermistor leg 2 to Arduino A2
3. Connect 10kΩ resistor from A2 to GND

---

### B3 — PHOTORESISTOR (AMBIENT LIGHT)

```
┌─────────────────────────────────────────────────────────────────┐
│                  PHOTORESISTOR WIRING                           │
│                                                                 │
│   Photoresistor (LDR)            Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │     ┌───┐        │                                          │
│   │     │ ──┼────────┼──► 5V                                    │
│   │     │   │        │                                          │
│   │     │   ├────────┼──► A3                                    │
│   │     │   │        │                                          │
│   │     └───┼────────┼──► GND                                   │
│   │         │        │                                          │
│   │       10kΩ       │                                          │
│   │         │        │                                          │
│   └─────────┴────────┘                                          │
│                                                                 │
│   OUTPUT: Higher value = brighter light                         │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect photoresistor leg 1 to 5V
2. Connect photoresistor leg 2 to Arduino A3
3. Connect 10kΩ resistor from A3 to GND

---

### B4 — POTENTIOMETER (SENSITIVITY CALIBRATION)

```
┌─────────────────────────────────────────────────────────────────┐
│                POTENTIOMETER WIRING                             │
│                                                                 │
│   10k Potentiometer              Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  ○───────○───────○  │                                       │
│   │  1       2       3  │                                       │
│   │  │       │       │  │                                       │
│   │  │       │       │  │                                       │
│   │  │       └───────┼──► A4                                    │
│   │  │               │                                          │
│   │  └───────────────┼──► GND                                   │
│   │                  │                                          │
│   └──────────────────┼──► 5V                                    │
│                                                                 │
│   CALIBRATION RANGE: 0.5x to 2.0x sensitivity                   │
│   Turn clockwise to increase sensitivity                        │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Pot pin 1 (left) to GND
2. Connect Pot pin 2 (center/wiper) to Arduino A4
3. Connect Pot pin 3 (right) to 5V

---

### B5 — TILT SWITCH (LANDSLIDE DETECTOR)

```
┌─────────────────────────────────────────────────────────────────┐
│                   TILT SWITCH WIRING                            │
│                                                                 │
│   SW-520D Tilt Switch            Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  ┌─────────┐     │                                          │
│   │  │  ○ ○    │     │                                          │
│   │  │  │ │    │     │                                          │
│   │  │  │ └────┼─────┼──► GND                                   │
│   │  │  │      │     │                                          │
│   │  │  └──────┼─────┼──► D2 (INT0)                             │
│   │  │         │     │                                          │
│   │  └─────────┘     │                                          │
│   │                  │                                          │
│   │  [Mount at 15° angle]                                       │
│   │   Triggers on slope change                                  │
│   └──────────────────┘                                          │
│                                                                 │
│   INTERRUPT: FALLING edge (HIGH→LOW when tilted)                │
│   Uses internal INPUT_PULLUP (no external resistor needed)      │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Tilt Switch pin 1 to Arduino D2
2. Connect Tilt Switch pin 2 to GND
3. Mount on cardboard wedge at 15° angle

---

### B6 — ACTIVE BUZZER (ALARM)

```
┌─────────────────────────────────────────────────────────────────┐
│                   ACTIVE BUZZER WIRING                          │
│                                                                 │
│   Active Buzzer 5V               Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  +       -       │                                          │
│   │  │       │       │                                          │
│   │  │       └───────┼──► GND                                   │
│   │  │               │                                          │
│   │  └───────────────┼──► D3                                    │
│   │                  │                                          │
│   │  [Long leg = +]  │                                          │
│   │   Continuous tone when HIGH                                  │
│   └──────────────────┘                                          │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Buzzer + (long leg) to Arduino D3
2. Connect Buzzer - (short leg) to GND

---

### B7 — PASSIVE BUZZER (TONE GENERATOR)

```
┌─────────────────────────────────────────────────────────────────┐
│                  PASSIVE BUZZER WIRING                          │
│                                                                 │
│   Passive Buzzer 5V              Arduino Uno                    │
│   ┌──────────────────┐                                          │
│   │  +       -       │                                          │
│   │  │       │       │                                          │
│   │  │       └───────┼──► GND                                   │
│   │  │               │                                          │
│   │  └───────────────┼──► D11 (PWM)                             │
│   │                  │                                          │
│   │   Requires tone() function                                   │
│   │   Different frequencies for alert levels                    │
│   └──────────────────┘                                          │
│                                                                 │
│   TONE PATTERNS:                                                │
│   Tilt triggered: 2500 Hz continuous                            │
│   Critical:       1800 Hz beep every 400ms                      │
│   Caution:        880 Hz beep every 3s                          │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Connect Passive Buzzer + to Arduino D11
2. Connect Passive Buzzer - to GND

---

### B8 — 7-SEGMENT DISPLAY (WATER LEVEL INDICATOR)

```
┌─────────────────────────────────────────────────────────────────┐
│              7-SEGMENT DISPLAY WIRING                           │
│                                                                 │
│   Common Cathode 7-Segment       Arduino Uno                    │
│   (view from front)                                             │
│                                                                 │
│         ┌───┐                                                   │
│      f  │ a │  b                                                │
│         │───│                                                   │
│      e  │ g │  c     Segment  Arduino   Resistor                │
│         │───│       ────────────────────────────                │
│         │ d │         a ──────► D4 ──► 220Ω ──► pin a          │
│         └───┘           b ──────► D5 ──► 220Ω ──► pin b        │
│                           c ──────► D6 ──► 220Ω ──► pin c        │
│   Pin Layout (bottom):    d ──────► D12 ─► 220Ω ──► pin d        │
│   dp e d c b a g dp       e ──────► D13 ─► 220Ω ──► pin e        │
│     │ │ │ │ │ │ │         f ──────► A5 ──► 220Ω ──► pin f        │
│     └─┴─┴─┴─┴─┴─┴─┘       g ──────► GND (common cathode)        │
│                                                                 │
│   DISPLAY VALUE: Shows water level / 11.1 (0-9 scale)           │
│   Shows "9" when tilt alarm triggered                           │
└─────────────────────────────────────────────────────────────────┘
```

**Steps:**
1. Identify 7-segment pins (use multimeter to find segments)
2. Connect each segment (a-f) through 220Ω resistor to Arduino pins
3. Connect common cathode (GND) to Arduino GND
4. Segment g connects directly to GND (always on for digits 0-9)

**Segment to Pin Mapping:**
| Segment | Arduino Pin | Resistor |
|---------|-------------|----------|
| a | D4 | 220Ω |
| b | D5 | 220Ω |
| c | D6 | 220Ω |
| d | D12 | 220Ω |
| e | D13 | 220Ω |
| f | A5 | 220Ω |
| g | GND | Direct |
| DP | NC | Not connected |

---

## PART C — INTER-BOARD COMMUNICATION

### Serial Connection (ESP32 ↔ Arduino)

```
┌─────────────────────────────────────────────────────────────────┐
│           SERIAL COMMUNICATION WIRING                           │
│                                                                 │
│   Arduino Uno                    ESP32 Master                    │
│   ┌──────────────────┐          ┌──────────────────┐           │
│   │              D1  │──TX──────┤ GPIO16 (RX2)     │           │
│   │              D0  │──RX──────┤ GPIO17 (TX2)     │           │
│   │              GND │──────────┤ GND              │           │
│   └──────────────────┘          └──────────────────┘           │
│                                                                 │
│   ⚠️ VOLTAGE DIVIDER REQUIRED (Arduino TX → ESP32 RX):         │
│                                                                 │
│   Arduino D1 (TX) ──┬── 1kΩ ──┬──► ESP32 GPIO16                 │
│                     │         │                                 │
│                     │       2kΩ │                               │
│                     │         │                                 │
│                     └─────────┴──► GND                           │
│                                                                 │
│   BAUD RATE: 9600 bps                                           │
│   FORMAT: 8N1 (8 data, No parity, 1 stop)                       │
│                                                                 │
│   MESSAGE FORMAT (Arduino → ESP32):                             │
│   "W:xx.x,T:xx.x,S:xx.x,P:x.xx,I:x\\n"                          │
│   Where: W=Water, T=Temp, S=Slope, P=Pot, I=Interrupt           │
│                                                                 │
│   COMMAND FORMAT (ESP32 → Arduino):                             │
│   "CMD:xx\\n"  (xx = alert level 0/1/2)                         │
└─────────────────────────────────────────────────────────────────┘
```

**⚠️ CRITICAL:** Arduino TX outputs 5V logic, ESP32 RX is 3.3V max!
**Without the voltage divider, you WILL damage the ESP32!**

**Steps:**
1. Connect Arduino D1 (TX) to 1kΩ resistor
2. Connect other end of 1kΩ to ESP32 GPIO16
3. Connect 2kΩ resistor from GPIO16 junction to GND
4. Connect Arduino D0 (RX) to ESP32 GPIO17 (direct, 3.3V is safe)
5. Connect Arduino GND to ESP32 GND (COMMON GROUND REQUIRED!)

---

## PART D — POWER DISTRIBUTION

```
┌─────────────────────────────────────────────────────────────────┐
│                  POWER DISTRIBUTION NETWORK                     │
│                                                                 │
│   USB Power (5V 2A recommended)                                 │
│         │                                                       │
│    ┌────┴────┐                                                  │
│    │         │                                                  │
│   ┌┴┐       ┌┴┐                                                 │
│   │ │ ESP32 │ │ Arduino                                         │
│   └┬┘       └┬┘                                                 │
│    │5V      │5V                                                 │
│    │        │                                                   │
│    └───┬────┘                                                   │
│        │                                                        │
│   ┌────┴────────────────────────────────────┐                  │
│   │         5V POWER RAIL                    │                  │
│   ├─────────────────────────────────────────┤                  │
│   │ Components powered from 5V rail:        │                  │
│   │ • LCD 1602 VCC                          │                  │
│   │ • HC-SR04 VCC                           │                  │
│   │ • PIR Sensor VCC                        │                  │
│   │ • SG90 Servo (red wire)                 │                  │
│   │ • Relay Module VCC                      │                  │
│   │ • Dot Matrix VCC                        │                  │
│   │ • Water Sensor VCC                      │                  │
│   │ • Thermistor (via 5V)                   │                  │
│   │ • Photoresistor (via 5V)                │                  │
│   │ • Active Buzzer +                       │                  │
│   │ • Passive Buzzer +                      │                  │
│   │ • Potentiometer pin 3                   │                  │
│   └─────────────────────────────────────────┘                  │
│                                                                 │
│   ┌─────────────────────────────────────────┐                  │
│   │         3.3V POWER RAIL (ESP32 only)     │                  │
│   ├─────────────────────────────────────────┤                  │
│   │ Components powered from 3.3V rail:      │                  │
│   │ • OLED Display VCC                      │                  │
│   │ • DHT11 Pin 1                           │                  │
│   │ • Touch Sensor VCC                      │                  │
│   │ • Joystick VCC                          │                  │
│   │ • Soil Sensor VCC                       │                  │
│   │ • Obstacle Sensor VCC                   │                  │
│   └─────────────────────────────────────────┘                  │
│                                                                 │
│   ┌─────────────────────────────────────────┐                  │
│   │              GND RAIL (COMMON)           │                  │
│   ├─────────────────────────────────────────┤                  │
│   │ ALL grounds connect together:           │                  │
│   │ • ESP32 GND                             │                  │
│   │ • Arduino GND                           │                  │
│   │ • All sensor GNDs                       │                  │
│   │ • All LED cathodes                      │                  │
│   │ • Voltage divider GNDs                  │                  │
│   └─────────────────────────────────────────┘                  │
│                                                                 │
│   ⚠️ POWER NOTES:                                               │
│   • Servo can draw 500mA+ under load                           │
│   • Use separate 5V supply if servo jitters occur              │
│   • Add 100µF capacitor across 5V/GND near servo               │
│   • ESP32's 3.3V pin can supply ~500mA max                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## PHYSICAL STATION LAYOUT

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              DEMO STATION LAYOUT                            │
│                          (Top-down view, not to scale)                      │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐  │
│   │  ZONE A: FLOOD SIMULATION           │  ZONE B: DROUGHT MONITORING   │  │
│   │  ┌───────────────────────────────┐  │  ┌─────────────────────────┐  │  │
│   │  │  Plastic water tray         │  │  │  Plant pot with soil    │  │  │
│   │  │  ┌─────────────────────┐    │  │  │  ┌───────────────────┐  │  │  │
│   │  │  │  Water Level Sensor │    │  │  │  │ Soil Moisture     │  │  │  │
│   │  │  │      submerged      │    │  │  │  │ probe inserted    │  │  │  │
│   │  │  └─────────────────────┘    │  │  │  └───────────────────┘  │  │  │
│   │  │         ↕ HC-SR04           │  │  │      ↕ Thermistor       │  │  │
│   │  │  (20cm above water)         │  │  │  (buried in soil)       │  │  │
│   │  │                             │  │  │                         │  │  │
│   │  │  ┌─────────────────────┐    │  │  └─────────────────────────┘  │  │
│   │  │  │ Obstacle IR (debris)│    │  │                               │  │
│   │  │  └─────────────────────┘    │  │                               │  │
│   │  └───────────────────────────────┘  │                               │  │
│   └─────────────────────────────────────┼───────────────────────────────┘  │
│                                         │                                   │
│   ┌─────────────────────────────────────┼───────────────────────────────┐  │
│   │  ZONE C: LANDSLIDE SIMULATION       │  ZONE D: DISPLAY PANEL        │  │
│   │  ┌───────────────────────────────┐  │  ┌─────────────────────────┐  │  │
│   │  │  Cardboard wedge (15°)       │  │  │  ┌─────┐ ┌───────────┐  │  │  │
│   │  │  ┌─────────────────────┐     │  │  │  │ OLED│ │   LCD     │  │  │  │
│   │  │  │  Tilt Switch        │     │  │  │  │128x64│ │  1602     │  │  │  │
│   │  │  │  glued to surface   │     │  │  │  └─────┘ └───────────┘  │  │  │
│   │  │  └─────────────────────┘     │  │  │                         │  │  │
│   │  │                              │  │  │  ┌───────────────────┐  │  │  │
│   │  │  Tip wedge to trigger        │  │  │  │ 7-Segment │ Dot   │  │  │  │
│   │  │  landslide alarm             │  │  │  │ Display   │Matrix │  │  │  │
│   │  │                              │  │  │  │           │ 8x8   │  │  │  │
│   │  └───────────────────────────────┘  │  │  └───────────────────┘  │  │  │
│   └─────────────────────────────────────┼──┴─────────────────────────┘  │
│                                         │                                │
│   ┌─────────────────────────────────────┴────────────────────────────┐   │
│   │                    ZONE E: CONTROL CENTER                        │   │
│   │  ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐   │   │
│   │  │   ESP32      │◄──►│   Arduino    │    │   Breadboards    │   │   │
│   │  │   Dev Module │    │   Uno R3     │    │   (terminal      │   │   │
│   │  └──────────────┘    └──────────────┘    │    strips)       │   │   │
│   │                                          └──────────────────┘   │   │
│   │  ┌──────────────────────────────────────────────────────────┐   │   │
│   │  │  LED ARRAY:  [GREEN] [BLUE] [YELLOW] [RED]               │   │   │
│   │  └──────────────────────────────────────────────────────────┘   │   │
│   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │   │
│   │  │ SG90 Servo   │  │ Relay Module │  │ UI Controls:         │  │   │
│   │  │ (warning     │  │ (siren       │  │ • Joystick           │  │   │
│   │  │  flag)       │  │  output)     │  │ • Touch Sensor       │  │   │
│   │  └──────────────┘  └──────────────┘  │ • PIR Sensor         │  │   │
│   │                                      └──────────────────────┘  │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  ACCESSORIES:                                                   │   │
│   │  • Water pitcher (for flood demo)                               │   │
│   │  • Phone/Laptop (WiFi dashboard: http://192.168.4.1)            │   │
│   │  • External siren/lamp (connected to relay)                     │   │
│   └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## STEP-BY-STEP ASSEMBLY GUIDE

### Phase 1: Board Preparation (30 min)

1. **Mount boards on base**
   - Secure ESP32 and Arduino to foam board/cardboard using double-sided tape
   - Leave 10cm spacing between boards for wiring

2. **Install power rails**
   - Insert breadboard power rails along edges
   - Connect ESP32 5V to positive rail
   - Connect ESP32 GND to negative rail
   - Jump power rails between breadboards

3. **Connect common ground**
   - Run thick GND wire from ESP32 GND to Arduino GND
   - Connect all breadboard GND rails together

### Phase 2: ESP32 Sensors (45 min)

4. **Wire I2C displays**
   - Connect OLED (VCC→3.3V, GND→GND, SDA→GPIO21, SCL→GPIO22)
   - Connect LCD (VCC→5V, GND→GND, SDA→GPIO21, SCL→GPIO22)

5. **Wire DHT11**
   - Connect with 10kΩ pull-up resistor

6. **Wire HC-SR04**
   - Build voltage divider for ECHO pin

7. **Wire remaining sensors**
   - PIR, Soil, Obstacle, Touch, Joystick

### Phase 3: ESP32 Outputs (30 min)

8. **Wire LEDs**
   - All 4 LEDs with 220Ω current-limiting resistors

9. **Wire servo and relay**
   - Servo to GPIO13, Relay to GPIO12

10. **Wire dot matrix**
    - SPI connection (DIN→GPIO23, CLK→GPIO26, CS→GPIO15)

### Phase 4: Arduino Sensors (30 min)

11. **Wire analog sensors**
    - Water Level (A1), Thermistor (A2), Photoresistor (A3), Pot (A4)

12. **Wire tilt switch**
    - Connect to D2 (interrupt pin)

13. **Wire buzzers**
    - Active (D3), Passive (D11)

### Phase 5: Arduino Outputs (15 min)

14. **Wire 7-segment display**
    - All segments through 220Ω resistors

### Phase 6: Inter-Board Connection (15 min)

15. **Wire serial communication**
    - Build voltage divider for Arduino TX→ESP32 RX
    - Connect Arduino RX→ESP32 TX (direct)
    - Verify common ground

### Phase 7: Testing (15 min)

16. **Power-on test**
    - Connect USB cables to both boards
    - Verify no components overheat
    - Check LED boot sequence

17. **Serial monitor verification**
    - Open Serial Monitor for ESP32 (115200 baud)
    - Open Serial Monitor for Arduino (9600 baud)
    - Verify "Ready" messages

---

## TESTING & VERIFICATION

### Pre-Power Checklist

- [ ] All VCC connections verified (3.3V vs 5V)
- [ ] All GND connections common
- [ ] Voltage dividers installed (HC-SR04 ECHO, Arduino TX)
- [ ] No loose wire strands causing shorts
- [ ] LED polarity correct (long leg = anode)
- [ ] I2C addresses verified (OLED 0x3C, LCD 0x27)

### Power-On Sequence

1. **Connect ESP32 USB first**
   - OLED should display "GEO-SENSE AFRICA v2.0 BOOTING..."
   - LCD should show "GEO-SENSE AFRICA"
   - LEDs should flash green/blue 3 times
   - Servo should sweep to 0°

2. **Connect Arduino USB second**
   - 7-segment should count 0-9 during boot
   - 7-segment should display water level (0 when dry)

3. **Verify Serial Monitor output**

**ESP32 (115200 baud):**
```
AP IP address: 192.168.4.1
Geo-Sense Africa v2.0 - Master Node Ready
```

**Arduino (9600 baud):**
```
Arduino Slave Ready
```

### Sensor Verification Tests

| Sensor | Test Method | Expected Result |
|--------|-------------|-----------------|
| DHT11 | Breathe on sensor | Temp/humidity increase on dashboard |
| HC-SR04 | Move hand toward sensor | Distance value decreases |
| PIR | Wave hand in front | PIR status changes to "MOTION" |
| Soil | Insert in dry/wet soil | Moisture % changes |
| Water | Submerge probe | Water % increases, 7-seg value rises |
| Thermistor | Warm with fingers | Temperature increases |
| Tilt | Tip wedge | Buzzer sounds, RED LED, servo to 180° |
| Joystick | Push left/right | OLED page changes |
| Touch | Press sensor | Alerts acknowledge (silence buzzer) |

---

## TROUBLESHOOTING

### Display Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| OLED blank | Wrong I2C address | Try 0x3D instead of 0x3C |
| OLED garbled | Loose connection | Check SDA/SCL wiring |
| LCD shows blocks | Wrong I2C address | Try 0x3F instead of 0x27 |
| LCD backlight only | Contrast issue | Adjust LCD potentiometer |
| Dot Matrix random pixels | Wrong SPI pins | Verify DIN=23, CLK=26, CS=15 |

### Sensor Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| DHT11 reads NaN | Missing pull-up | Add 10kΩ between DATA and VCC |
| HC-SR04 always 0cm | ECHO voltage high | Verify 1kΩ+2kΩ divider |
| HC-SR04 always 400cm | No echo received | Check TRIG connection |
| PIR always triggered | Sensitivity max | Turn left pot counter-clockwise |
| Soil reads 0% | Wrong power | Verify 3.3V (not 5V) |
| Water reads 100% | Probe shorted | Clean probe, check wiring |

### Communication Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| No Serial2 data | Wrong TX/RX | Verify RX=16, TX=17 |
| Garbage serial data | Baud mismatch | Verify 9600 baud both sides |
| ESP32 resets randomly | Missing common GND | Connect ESP32 GND to Arduino GND |
| Intermittent data | Loose wires | Solder or use screw terminals |

### Power Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| Servo jitters | Insufficient current | Add 100µF cap across 5V/GND |
| ESP32 brownouts | USB underpowered | Use 2A USB supply |
| LEDs dim | High resistance | Check ground connections |
| Random resets | Power noise | Add decoupling capacitors |

### WiFi Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| AP not visible | Boot incomplete | Wait 5 seconds after power-on |
| Can't connect | Wrong password | Verify "geosense2024" |
| Dashboard won't load | Wrong URL | Use http://192.168.4.1 (not https) |
| Data not updating | JavaScript error | Clear browser cache |

---

## SAFETY WARNINGS

⚠️ **ELECTRICAL SAFETY**
- Never connect 5V to ESP32 GPIO pins (except via voltage divider)
- Always disconnect USB before modifying wiring
- Double-check polarity before powering electrolytic capacitors

⚠️ **WATER SAFETY**
- Keep water tray away from electronics
- Use battery power or GFCI-protected USB hub for demo
- Dry all sensors before storage

⚠️ **SERVO SAFETY**
- Servo can draw high current under stall conditions
- Use separate 5V supply if multiple servos
- Don't force servo mechanically while powered

---

## MAINTENANCE

### After Each Demo
1. Dry all water-exposed sensors
2. Disconnect USB power
3. Store in anti-static bag

### Monthly
1. Check all wire connections for corrosion
2. Verify voltage divider resistors
3. Test all sensors with Serial Monitor

### Before Competition
1. Replace all jumper wires (fatigue can cause breaks)
2. Re-solder any cold joints
3. Update libraries to latest versions
4. Re-flash both boards with final code

---

*GEO-SENSE AFRICA v2.0 | Dual-board Multi-Hazard Early Warning System*  
*Hack Keele Hackathon 2026*  
*Africa's last-mile disaster shield — built from two starter kits.*
