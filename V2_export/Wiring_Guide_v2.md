# GEO-SENSE AFRICA v2.0 — WIRING GUIDE
## Clear, Step-by-Step Instructions for Building the Complete System

---

## 📖 HOW TO USE THIS GUIDE

1. **Read the entire guide first** before touching any components
2. **Gather all parts** using the checklist on page 3
3. **Work through each section in order** — don't skip ahead
4. **Check off each step** as you complete it
5. **Test after each section** before moving to the next

---

## ⚡ QUICK START — MINIMUM WORKING SYSTEM

If you need a basic working demo quickly, wire only these components first:

| # | Component | Connects To | Pin |
|---|-----------|-------------|-----|
| 1 | ESP32 Board | USB Power | — |
| 2 | Arduino Uno | USB Power | — |
| 3 | OLED Display | ESP32 | SDA→21, SCL→22, VCC→3.3V, GND→GND |
| 4 | Arduino TX | ESP32 GPIO16 | **Via voltage divider!** |
| 5 | Arduino RX | ESP32 GPIO17 | Direct connection |
| 6 | Common Ground | ESP32 GND → Arduino GND | Required! |

**This minimum setup will show:**
- OLED display working
- Serial communication between boards
- WiFi dashboard accessible at http://192.168.4.1

---

## 📦 PARTS CHECKLIST

### ✅ Main Boards (2 items)
- [ ] ESP32 Dev Module (any version with 30+ pins)
- [ ] Arduino Uno R3 (or compatible)

### ✅ Displays (4 items)
- [ ] OLED 128×64 with I2C (0x3C address)
- [ ] LCD 1602 with I2C backpack (0x27 address)
- [ ] 7-Segment display (common cathode, 1 digit)
- [ ] MAX7219 8×8 Dot Matrix module

### ✅ Sensors for ESP32 (7 items)
- [ ] DHT11 temperature/humidity sensor
- [ ] HC-SR04 ultrasonic sensor
- [ ] HC-SR501 PIR motion sensor
- [ ] Capacitive soil moisture sensor
- [ ] IR obstacle avoidance sensor
- [ ] TTP223 touch sensor
- [ ] KY-023 joystick module

### ✅ Sensors for Arduino (5 items)
- [ ] Water level sensor
- [ ] Thermistor 10k NTC
- [ ] Photoresistor (LDR)
- [ ] Potentiometer 10k
- [ ] Tilt switch SW-520D

### ✅ Outputs (8 items)
- [ ] SG90 micro servo
- [ ] 1-channel relay module
- [ ] Active buzzer 5V
- [ ] Passive buzzer 5V
- [ ] LED green 5mm
- [ ] LED blue 5mm
- [ ] LED yellow 5mm
- [ ] LED red 5mm

### ✅ Resistors (18 pieces)
- [ ] 220Ω resistors ×8 (red-red-brown-gold) — for LEDs
- [ ] 1kΩ resistors ×4 (brown-black-red-gold) — for voltage dividers
- [ ] 2kΩ resistors ×2 (red-black-red-gold) — for voltage dividers
- [ ] 10kΩ resistors ×4 (brown-black-orange-gold) — for pull-ups

### ✅ Miscellaneous
- [ ] Breadboards ×2 (400 points each)
- [ ] Jumper wires (male-to-male) ×40
- [ ] Jumper wires (male-to-female) ×20
- [ ] USB cables ×2 (micro-USB)
- [ ] Cardboard or foam board (for base)

---

## 🔌 SECTION 1: ESP32 MASTER BOARD

