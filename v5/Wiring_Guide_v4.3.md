# GEO-SENSE AFRICA v4.3 — COMPLETE WIRING GUIDE
## ESP32 Master + Arduino Uno Slave | Core Essentials System

**Version:** 4.3 (Streamlined Profile)
**Last Updated:** March 2026
**Difficulty:** Intermediate
**Estimated Time:** 1.5-2 hours

---

## 📋 TABLE OF CONTENTS

1. [System Overview](#system-overview)
2. [Components Checklist](#components-checklist)
3. [Tools Required](#tools-required)
4. [ESP32 Master Wiring](#part-a--esp32-master-wiring)
5. [Arduino Slave Wiring](#part-b--arduino-uno-slave-wiring)
6. [Inter-Board Connection](#part-c--inter-board-communication)
7. [Power Distribution](#part-d--power-distribution)
8. [Step-by-Step Assembly](#step-by-step-assembly-guide)
9. [Testing & Verification](#testing--verification)
10. [Troubleshooting](#troubleshooting)

---

## 🔍 SYSTEM OVERVIEW

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GEO-SENSE AFRICA v4.3                               │
│                 Dual-Board Multi-Hazard Early Warning System                │
│                                                                             │
│   ┌─────────────────────────┐         ┌─────────────────────────┐          │
│   │    ESP32 MASTER NODE    │         │   ARDUINO UNO SLAVE     │          │
│   │  ┌───────────────────┐  │         │  ┌───────────────────┐  │          │
│   │  │ WiFi Web Server   │  │         │  │ Water Level (A1)  │  │          │
│   │  │ OLED 128x64 I2C   │  │         │  │ Thermistor (A2)   │  │          │
│   │  │ LCD 1602 I2C      │  │         │  │ Tilt Switch (D2)  │  │          │
│   │  │ DHT11 (Air T/H)   │  │         │  │ Active Buzzer(D3) │  │          │
│   │  │ HC-SR04 (Flood)   │  │         │  │ Passive Buzzer(D11)│ │          │
│   │  │ PIR Motion        │  │         │  │ Potentiometer(A4) │  │          │
│   │  │ Soil Moisture     │  │         │  └───────────────────┘  │          │
│   │  │ SG90 Servo        │  │         └───────────┬─────────────┘          │
│   │  │ Relay (Siren)     │  │                     │                         │
│   │  │ 4x Status LEDs    │  │         ┌───────────┴─────────────┐          │
│   │  │ Touch Sensor      │  │         │    SERIAL TELEMETRY     │          │
│   │  │ Obstacle IR       │  │         │  9600 Baud (Arduino)    │          │
│   │  └───────────────────┘  │         │  115200 Baud (ESP32)    │          │
│   └───────────┬─────────────┘         └─────────────────────────┘          │
│               │                                   │                         │
│               └───────── Serial2 (UART) ──────────┘                         │
│                    TX2 (GPIO17) ◄──► RX (D0)                                │
│                    RX2 (GPIO16) ◄──► TX (D1) [via voltage divider]          │
│                         GND ────────── GND (COMMON)                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 COMPONENTS CHECKLIST

### Main Boards
| Item | Quantity | Notes |
|------|----------|-------|
| ESP32 Dev Module (DOIT DEVKIT V1) | 1 | 30-pin or 36-pin |
| Arduino Uno R3 | 1 | ATmega328P |

### Displays
| Item | Quantity | I2C Address | Notes |
|------|----------|-------------|-------|
| OLED 128x64 SSD1306 | 1 | 0x3C | High-Density Master Dashboard |
| LCD 1602 with I2C backpack | 1 | 0x27 | Secondary Status Display |

### Sensors - ESP32 Side
| Item | Quantity | Notes |
|------|----------|-------|
| DHT11 Temperature/Humidity | 1 | Air conditions |
| HC-SR04 Ultrasonic | 1 | River level distance |
| HC-SR501 PIR Motion | 1 | Human/Animal movement |
| Capacitive Soil Moisture v1.2 | 1 | Drought monitoring |
| IR Obstacle Avoidance | 1 | Debris detection |
| TTP223B Touch Sensor | 1 | Alert Acknowledge (ACK) |

### Sensors - Arduino Side
| Item | Quantity | Notes |
|------|----------|-------|
| Water Level Sensor | 1 | Submersible probe (A1) |
| Thermistor 10k NTC | 1 | Soil temperature (A2) |
| Potentiometer 10k | 1 | Terrain calibration (A4) |
| Tilt Switch SW-520D | 1 | Landslide interrupt (D2) |

### Outputs
| Item | Quantity | Notes |
|------|----------|-------|
| SG90 Micro Servo | 1 | Warning flag mechanism |
| 1-Channel Relay Module | 1 | External siren/lamp control |
| Active Buzzer 5V | 1 | Continuous alarm (DC) |
| Passive Buzzer 5V | 1 | Frequency alerts (tone) |
| LEDs (G, B, Y, R) | 4 | Status/Hazard indicators |

---

## PART A — ESP32 MASTER WIRING

### Pin Reference Table (v4.3)

| GPIO | Function | Type | Notes |
|------|----------|------|-------|
| GPIO21 | I2C SDA | Output | Shared LCD/OLED |
| GPIO22 | I2C SCL | Output | Shared LCD/OLED |
| GPIO4 | DHT11 DATA | Digital | 10k pull-up |
| GPIO5 | HC-SR04 TRIG | Output | |
| GPIO18 | HC-SR04 ECHO | Input | **1kΩ+2kΩ Divider** |
| GPIO19 | PIR OUT | Input | |
| GPIO13 | Servo PWM | Output | |
| GPIO12 | Relay IN | Output | Active LOW |
| GPIO14 | Touch SIG | Input | |
| GPIO25 | Green LED | Output | Via 220Ω |
| GPIO2 | Blue LED | Output | Via 220Ω |
| GPIO27 | Yellow LED | Output | Via 220Ω |
| GPIO33 | Red LED | Output | Via 220Ω |
| GPIO36 | Soil Signal | ADC | |
| GPIO39 | Obstacle OUT | ADC | |
| GPIO16 | Serial2 RX | Input | From Arduino TX (Divider) |
| GPIO17 | Serial2 TX | Output | To Arduino RX |

---

### A1 — I2C DISPLAY BUS (OLED + LCD)

```
ESP32              OLED 0x3C              LCD 0x27
┌─────┐           ┌──────────┐          ┌──────────┐
│GPIO21├─SDA──────┤SDA       │──────────┤SDA       │
│GPIO22├─SCL──────┤SCL       │──────────┤SCL       │
│ 3.3V├─VCC───────┤VCC       │          │VCC       │◄──5V
│  GND├─GND───────┤GND       │──────────┤GND       │
└─────┘           └──────────┘          └──────────┘
```

---

### A2 — CORE SENSORS (DHT11, HC-SR04, PIR)

1. **DHT11:** VCC(3.3V), GND, DATA(GPIO4) + 10kΩ pull-up to 3.3V.
2. **HC-SR04:** VCC(5V), GND, TRIG(GPIO5), ECHO(GPIO18 via 1k/2k divider).
3. **PIR:** VCC(5V), GND, OUT(GPIO19).

---

### A3 — PERIPHERALS (Servo, Relay, Touch)

1. **Servo:** Brown(GND), Red(5V), Orange(GPIO13).
2. **Relay:** VCC(5V), GND, IN(GPIO12).
3. **Touch:** VCC(3.3V), GND, SIG(GPIO14).

---

## PART B — ARDUINO UNO SLAVE WIRING

### Pin Reference Table (v4.3)

| Pin | Function | Type | Notes |
|-----|----------|------|-------|
| A1 | Water Level | ADC | 0-700 Range |
| A2 | Thermistor | ADC | 10k NTC |
| A4 | Potentiometer | ADC | Calibration |
| D2 | Tilt Switch | Interrupt | INT0 |
| D3 | Active Buzzer | Output | |
| D11 | Passive Buzzer | PWM | tone() |
| D0 | Serial RX | UART | From ESP32 TX |
| D1 | Serial TX | UART | To ESP32 RX (Divider) |

---

### B1 — CORE SENSORS (Water, Temp, Tilt)

1. **Water:** VCC(5V), GND, SIG(A1).
2. **Thermistor:** 5V -> 10k NTC -> A2 -> 10kΩ Resistor -> GND.
3. **Tilt Switch:** Pin1(D2), Pin2(GND). Internal pull-up used in code.
4. **Potentiometer:** Left(GND), Center(A4), Right(5V).

---

## PART C — INTER-BOARD COMMUNICATION

### Serial2 Connection (ESP32 ↔ Arduino)

```
ESP32 Master             Arduino Slave
┌─────────┐             ┌───────────┐
│GPIO17   │──TX2────────┤D0 (RX)    │
│         │             │           │
│GPIO16   │──RX2────┬───┤D1 (TX)    │
│         │        │   │           │
│         │     ┌──┴──┐│           │
│         │     │ 1kΩ ││           │
│         │     └──┬──┘│           │
│         │        ├────           │
│         │     ┌──┴──┐│           │
│         │     │ 2kΩ ││           │
│         │     └──┬──┘│           │
│         │        │   │           │
│GND      │────────┴───┤GND        │
└─────────┘             └───────────┘
```

**⚠️ CRITICAL:** Arduino TX (5V) must pass through the 1kΩ/2kΩ divider to reach ESP32 RX (3.3V).

---

## PART D — POWER DISTRIBUTION

1. **Common Ground:** Connect ESP32 GND and Arduino GND together.
2. **5V Rail:** Use for HC-SR04, PIR, Servo, Relay, Buzzers, and Arduino Sensors.
3. **3.3V Rail:** Use for DHT11, OLED, and Touch sensor.
4. **Stability:** If the servo causes resets, use a separate 5V supply for it, keeping GNDs common.

---

## STEP-BY-STEP ASSEMBLY

1. **Voltage Dividers:** Install the two required 1k/2k dividers (one for HC-SR04 Echo, one for Arduino TX).
2. **I2C Bus:** Wire the OLED and LCD to GPIO21/22.
3. **ESP32 Sensors:** Wire DHT11, HC-SR04, PIR, Soil, and Obstacle sensors.
4. **Arduino Sensors:** Wire Water, Thermistor, Potentiometer, and Tilt switch.
5. **Outputs:** Connect Servo, Relay, Buzzers, and the 4 Status LEDs (with 220Ω resistors).
6. **Inter-Connect:** Connect the Serial2 wires and the common ground.

---

## TESTING & VERIFICATION (v4.3 Telemetry)

1. **Power Up:** Open Arduino IDE Serial Monitor at **115200 baud**.
2. **System Heartbeat:** You should see `[SYS] WiFi: GeoSense-Africa | IP: 192.168.4.1 | Uptime: XXs` every 5 seconds.
3. **Data Sync:** You should see `[RX] Data Sync - W:XX.X T:XX.X S:XX.X Risk: XX` when packets arrive.
4. **Slave Debug:** If monitoring the Arduino directly (9600 baud), look for `[SLAVE]` summaries and `[TX]` packets.
5. **Dashboard:** Connect to the WiFi and visit `http://192.168.4.1` to see the high-density UI.

---

## TROUBLESHOOTING

- **No Serial Data:** Check the TX/RX crossover (TX goes to RX).
- **Garbage Text:** Ensure baud rates are 115200 (ESP32) and 9600 (Arduino).
- **OLED Blank:** Check I2C address (0x3C) and VCC (3.3V).
- **ESP32 Resetting:** Check for 5V signals on GPIO pins; ensure common ground is connected.

---

*GEO-SENSE AFRICA v4.3 | Core Essentials Wiring Guide | March 2026*
