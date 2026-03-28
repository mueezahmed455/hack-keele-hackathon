# GEO-SENSE AFRICA v2.0 — COMPLETE WIRING GUIDE
## ESP32 Master + Arduino Uno Slave + Both Kits

---

## SYSTEM ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────┐
│                    GEO-SENSE AFRICA v2.0                    │
│                                                             │
│  ┌──────────────────┐     Serial      ┌──────────────────┐ │
│  │    ESP32 MASTER  │◄───TX/RX───────►│  ARDUINO SLAVE   │ │
│  │                  │                 │                  │ │
│  │ • WiFi Dashboard │                 │ • Water Level    │ │
│  │ • OLED Display   │                 │ • Thermistor     │ │
│  │ • LCD 1602       │                 │ • Potentiometer  │ │
│  │ • HC-SR04 Flood  │                 │ • Tilt Switch    │ │
│  │ • PIR Motion     │                 │ • 7-Seg Display  │ │
│  │ • SG90 Servo     │                 │ • Active Buzzer  │ │
│  │ • Relay Module   │                 │ • Passive Buzzer │ │
│  │ • Dot Matrix     │                 │ • Photoresistor  │ │
│  │ • DHT11          │                 │                  │ │
│  │ • Soil Sensor    │                 │                  │ │
│  │ • Obstacle IR    │                 │                  │ │
│  │ • 4x LEDs        │                 │                  │ │
│  └──────────────────┘                 └──────────────────┘ │
│           │                                                  │
│      WiFi AP ──► Phone/Laptop ──► http://192.168.4.1        │
└─────────────────────────────────────────────────────────────┘
```

---

## PART A — ESP32 MASTER WIRING

### A1 — I2C BUS (OLED + LCD share same bus)
```
ESP32 GPIO21 (SDA) ──► OLED SDA  AND  LCD SDA
ESP32 GPIO22 (SCL) ──► OLED SCL  AND  LCD SCL
OLED VCC            ──► 3.3V
OLED GND            ──► GND
LCD  VCC            ──► 5V  (LCD needs 5V, OLED 3.3V — both on same I2C is fine)
LCD  GND            ──► GND
```
**IMPORTANT:** If OLED and LCD share I2C, they need different addresses:
- OLED default: 0x3C
- LCD default:  0x27
- If they clash, change LCD address jumper (solder A0/A1/A2 pads)

### A2 — DHT11 (on ESP32)
```
DHT11 Pin 1 (VCC)  ──► 3.3V
DHT11 Pin 2 (DATA) ──► ESP32 GPIO4  + 10kΩ pull-up to 3.3V
DHT11 Pin 4 (GND)  ──► GND
```

### A3 — HC-SR04 Ultrasound (Flood Distance)
```
HC-SR04 VCC   ──► 5V
HC-SR04 GND   ──► GND
HC-SR04 TRIG  ──► ESP32 GPIO5
HC-SR04 ECHO  ──► Voltage divider ──► ESP32 GPIO18
```
**VOLTAGE DIVIDER for ECHO (5V→3.3V):**
```
ECHO ──► 1kΩ ──► GPIO18
                    │
                   2kΩ
                    │
                   GND
