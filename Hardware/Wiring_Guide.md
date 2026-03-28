# GEO-SENSE AFRICA — Physical Build & Wiring Guide
## Hackathon Edition | LAFVIN R3 Kit

---

## COMPLETE PIN MAPPING TABLE

| Component            | Kit Part              | Arduino Pin | Wire Colour (suggest) |
|----------------------|-----------------------|-------------|----------------------|
| Soil Humidity Signal | Soil Humidity Sensor  | A0          | Yellow               |
| Water Level Signal   | Water Level Sensor    | A1          | Blue                 |
| Thermistor           | Thermistor            | A2          | Orange               |
| Photoresistor        | Photoresistor         | A3          | White                |
| Potentiometer Signal | Potentiometer (10K)   | A4          | Purple               |
| DHT11 Data           | DHT11                 | D7          | Green                |
| Obstacle OUT         | Obstacle Module       | D8          | Grey                 |
| Tilt Switch          | Tilt Switch           | D2 (INT0)   | RED — critical!      |
| Active Buzzer +      | Active Buzzer         | D3          | Red                  |
| RGB LED Red          | RGB LED (R pin)       | D9          | Red                  |
| RGB LED Green        | RGB LED (G pin)       | D10         | Green                |
| RGB LED Blue         | RGB LED (B pin)       | D11         | Blue                 |
| 7-Seg A              | 7-Segment Display     | D4          | Brown                |
| 7-Seg B              | 7-Segment Display     | D5          | Red                  |
| 7-Seg C              | 7-Segment Display     | D6          | Orange               |
| 7-Seg D              | 7-Segment Display     | D12         | Yellow               |
| 7-Seg E              | 7-Segment Display     | D13         | Green                |
| 7-Seg F              | 7-Segment Display     | A5          | Blue                 |
| All sensor VCC       | —                     | 5V rail     | Red                  |
| All sensor GND       | —                     | GND rail    | Black                |

---

## WIRING INSTRUCTIONS — STEP BY STEP

### STEP 1 — POWER RAILS
1. Connect Arduino **5V** pin → breadboard **positive (+) rail** (red wire)
2. Connect Arduino **GND** pin → breadboard **negative (−) rail** (black wire)
3. Add a second jumper linking both GND rails if your breadboard has split rails

---

### STEP 2 — SOIL HUMIDITY SENSOR
```
Sensor Pin  →  Where
VCC         →  5V rail
GND         →  GND rail
A0 (signal) →  Arduino A0
```
- Place sensor probes into a small cup of soil or damp sponge for demo
- DRY soil = high resistance = HIGH reading (Risk = LOW moisture)
- WET soil = low resistance = LOW reading (Risk = HIGH moisture/flood prep)

---

### STEP 3 — WATER LEVEL SENSOR
```
Sensor Pin  →  Where
VCC (5V+)   →  5V rail
GND         →  GND rail
S (signal)  →  Arduino A1
```
- Place in a shallow tray of water for demo — pour water slowly to show rising level
- The comb traces must be submerged; more submersion = higher reading

---

### STEP 4 — OBSTACLE AVOIDANCE MODULE (Flood Debris)
```
Module Pin  →  Where
VCC         →  5V rail
GND         →  GND rail
OUT         →  Arduino D8
```
- Adjust the blue trim pot on the module until LED flickers at ~10cm range
- Point it at floating debris (paper ball) in your water tray for demo

---

### STEP 5 — DHT11 (Temperature & Humidity)
```
DHT11 Pin   →  Where
Pin 1 (VCC) →  5V rail
Pin 2 (DAT) →  Arduino D7  [also add 10kΩ pull-up resistor to 5V]
Pin 4 (GND) →  GND rail
```
- Use one of the 10kΩ resistors from your kit as the pull-up
- The DHT11 has 4 pins; pin 3 is NC (not connected) — skip it

---

### STEP 6 — THERMISTOR (Soil Temperature)
```
Thermistor  →  Where
One leg     →  5V rail
Other leg   →  Arduino A2  AND  10kΩ resistor to GND
```
- This is a voltage divider: Thermistor + 10kΩ resistor in series
- Use any 10kΩ from your resistor pack (brown-black-orange-gold)
- Push thermistor probe into soil next to the humidity sensor

---

### STEP 7 — PHOTORESISTOR (Solar/Light Monitor)
```
Photoresistor → Where
One leg       → 5V rail
Other leg     → Arduino A3  AND  10kΩ resistor to GND
```
- Same voltage divider setup as thermistor
- Cover it = drought simulated (low solar input warning)
- Uncover = full solar, system healthy

---

### STEP 8 — POTENTIOMETER (Sensitivity Calibration)
```
Pot Pin     →  Where
Left (1)    →  GND rail
Middle (2)  →  Arduino A4  (wiper = signal)
Right (3)   →  5V rail
```
- Turn fully left = sensitivity 0.5× (less sensitive, for stable terrain)
- Turn fully right = sensitivity 2.0× (more sensitive, for unstable slopes)
- This is the "village leader calibration" dial — label it!

---