### Pin Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 PIN MAP                            │
├──────────────┬──────────────┬──────────────┬───────────────┤
│ GPIO 21 ────►│ I2C SDA      │ GPIO 25 ────►│ Green LED     │
│ GPIO 22 ────►│ I2C SCL      │ GPIO 2  ────►│ Blue LED      │
│ GPIO 4  ────►│ DHT11 Data   │ GPIO 27 ────►│ Yellow LED    │
│ GPIO 5  ────►│ HC-SR04 Trig │ GPIO 33 ────►│ Red LED       │
│ GPIO 18 ────►│ HC-SR04 Echo │ GPIO 23 ────►│ Dot Matrix DIN│
│ GPIO 19 ────►│ PIR Out      │ GPIO 26 ────►│ Dot Matrix CLK│
│ GPIO 13 ────►│ Servo        │ GPIO 15 ────►│ Dot Matrix CS │
│ GPIO 12 ────►│ Relay        │ GPIO 36 ────►│ Soil Sensor   │
│ GPIO 14 ────►│ Touch        │ GPIO 39 ────►│ Obstacle IR   │
│ GPIO 34 ────►│ Joystick X   │ GPIO 16 ────►│ Arduino TX    │
│ GPIO 35 ────►│ Joystick Y   │ GPIO 17 ────►│ Arduino RX    │
│ GPIO 32 ────►│ Joystick Btn │              │               │
└──────────────┴──────────────┴──────────────┴───────────────┘
```

---

### 1.1 — I2C Displays (OLED + LCD)

**Time:** 10 minutes | **Difficulty:** Easy

Both displays share the same two data wires (I2C bus).

```
WIRING DIAGRAM:

ESP32          OLED Display        LCD Display
─────          ────────────        ───────────
GPIO21 ──────── SDA ─────────────── SDA
GPIO22 ──────── SCL ─────────────── SCL
3.3V ────────── VCC                (not connected)
GND ─────────── GND ─────────────── GND
5V ──────────── (not connected) ─── VCC
```

**Steps:**

1. Connect ESP32 **GPIO21** to OLED **SDA**
2. Connect ESP32 **GPIO22** to OLED **SCL**
3. Connect ESP32 **3.3V** to OLED **VCC**
4. Connect ESP32 **GND** to OLED **GND**
5. Connect OLED **SDA** to LCD **SDA** (daisy-chain)
6. Connect OLED **SCL** to LCD **SCL** (daisy-chain)
7. Connect Arduino **5V** to LCD **VCC**
8. Connect LCD **GND** to any GND point

**Test:** After uploading code, OLED should show "GEO-SENSE AFRICA"

---

### 1.2 — DHT11 Temperature Sensor

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

DHT11 (facing the grille)
┌───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ 4 │  ← Pin numbers
└─┬─┴─┬─┴───┴─┬─┘
  │   │       │
  │   └──► GPIO4 (DATA)
  │           │
  │        ┌──┴──┐
  │        │ 10k │  ← Resistor
  │        └──┬──┘
  │           │
  └──────────► 3.3V
              │
Pin 4 ───────► GND
```

**Steps:**

1. Connect DHT11 **Pin 1** to ESP32 **3.3V**
2. Connect DHT11 **Pin 2** to ESP32 **GPIO4**
3. Connect DHT11 **Pin 4** to ESP32 **GND**
4. Solder **10kΩ resistor** between Pin 1 and Pin 2

**Note:** Pin 3 is not connected (NC = No Connection)

---

### 1.3 — HC-SR04 Ultrasonic Sensor

**Time:** 10 minutes | **Difficulty:** Medium

⚠️ **WARNING:** Requires voltage divider on ECHO pin!

```
WIRING DIAGRAM:

HC-SR04                    ESP32
───────                    ─────
VCC ─────────────────────► 5V
TRIG ────────────────────► GPIO5
ECHO ──► 1kΩ ──┬─────────► GPIO18
               │
              2kΩ
               │
              GND
GND ─────────────────────► GND
```

**Steps:**

1. Connect HC-SR04 **VCC** to **5V**
2. Connect HC-SR04 **GND** to **GND**
3. Connect HC-SR04 **TRIG** to ESP32 **GPIO5**
4. Connect HC-SR04 **ECHO** to one end of **1kΩ resistor**
5. Connect other end of 1kΩ to ESP32 **GPIO18**
6. Connect **2kΩ resistor** from GPIO18 to **GND**

**Why the voltage divider?** HC-SR04 outputs 5V, but ESP32 can only handle 3.3V. The resistors reduce the voltage safely.

---

### 1.4 — PIR Motion Sensor

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