```
Use 1kΩ + 2kΩ from your resistor kit (brown-black-red and red-black-red).

### A4 — HC-SR501 PIR Motion Sensor
```
PIR VCC  ──► 5V
PIR GND  ──► GND
PIR OUT  ──► ESP32 GPIO19
```
Adjust the two trim pots on the PIR:
- Left pot:  sensitivity (turn clockwise = more sensitive)
- Right pot: time delay (turn anticlockwise = shortest delay ~3s)

### A5 — SG90 Servo Motor
```
Servo Brown  (GND)   ──► GND
Servo Red    (5V)    ──► 5V
Servo Orange (Signal)──► ESP32 GPIO13
```
Servo positions used by code:
- 0°   = ALL CLEAR (flag down)
- 90°  = CAUTION
- 135° = WARNING
- 180° = CRITICAL / EVACUATE (flag fully raised)

### A6 — 1-Way Relay Module
```
Relay VCC  ──► 5V
Relay GND  ──► GND
Relay IN   ──► ESP32 GPIO12  (LOW = relay fires)
```
Connect your external siren or warning lamp to the relay's:
- COM (common) + NO (normally open) terminals
The relay fires when Risk Index ≥ 7 for 3+ consecutive readings.

### A7 — 8x8 Red Dot Matrix (SPI)
```
Matrix VCC  ──► 5V
Matrix GND  ──► GND
Matrix DIN  ──► ESP32 GPIO23 (MOSI)
Matrix CLK  ──► ESP32 GPIO18 (SCK)
Matrix CS   ──► ESP32 GPIO15
```
**NOTE:** GPIO18 is shared with HC-SR04 ECHO in the default mapping.
If you get conflicts, move the dot matrix CLK to GPIO26 and update PIN_DM_CLK.

### A8 — TTP223B Touch Sensor
```
Touch VCC ──► 3.3V
Touch GND ──► GND
Touch SIG ──► ESP32 GPIO14
```
Touch = manual alert acknowledge (silences buzzer + relay for 60s).

### A9 — Joystick Module
```
Joystick VCC  ──► 3.3V
Joystick GND  ──► GND
Joystick VRx  ──► ESP32 GPIO34 (ADC only)
Joystick VRy  ──► ESP32 GPIO35 (ADC only)
Joystick SW   ──► ESP32 GPIO32 + 10kΩ pull-up to 3.3V
```
Push LEFT/RIGHT to cycle OLED display pages:
Page 0: Risk Index + waveform
Page 1: Flood data
Page 2: Drought data
Page 3: System info + WiFi

### A10 — Soil Humidity Sensor (on ESP32)
```
Soil VCC    ──► 3.3V  (use 3.3V not 5V for ESP32 ADC safety)
Soil GND    ──► GND
Soil Signal ──► ESP32 GPIO36 (ADC1 — input only pin)
```

### A11 — Obstacle Avoidance Module (on ESP32)
```
Obstacle VCC ──► 3.3V
Obstacle GND ──► GND
Obstacle OUT ──► ESP32 GPIO39 (ADC1 — input only)
```

### A12 — Indicator LEDs (4 hazard channels)
```
ESP32 GPIO25 ──► 220Ω ──► GREEN LED  anode  ──► GND   (All Clear)
ESP32 GPIO26 ──► 220Ω ──► BLUE  LED  anode  ──► GND   (Flood)
ESP32 GPIO27 ──► 220Ω ──► YELLOW LED anode  ──► GND   (Drought)
ESP32 GPIO33 ──► 220Ω ──► RED   LED  anode  ──► GND   (Landslide/Critical)
```
Use the 220Ω resistors from your resistor pack (red-red-brown-gold).

---

## PART B — ARDUINO UNO SLAVE WIRING

### B1 — Serial Link (ESP32 ↔ Arduino)
```
Arduino TX (D1) ──► ESP32 GPIO16 (RX2)
Arduino RX (D0) ──► ESP32 GPIO17 (TX2)
Arduino GND     ──► ESP32 GND  (MUST share ground!)
```
**VOLTAGE WARNING:** Arduino TX is 5V, ESP32 RX is 3.3V max!
Add a voltage divider:
```
Arduino TX ──► 1kΩ ──► ESP32 RX
                          │
                         2kΩ
                          │
                         GND
