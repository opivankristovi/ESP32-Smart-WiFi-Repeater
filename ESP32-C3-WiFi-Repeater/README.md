# ESP32-C3 Wi-Fi Repeater — SuperMini Build

This sketch targets the **ESP32-C3 SuperMini** (RISC-V single-core, 22 × 18 mm). For the classic ESP32 Dev Module build see [`ESP32-WiFi-Repeater/`](../ESP32-WiFi-Repeater/).

Full documentation is in the [root README](../README.md).

## Board & toolchain

- **Board:** ESP32-C3 SuperMini
- **Arduino IDE** with the esp32 by Espressif Systems board package
- **Target in IDE:** *Tools → Board → ESP32 Arduino → ESP32C3 Dev Module*
- **Compiles on:** Arduino-ESP32 core 2.x and 3.x (NAPT API is `#if`-switched)

## Pin map

| Channel | GPIO | Notes |
|---------|------|-------|
| I2C SDA | 6 | BME280 / BMP280 / BMP180 |
| I2C SCL | 7 | |
| Probe (DS18B20 / DHT) | 5 | ADC2 — fine for digital use; 4.7 kΩ pull-up to 3V3 required |
| Input 1 | 0 | ADC1 — digital button **or** analog sensor |
| Input 2 | 1 | ADC1 — digital button **or** analog sensor |
| Input 3 | 3 | ADC1 — digital button **or** analog sensor |
| Input 4 | 4 | ADC1 — digital button **or** analog sensor |
| Relay / SSR output 1 | 10 | Active-high or active-low (selectable) |
| Relay / SSR output 2 | 20 | |

> **GPIO 2 is skipped** — it is a SPI-boot strapping pin and must be left floating at boot.  
> **ADC1 only for analog inputs** — GPIO 0/1/3/4 are ADC1 and work correctly while Wi-Fi is active. GPIO 5 is ADC2 (unusable for analog) but is fine for the digital probe bus.

## What makes this build distinct

- **4 unified inputs** — each of the 4 input channels can be individually configured as either a **digital push-button** (published as an MQTT `binary_sensor` / HA binary_sensor, can toggle a relay) or an **analog sensor** (ADC reading scaled to raw/percent/voltage, published as an MQTT sensor / HA sensor with optional alert thresholds). There is no separate analog-only channel.
- **Configurable button→relay mapping** — in Button toggle mode, each relay independently selects which of the 4 inputs drives it (instead of the fixed relay-N-follows-input-N mapping of the classic build).
- **No capacitive touch** — the ESP32-C3 has no touch hardware. Input type is digital or analog only.
- **Compact footprint** — the SuperMini is tiny (22 × 18 mm), making it easy to fit in tight enclosures.

## Required libraries

PubSubClient · Adafruit BME280 · Adafruit BMP280 Library · Adafruit BMP085 Library · DHT sensor library · Adafruit Unified Sensor · OneWire · DallasTemperature · **ArduinoJson v7** (v6 will not compile).

Core-shipped: `WiFi`, `WebServer`, `DNSServer`, `Preferences`, `Wire`.