HC-SR501 PIR (bottom view)
┌─────────────────┐
│    ┌───────┐    │
│    │ LENS  │    │  ← Front (facing out)
│    └───────┘    │
│    ○ ○ ○        │  ← Pins (rear view)
│    │ │ │        │
│    │ │ └────────┼──► GND
│    │ └──────────┼──► GPIO19 (OUT)
│    └────────────┼──► 5V (VCC)
└─────────────────┘
```

**Steps:**

1. Connect PIR **VCC** to **5V**
2. Connect PIR **GND** to **GND**
3. Connect PIR **OUT** to ESP32 **GPIO19**

**Adjustment:** Turn the two trim pots on the sensor:
- **Left pot:** Turn clockwise for more sensitivity
- **Right pot:** Turn counter-clockwise for shorter delay (~3 seconds)

---

### 1.5 — SG90 Servo Motor

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

SG90 Servo (female connector)
┌───────────────┐
│ Brown Red Orange │  ← Wire colors
│   │    │    │     │
│   │    │    └─────┼──► GPIO13
│   │    │          │
│   │    └──────────┼──► 5V
│   │               │
│   └───────────────┼──► GND
└───────────────────┘
```

**Steps:**

1. Connect **Brown wire** to **GND**
2. Connect **Red wire** to **5V**
3. Connect **Orange wire** to ESP32 **GPIO13**

**Servo Positions:**
- 0° = All clear (flag down)
- 90° = Caution
- 180° = Emergency (flag raised)

---

### 1.6 — Relay Module

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Relay Module              ESP32
────────────              ─────
VCC ────────────────────► 5V
GND ────────────────────► GND
IN  ────────────────────► GPIO12

COM ──┐
      ├──► Connect external siren/lamp here
NO  ──┘
```

**Steps:**

1. Connect Relay **VCC** to **5V**
2. Connect Relay **GND** to **GND**
3. Connect Relay **IN** to ESP32 **GPIO12**
4. Connect external device to **COM** and **NO** terminals

**Note:** Relay is ACTIVE LOW (triggers when GPIO12 is LOW)

---

### 1.7 — Dot Matrix 8×8

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

MAX7219 Module          ESP32
──────────────          ─────
VCC ──────────────────► 5V
GND ──────────────────► GND
DIN ──────────────────► GPIO23
CLK ──────────────────► GPIO26
CS  ──────────────────► GPIO15
```

**Steps:**

1. Connect **VCC** to **5V**
2. Connect **GND** to **GND**
3. Connect **DIN** to ESP32 **GPIO23**
4. Connect **CLK** to ESP32 **GPIO26**
5. Connect **CS** to ESP32 **GPIO15**

---

### 1.8 — Touch Sensor

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

TTP223B              ESP32
───────              ─────
VCC ────────────────► 3.3V
GND ────────────────► GND
I/O ────────────────► GPIO14
```

**Steps:**

1. Connect **VCC** to **3.3V**
2. Connect **GND** to **GND**
3. Connect **I/O** to ESP32 **GPIO14**

**Function:** Touch to acknowledge/silence alarms

---

### 1.9 — Joystick Module

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

KY-023 Joystick        ESP32
─────────────          ─────
VCC ──────────────────► 3.3V
GND ──────────────────► GND
VRx ──────────────────► GPIO34
VRy ──────────────────► GPIO35
SW  ──────────────────► GPIO32
```

**Steps:**

1. Connect **VCC** to **3.3V**
2. Connect **GND** to **GND**
3. Connect **VRx** to ESP32 **GPIO34**
4. Connect **VRy** to ESP32 **GPIO35**
5. Connect **SW** to ESP32 **GPIO32**

**Function:** Push left/right to change OLED display pages

---

### 1.10 — Soil Moisture Sensor

**Time:** 3 minutes | **Difficulty:** Easy

⚠️ **WARNING:** Use 3.3V NOT 5V!

```
WIRING DIAGRAM:

Soil Sensor            ESP32
───────────            ─────
VCC ──────────────────► 3.3V  ⚠️ NOT 5V!
GND ──────────────────► GND
AOUT ─────────────────► GPIO36
```

**Steps:**

1. Connect **VCC** to ESP32 **3.3V** (NOT 5V!)
2. Connect **GND** to **GND**
3. Connect **AOUT** to ESP32 **GPIO36**

---

### 1.11 — Obstacle IR Sensor

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