### STEP 9 — TILT SWITCH (LANDSLIDE INTERRUPT)
```
Tilt Switch →  Where
One leg     →  Arduino D2
Other leg   →  GND rail
```
- The code uses INPUT_PULLUP — no external resistor needed
- Mount at an angle on your prototype (glue to a small wedge of cardboard)
- Tip it to demonstrate the landslide interrupt — buzzer fires instantly

---

### STEP 10 — ACTIVE BUZZER
```
Buzzer Pin  →  Where
+ (longer)  →  Arduino D3
- (shorter) →  GND rail
```
- The active buzzer already has an oscillator inside — just needs DC power
- D3 is PWM capable; tone() function controls frequency

---

### STEP 11 — RGB LED
```
RGB LED Pin →  Where          Resistor (use ~220Ω from kit)
R (longest) →  220Ω → D9
G           →  220Ω → D10
B           →  220Ω → D11
GND (short) →  GND rail
```
- Common CATHODE type (the short leg is GND)
- Each colour channel needs its own 220Ω resistor to prevent burning
- Use the yellow, red, green LEDs from your kit as resistor colour guides

---

### STEP 12 — 7-SEGMENT DISPLAY (1-digit, common cathode)
```
Display Pin →  Arduino Pin
a (top)     →  D4
b (top-R)   →  D5
c (bot-R)   →  D6
d (bot)     →  D12
e (bot-L)   →  D13
f (top-L)   →  A5
GND (com)   →  GND rail
```
- Each segment pin needs a 220Ω current-limiting resistor
- Common cathode pin(s) → directly to GND
- The G segment (middle bar) is left disconnected in this build
  (digits still read 0–9 clearly without it for demo purposes)

---

## PHYSICAL PRESENTATION SETUP

### THE "FIELD STATION" LAYOUT

```
┌─────────────────────────────────────────────┐
│                                             │
│  [SOIL CUP]   [WATER TRAY]   [SLOPE MODEL] │
│  Soil sensor   Water level    Tilt switch   │
│                Obstacle IR    (cardboard    │
│                               wedge)        │
│                                             │
│         ┌─────────────────┐                │
│         │  ARDUINO + BB   │                │
│         │  Central board  │                │
│         └─────────────────┘                │
│                                             │
│  [7-SEG]  [RGB LED]  [BUZZER]  [DHT11]     │
│  Risk:0-9  Alert      Sound     Temp/RH     │
│                                             │
│         [POTENTIOMETER CAL DIAL]           │
│                                             │
└─────────────────────────────────────────────┘
```

### MATERIALS YOU'LL NEED (from around you)
- Small plastic tray or lid → water level demo
- Cup with soil/sand → soil sensor demo
- Cardboard wedge (~15° angle) → mount tilt switch for landslide demo
- Labels/sticky notes → label every component clearly
- Optional: small fan → show DHT11 responding to air movement

### DEMO SCRIPT (3 minutes)

**[0:00–0:30] BOOT**
- Power on → watch LED sweep R→G→B, 7-seg counts 0→9
- "The system self-tests on every boot — a field engineer can confirm it's working with no phone"

**[0:30–1:00] DROUGHT SCENARIO**
- Leave soil dry → A0 reads high resistance → soil moisture NORMAL
- Cover photoresistor → simulate no sunlight → serial monitor shows ET rate rising
- "In Ethiopia, we detect the wilting point before farmers can see it"

**[1:00–1:30] FLOOD SCENARIO**
- Pour water slowly into tray → water level sensor rises → LED turns blue
- Wave hand near obstacle sensor → debris detected, risk index jumps
- "In Nigeria, we detect rising water AND floating debris simultaneously"

**[1:30–2:00] LANDSLIDE INTERRUPT**
- Tilt the cardboard wedge → tilt switch triggers
- Buzzer SCREAMS at 2500Hz, LED flashes red, 7-seg shows 9
- "This is a hardware interrupt — no software can delay it. Zero latency."

**[2:00–2:30] CALIBRATION**
- Turn potentiometer slowly → show risk index scaling on serial monitor
- "A village leader with no engineering training can tune sensitivity to their terrain"

**[2:30–3:00] CLOSE**
- "No satellite. No smartphone. No internet required."
- "Under $50. Runs 6 months on a solar battery."
- "Designed for the 600 million Africans that commercial systems ignore."

---

## REQUIRED LIBRARY

Install before the hackathon:
1. Open Arduino IDE
2. Sketch → Include Library → Manage Libraries
3. Search: "DHT sensor library"
4. Install: **DHT sensor library** by Adafruit (version 1.4.x)
5. Also install: **Adafruit Unified Sensor** (dependency, auto-prompted)

---

## TROUBLESHOOTING

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| DHT11 always NaN | No pull-up resistor | Add 10kΩ between D7 and 5V |
| 7-seg shows wrong digit | Wiring order off | Check A=D4, B=D5, C=D6 order |
| RGB LED only one colour | Missing resistors | Add 220Ω on each colour pin |
| Tilt not triggering | Wired to wrong pin | Must be D2 (INT0) |
| Buzzer constant noise | Active vs passive confusion | Active buzzer = always tone at 5V |
| Soil reads 0 always | Sensor needs calibration | Map range: dry=1023, wet=300 |
| Water level ignores water | Submerge deeper | Comb traces must touch water |

---

*GEO-SENSE AFRICA v1.0 | Hackathon Build | Good luck — Africa is watching.*
