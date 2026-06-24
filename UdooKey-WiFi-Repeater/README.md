# Udoo Key Wi-Fi Repeater Node

This variant targets the **[Udoo Key](http://www.udoo.org/docs-key/Introduction/Introduction.html)** development board — a dual-MCU board that pairs an **ESP32** (Wi-Fi / Bluetooth / networking) with an **RP2040** (GPIO / ADC / PWM) on a single PCB, connected by an on-board UART bridge.

For the single-MCU builds see [`ESP32-WiFi-Repeater/`](../ESP32-WiFi-Repeater/) and [`ESP32-C3-WiFi-Repeater/`](../ESP32-C3-WiFi-Repeater/).

## Architecture

```
  [ Upstream Wi-Fi ] ─── STA ─── [ ESP32 ]─── AP+NAT ──► [ Your devices ]
                                      │
                                   UART bridge
                                   9600 baud
                                      │
                                  [ RP2040 ]
                                      │
                         ┌────────────┴────────────┐
                   2× I2C sensors         4× digital/PWM outputs
                   1× probe (DS18B20/DHT)
                   2× analog inputs
                   2× digital inputs
```

- **ESP32** handles networking: STA + AP with NAT (true repeater), MQTT publish/subscribe, UART receive/send.
- **RP2040** handles all I/O: reads sensors and inputs every 10 s, sends a JSON packet to the ESP32, receives and applies output commands.

## Key differences from the single-MCU variants

| | Classic / C3 | **Udoo Key** |
|--|--|--|
| Configuration | Captive-portal web page (no re-flashing) | **Captive-portal web page** (no re-flashing) |
| I2C sensors | 1 | **2** (independent type and address per device) |
| Inputs | 2 digital + 2 analog | 2 analog + 2 digital (on RP2040) |
| Outputs | 2 relay/SSR | **4** digital / PWM (on RP2040) |
| Home Assistant discovery | Auto (retained MQTT discovery) | **Auto** (retained MQTT discovery) |
| NTP clock schedules | ✅ | ✅ |
| Web portal | ✅ | ✅ |

## Sketches

| Folder | Board | What to open in the IDE |
|--------|-------|------------------------|
| [`esp32/RepeaterNodeESP32/`](esp32/RepeaterNodeESP32/) | ESP32 Dev Module | `RepeaterNodeESP32.ino` |
| [`rp2040/RepeaterNodeRP2040/`](rp2040/RepeaterNodeRP2040/) | Raspberry Pi Pico (RP2040) | `RepeaterNodeRP2040.ino` |

Flash **both** sketches — one to each MCU on the same board.

## Quick start

### ESP32 side

1. Open `RepeaterNodeESP32.ino` in Arduino IDE, select **ESP32 Dev Module**, upload.
2. Libraries needed: **PubSubClient**, **ArduinoJson v7**.  
   (`WebServer`, `DNSServer`, `Preferences` ship with the ESP32 core.)
3. On first boot, join Wi-Fi **UdooKey-AP** / **repeater1** and browse to **http://192.168.4.1** to configure Wi-Fi credentials, MQTT broker, NTP timezone, and clock schedules.  
   Alternatively, edit [`esp32/RepeaterNodeESP32/config.h`](esp32/RepeaterNodeESP32/config.h) with your defaults before flashing.

### RP2040 side

1. Edit the **CONFIGURATION** block at the top of `RepeaterNodeRP2040.ino`:
   ```c
   #define I2C_A_TYPE  I2C_BME280   // or I2C_BMP280, I2C_BMP180, I2C_NONE
   #define I2C_A_ADDR  0x76
   #define I2C_B_TYPE  I2C_NONE
   #define PROBE_TYPE  PROBE_DHT22  // or PROBE_DS18B20, PROBE_DHT11, PROBE_NONE
   ```
2. Open `RepeaterNodeRP2040.ino`, select **Raspberry Pi Pico**, upload.
3. Libraries needed: only install what matches your `#define` choices (see comments in sketch).

Board manager URL for RP2040: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`

## Pin maps

### ESP32 (fixed by Udoo Key PCB)

| Signal | GPIO |
|--------|------|
| UART RX (from RP2040) | 22 |
| UART TX (to RP2040) | 19 |
| Blue LED | 32 |
| Yellow LED | 33 |

### RP2040 (defaults, adjustable in sketch)

| Channel | GPIO | Notes |
|---------|------|-------|
| I2C SDA | GP4 | Both I2C sensors on the same bus |
| I2C SCL | GP5 | |
| Probe | GP16 | DS18B20 / DHT; add 4.7 kΩ pull-up to 3V3 |
| Analog input 1 | GP26 | ADC-capable (12-bit, 0–4095) |
| Analog input 2 | GP27 | ADC-capable |
| Digital input 1 | GP14 | Active-low with internal pull-up |
| Digital input 2 | GP15 | Active-low with internal pull-up |
| Output 1 | GP17 | Digital or PWM (0–255) |
| Output 2 | GP18 | |
| Output 3 | GP19 | |
| Output 4 | GP20 | |
| UART RX (from ESP32) | GP0 | On-board bridge |
| UART TX (to ESP32) | GP1 | On-board bridge |

> Only **GP26**, **GP27**, **GP28** are ADC-capable on the RP2040.

## UART protocol

Lines are newline-terminated, 9600 baud on the on-board UART bridge.

| Direction | Format | Example |
|-----------|--------|---------|
| RP2040 → ESP32 | `S:{json}\n` | `S:{"ia":{"t":23.40,"h":65.20,"p":1013.25},"an":[512,1024],"di":[0,1]}` |
| ESP32 → RP2040 | `C:{json}\n` | `C:{"o":0,"v":255}` — output 1 fully on |

JSON keys: `ia`/`ib` = I2C device A/B, `pr` = probe, `an` = analog array, `di` = digital array. Within each device: `t` = °C, `h` = % RH, `p` = hPa.

## MQTT topics

All under `<TOPIC_BASE>/` (default `udookey/`, set in `config.h`).

| Topic | Notes |
|-------|-------|
| `status` | `online` / `offline` (retained LWT) |
| `sensor/i2c_a/temperature` | °C — only published if sensor A is enabled and read succeeds |
| `sensor/i2c_a/humidity` | % RH (BME280 only) |
| `sensor/i2c_a/pressure` | hPa |
| `sensor/i2c_b/…` | Same fields for sensor B |
| `sensor/probe/temperature` | °C |
| `sensor/probe/humidity` | % RH (DHT11/DHT22 only) |
| `sensor/analog/1`, `.../2` | Raw 12-bit ADC value (0–4095) |
| `input/1/state`, `.../2/state` | `ON` / `OFF` (retained) |
| `output/1/state` … `output/4/state` | `ON` / `OFF` (retained, updated on command) |
| `output/1/set` … `output/4/set` | **Subscribe** — `ON` \| `OFF` \| `0–255` |

## Libraries summary

| Library | ESP32 | RP2040 |
|---------|-------|--------|
| PubSubClient | ✅ required | — |
| ArduinoJson v7 | ✅ required | ✅ required |
| Adafruit BME280 + Unified Sensor | — | if `I2C_BME280` |
| Adafruit BMP280 + Unified Sensor | — | if `I2C_BMP280` |
| Adafruit BMP085 + Unified Sensor | — | if `I2C_BMP180` |
| DHT sensor library + Unified Sensor | — | if `PROBE_DHT11/DHT22` |
| OneWire + DallasTemperature | — | if `PROBE_DS18B20` |