IR Obstacle            ESP32
───────────            ─────
VCC ──────────────────► 3.3V
GND ──────────────────► GND
OUT ──────────────────► GPIO39
```

**Steps:**

1. Connect **VCC** to **3.3V**
2. Connect **GND** to **GND**
3. Connect **OUT** to ESP32 **GPIO39**

**Output:** LOW when obstacle detected

---

### 1.12 — Status LEDs (4 colors)

**Time:** 10 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

ESP32        220Ω Resistor     LED
─────        ─────────────     ───
GPIO25 ──►───[220Ω]───►─── Anode(+) Green LED ──► GND
GPIO2  ──►───[220Ω]───►─── Anode(+) Blue LED  ──► GND
GPIO27 ──►───[220Ω]───►─── Anode(+) Yellow LED ─► GND
GPIO33 ──►───[220Ω]───►─── Anode(+) Red LED   ──► GND
```

**Steps:**

For each LED (Green, Blue, Yellow, Red):

1. Connect ESP32 pin to one end of **220Ω resistor**
2. Connect other end of resistor to LED **long leg** (anode, +)
3. Connect LED **short leg** (cathode, -) to **GND**

**LED Pin Mapping:**
| Color | ESP32 Pin | Meaning |
|-------|-----------|---------|
| Green | GPIO25 | All clear |
| Blue | GPIO2 | Flood detected |
| Yellow | GPIO27 | Drought warning |
| Red | GPIO33 | Critical alert |

---

## 🔌 SECTION 2: ARDUINO SLAVE BOARD

### Pin Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│                  ARDUINO UNO PIN MAP                        │
├──────────────┬──────────────┬──────────────┬───────────────┤
│ A1  ────────►│ Water Level  │ D4  ────────►│ 7-Seg A       │
│ A2  ────────►│ Thermistor   │ D5  ────────►│ 7-Seg B       │
│ A3  ────────►│ Photoresistor│ D6  ────────►│ 7-Seg C       │
│ A4  ────────►│ Potentiometer│ D11 ────────►│ Passive Buzzer│
│ A5  ────────►│ 7-Seg F      │ D12 ────────►│ 7-Seg D       │
│ D2  ────────►│ Tilt Switch  │ D13 ────────►│ 7-Seg E       │
│ D3  ────────►│ Active Buzzer│ D0  ────────►│ ESP32 GPIO17  │
│              │              │ D1  ────────►│ ESP32 GPIO16  │
└──────────────┴──────────────┴──────────────┴───────────────┘
```

---

### 2.1 — Water Level Sensor

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Water Sensor           Arduino
────────────           ───────
VCC ──────────────────► 5V
GND ──────────────────► GND
SIG ──────────────────► A1
```

**Steps:**

1. Connect **VCC** to **5V**
2. Connect **GND** to **GND**
3. Connect **SIG** to Arduino **A1**

---

### 2.2 — Thermistor (Temperature)

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Thermistor             Arduino
────────────           ───────
Leg 1 ────────────────► 5V
Leg 2 ──┬─────────────► A2
        │
       10kΩ
        │
       GND
```

**Steps:**

1. Connect thermistor **Leg 1** to **5V**
2. Connect thermistor **Leg 2** to Arduino **A2**
3. Connect **10kΩ resistor** from A2 to **GND**

---

### 2.3 — Photoresistor (Light Sensor)

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Photoresistor          Arduino
─────────────          ───────
Leg 1 ────────────────► 5V
Leg 2 ──┬─────────────► A3
        │
       10kΩ
        │
       GND
```

**Steps:**

1. Connect photoresistor **Leg 1** to **5V**
2. Connect photoresistor **Leg 2** to Arduino **A3**
3. Connect **10kΩ resistor** from A3 to **GND**

---

### 2.4 — Potentiometer (Calibration)

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Potentiometer          Arduino
─────────────          ───────
Pin 1 (left) ─────────► GND
Pin 2 (center) ───────► A4
Pin 3 (right) ────────► 5V
```

**Steps:**

1. Connect **left pin** to **GND**
2. Connect **center pin** to Arduino **A4**
3. Connect **right pin** to **5V**

---

### 2.5 — Tilt Switch

**Time:** 5 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Tilt Switch            Arduino
────────────           ───────
Pin 1 ────────────────► D2
Pin 2 ────────────────► GND
```

**Steps:**

1. Connect **Pin 1** to Arduino **D2**
2. Connect **Pin 2** to **GND**