```

### B2 — Water Level Sensor
```
Water Sensor VCC  ──► 5V
Water Sensor GND  ──► GND
Water Sensor S    ──► Arduino A1
```

### B3 — Thermistor (Soil Temperature)
```
Thermistor leg 1  ──► 5V
Thermistor leg 2  ──► Arduino A2  AND  10kΩ to GND
```

### B4 — Photoresistor
```
Photoresistor leg 1  ──► 5V
Photoresistor leg 2  ──► Arduino A3  AND  10kΩ to GND
```

### B5 — Potentiometer (Sensitivity Calibration)
```
Pot pin 1 (left)  ──► GND
Pot pin 2 (wiper) ──► Arduino A4
Pot pin 3 (right) ──► 5V
```

### B6 — Tilt Switch (MUST be D2 for hardware interrupt)
```
Tilt leg 1  ──► Arduino D2
Tilt leg 2  ──► GND
```
No resistor needed — code uses INPUT_PULLUP.

### B7 — Active Buzzer
```
Buzzer + (long leg) ──► Arduino D3
Buzzer - (short)    ──► GND
```

### B8 — Passive Buzzer
```
Passive Buzzer +  ──► Arduino D11
Passive Buzzer -  ──► GND
```

### B9 — 7-Segment Display (1 digit, common cathode)
```
Display pin a ──► 220Ω ──► Arduino D4
Display pin b ──► 220Ω ──► Arduino D5
Display pin c ──► 220Ω ──► Arduino D6
Display pin d ──► 220Ω ──► Arduino D12
Display pin e ──► 220Ω ──► Arduino D13
Display pin f ──► 220Ω ──► Arduino A5
Display GND   ──► Arduino GND  (common cathode — connect ALL GND pins)
```

---

## PHYSICAL STATION LAYOUT

```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  ZONE A: WATER/FLOOD          ZONE B: SOIL/DROUGHT        │
│  ┌──────────────────┐         ┌──────────────────┐        │
│  │ [Water tray]     │         │ [Soil cup]       │        │
│  │  Water Level ──┐ │         │  Soil Sensor ──┐ │        │
│  │  HC-SR04 ↕↕   │ │         │  Thermistor ──┐│ │        │
│  │  Obstacle IR  │ │         │               ││ │        │
│  └───────────────│─┘         └───────────────││─┘        │
│                  │                            ││           │
│  ZONE C: SLOPE              ZONE D: DISPLAYS  ││           │
│  ┌──────────────────┐       ┌───────────────────┐         │
│  │ [Cardboard wedge]│       │ [OLED] [LCD]      │         │
│  │  Tilt switch     │       │ [7-Seg] [DotMtrx] │         │
│  │  (15° angle)     │       └───────────────────┘         │
│  └──────────────────┘                                      │
│                                                            │
│  ZONE E: CENTRAL CONTROL                                   │
│  ┌──────────────────────────────────────────────┐         │
│  │  [ESP32] ←──→ [Arduino Uno]                  │         │
│  │  [Breadboard 1]   [Breadboard 2]              │         │
│  │                                               │         │
│  │  [GREEN LED] [BLUE LED] [YELLOW LED] [RED LED]│         │
│  │  [Servo Flag ↑]  [Relay → Siren]              │         │
│  │  [POT dial]  [Touch sensor]  [PIR]            │         │
│  └──────────────────────────────────────────────┘         │
│                                                            │
│  [Joystick] = OLED page control                            │
└────────────────────────────────────────────────────────────┘
```

---

## REQUIRED LIBRARIES — INSTALL BEFORE HACKATHON

### For ESP32 (in Arduino IDE):
1. `Adafruit SSD1306`           — OLED display
2. `Adafruit GFX Library`       — Graphics (auto-installed with SSD1306)
3. `LiquidCrystal I2C`          — LCD 1602
4. `DHT sensor library`         — Adafruit DHT
5. `Adafruit Unified Sensor`    — Required by DHT
6. `ESP32Servo`                 — Servo on ESP32
7. `MD_MAX72XX`                 — 8x8 Dot Matrix
8. `MD_Parola`                  — (optional, for text scrolling)

### For Arduino Uno:
1. `DHT sensor library`         — Adafruit

### Board Manager:
- Add ESP32 boards: File → Preferences → Board Manager URLs:
  `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Install: "esp32 by Espressif Systems"