**Mounting:** Glue to cardboard at 15° angle. Tips to trigger landslide alarm.

---

### 2.6 — Active Buzzer

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Active Buzzer          Arduino
─────────────          ───────
+ (long leg) ─────────► D3
- (short leg) ────────► GND
```

**Steps:**

1. Connect **+** (long leg) to Arduino **D3**
2. Connect **-** (short leg) to **GND**

---

### 2.7 — Passive Buzzer

**Time:** 3 minutes | **Difficulty:** Easy

```
WIRING DIAGRAM:

Passive Buzzer         Arduino
──────────────         ───────
+ ────────────────────► D11
- ────────────────────► GND
```

**Steps:**

1. Connect **+** to Arduino **D11**
2. Connect **-** to **GND**

---

### 2.8 — 7-Segment Display

**Time:** 15 minutes | **Difficulty:** Medium

```
WIRING DIAGRAM:

7-Segment (front view)     Arduino        Resistor
────────────────────       ───────        ────────
     ┌───┐
  f  │ a │  b             Segment A ──►── D4 ──►──[220Ω]───► pin a
     │───│                Segment B ──►── D5 ──►──[220Ω]───► pin b
  e  │ g │  c             Segment C ──►── D6 ──►──[220Ω]───► pin c
     │───│                Segment D ──►── D12 ─►──[220Ω]───► pin d
     │ d │                Segment E ──►── D13 ─►──[220Ω]───► pin e
     └───┘                Segment F ──►── A5 ──►──[220Ω]───► pin f
                          Segment G ───────────────────────► GND
                          Common Cathode ──────────────────► GND
```

**Steps:**

1. Identify pins using multimeter (or datasheet)
2. Connect each segment through **220Ω resistor** to Arduino:
   - Segment **a** → **D4**
   - Segment **b** → **D5**
   - Segment **c** → **D6**
   - Segment **d** → **D12**
   - Segment **e** → **D13**
   - Segment **f** → **A5**
3. Connect segment **g** directly to **GND**
4. Connect **common cathode** to **GND**

---

## 🔌 SECTION 3: INTER-BOARD CONNECTION

### 3.1 — Serial Communication

**Time:** 10 minutes | **Difficulty:** Medium

⚠️ **CRITICAL:** Voltage divider required on Arduino TX!

```
WIRING DIAGRAM:

Arduino Uno              ESP32 Master
───────────              ────────────
D1 (TX) ──►──[1kΩ]──┬───► GPIO16 (RX2)
                    │
                   [2kΩ]
                    │
                   GND

D0 (RX) ──────────────────► GPIO17 (TX2)

GND ──────────────────────► GND  ⚠️ MUST CONNECT!
```

**Steps:**

1. Connect Arduino **D1 (TX)** to one end of **1kΩ resistor**
2. Connect other end of 1kΩ to ESP32 **GPIO16**
3. Connect **2kΩ resistor** from GPIO16 to **GND**
4. Connect Arduino **D0 (RX)** to ESP32 **GPIO17** (direct, no resistor)
5. Connect Arduino **GND** to ESP32 **GND** (REQUIRED!)

**Why the voltage divider?** Arduino TX outputs 5V, but ESP32 RX can only handle 3.3V!

---

## ⚡ SECTION 4: POWER DISTRIBUTION

### 4.1 — Power Overview

```
POWER FLOW DIAGRAM:

                    USB Cable (5V)
                         │
                    ┌────┴────┐
                    │         │
                   ┌┴┐       ┌┴┐
                   │E│       │A│  E = ESP32, A = Arduino
                   │S│       │R│
                   │P│       │D│
                   │3│       │U│
                   │2│       │N│
                   └┬┘       └┬┘
                    │5V      │5V
                    └───┬────┘
                        │
              ┌─────────┴─────────┐
              │   5V POWER RAIL   │
              ├───────────────────┤
              │ Powers:           │
              │ • LCD Display     │
              │ • HC-SR04         │
              │ • PIR Sensor      │
              │ • Servo           │
              │ • Relay           │
              │ • Dot Matrix      │
              │ • Water Sensor    │
              │ • Buzzers         │
              └───────────────────┘

              ┌───────────────────┐
              │   3.3V POWER RAIL │
              │   (from ESP32)    │
              ├───────────────────┤
              │ Powers:           │
              │ • OLED Display    │
              │ • DHT11           │
              │ • Touch Sensor    │
              │ • Joystick        │
              │ • Soil Sensor     │
              │ • Obstacle IR     │
              └───────────────────┘

              ┌───────────────────┐
              │   GND (COMMON)    │
              ├───────────────────┤
              │ ALL grounds       │
              │ connect together  │
              └───────────────────┘