- Select board: "ESP32 Dev Module"

---

## HOW TO ACCESS THE WIFI DASHBOARD

1. Power on the ESP32
2. On your phone or laptop, go to WiFi settings
3. Connect to: **GeoSense-Africa**
4. Password: **geosense2024**
5. Open browser → go to: **http://192.168.4.1**
6. Dashboard auto-refreshes every 4 seconds
7. Shows all sensor readings + risk index live

---

## DEMO SCRIPT v2.0 (5 minutes)

### [0:00–0:30] BOOT SEQUENCE
- Power on both boards
- Watch LED chase, servo sweep, LCD welcome screen
- "The system self-tests on every boot and raises the servo to all-clear position"

### [0:30–1:15] DROUGHT SCENARIO
- Leave soil cup dry, cover photoresistor
- Point at LCD: soil moisture low, ET rate rising, drought risk increasing
- Joystick to OLED page 2: "Watch evapotranspiration climb"
- "We detect the wilting point 72 hours before farmers see crop failure"

### [1:15–2:00] FLOOD SCENARIO
- Pour water slowly into tray → water level sensor activates
- Move hand toward HC-SR04 → distance drops → flood risk spikes
- Wave cardboard at obstacle IR → debris detected, risk jumps
- "Distance sensor acts as a riverbank monitor — as water rises, distance to sensor drops"
- Watch BLUE LED light up, LCD shows FLOOD

### [2:00–2:30] EVACUATION DETECTION
- Stand near PIR sensor → motion detected
- Combined with high water level = evacuation event detected
- "If people are already running, that's a signal itself"

### [2:30–3:15] LANDSLIDE INTERRUPT
- Tip the tilt switch (on its cardboard wedge)
- Buzzer SCREAMS, RED LED on, servo slams to 180°, relay fires
- 7-seg shows 9, OLED shows RISK 9/9
- "Hardware interrupt — zero software delay. Earth moves, alarm fires."
- Touch sensor to acknowledge: "Village leader can silence with one touch"

### [3:15–4:00] WIFI DASHBOARD
- Show on phone: connected to GeoSense-Africa
- Open http://192.168.4.1
- "Any smartphone within WiFi range gets live readings — no app needed"
- "A regional coordinator 500m away sees the same data in real time"

### [4:00–5:00] CLOSING
- Turn potentiometer: "Sensitivity adapts to local terrain — this dial is the difference between a village on a stable plateau vs an unstable hillside"
- "No satellite. No internet. No smartphone required for the sensor to work."
- "Total cost: under $80 for both boards plus all sensors."
- "A $10M NGO flood warning system versus a $80 GEO-SENSE node — same outcome, different scale."

---

## TROUBLESHOOTING v2.0

| Problem | Cause | Fix |
|---------|-------|-----|
| ESP32 crashes on boot | Library missing | Install all 7 libraries |
| OLED blank | Wrong I2C address | Try 0x3C or 0x3D in code |
| LCD shows garbage | Wrong I2C address | Try 0x27 or 0x3F |
| No Serial2 data | Wrong TX/RX | RX=16, TX=17; check voltage divider |
| Servo jitters | Power instability | Add 100µF cap across 5V/GND near servo |
| HC-SR04 always 0 | ECHO voltage too high | Check 1kΩ+2kΩ divider on ECHO pin |
| PIR always triggered | Sensitivity too high | Turn left trim pot anticlockwise |
| WiFi not visible | Boot not complete | Wait 5 seconds after power-on |
| Arduino TX damages ESP32 | No voltage divider | CRITICAL — add 1kΩ+2kΩ divider |

---

*GEO-SENSE AFRICA v2.0 | Dual-board MHEWS | Hackathon Edition*
*Africa's last-mile disaster shield — built from two starter kits.*