```

### 4.2 — Ground Connections Checklist

All these points must connect to common ground:

- [ ] ESP32 GND
- [ ] Arduino GND
- [ ] OLED GND
- [ ] LCD GND
- [ ] All sensor GNDs
- [ ] All LED cathodes
- [ ] Voltage divider GNDs
- [ ] Servo GND
- [ ] Relay GND
- [ ] Buzzer GNDs

---

## 🧪 SECTION 5: TESTING CHECKLIST

### Before Power-On

- [ ] All VCC connections correct (3.3V vs 5V)
- [ ] All GND connections common
- [ ] Voltage divider on HC-SR04 ECHO
- [ ] Voltage divider on Arduino TX
- [ ] No loose wire strands
- [ ] LED polarity correct (long leg = +)

### Power-On Test

1. **Connect ESP32 USB only**
   - [ ] OLED shows "GEO-SENSE AFRICA"
   - [ ] LCD shows "GEO-SENSE AFRICA"
   - [ ] LEDs flash green/blue 3 times
   - [ ] Servo sweeps to 0°

2. **Connect Arduino USB**
   - [ ] 7-segment counts 0-9
   - [ ] 7-segment shows water level

3. **Check Serial Monitors**

**ESP32 (115200 baud):**
```
AP IP address: 192.168.4.1
Geo-Sense Africa v2.0 - Master Node Ready
```

**Arduino (9600 baud):**
```
Arduino Slave Ready
```

### Sensor Tests

| Sensor | Test | Expected Result |
|--------|------|-----------------|
| DHT11 | Breathe on it | Temp/humidity increases |
| HC-SR04 | Move hand closer | Distance decreases |
| PIR | Wave hand | Dashboard shows "MOTION" |
| Soil | Touch probe | Moisture % changes |
| Water | Submerge probe | Water % increases |
| Tilt | Tip the wedge | Buzzer sounds, RED LED on |
| Joystick | Push left/right | OLED page changes |
| Touch | Press sensor | Alarm silences |

---

## ❌ TROUBLESHOOTING

### Display Problems

| Problem | Fix |
|---------|-----|
| OLED blank | Check I2C address (try 0x3D) |
| LCD shows squares | Adjust contrast pot on backpack |
| LCD wrong characters | Check I2C address (try 0x3F) |
| Dot Matrix random | Verify CLK=GPIO26, not GPIO18 |

### Sensor Problems

| Problem | Fix |
|---------|-----|
| DHT11 shows NaN | Add 10kΩ pull-up resistor |
| HC-SR04 shows 0 | Check voltage divider on ECHO |
| PIR always on | Turn sensitivity pot down |
| Soil always 0% | Verify using 3.3V not 5V |

### Communication Problems

| Problem | Fix |
|---------|-----|
| No data between boards | Check common GND connection |
| Garbage data | Verify 9600 baud on both |
| ESP32 resets | Add common ground wire |

### WiFi Problems

| Problem | Fix |
|---------|-----|
| Can't find WiFi | Wait 5 seconds after power-on |
| Wrong password | It's "geosense2024" |
| Dashboard won't load | Use http://192.168.4.1 (not https) |

---

## 📞 QUICK REFERENCE

### WiFi Access
- **Network:** GeoSense-Africa
- **Password:** geosense2024
- **Dashboard:** http://192.168.4.1

### Alert Levels
| Level | LED | Servo | Meaning |
|-------|-----|-------|---------|
| 0 | Green | 0° | All clear |
| 1 | Yellow | 90° | Caution |
| 2 | Red | 180° | Emergency |

### Hazard Types
| Hazard | LED | Trigger |
|--------|-----|---------|
| Flood | Blue | Water > 60% |
| Drought | Yellow | Soil > 70% |
| Landslide | Red | Tilt switch triggered |

---
