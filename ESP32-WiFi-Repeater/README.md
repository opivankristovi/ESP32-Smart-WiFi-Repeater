# ESP32 Wi-Fi Repeater — Classic ESP32 Build

This sketch targets a standard **ESP32 Dev Module** (Xtensa dual-core). For the ESP32-C3 SuperMini build see [`ESP32-C3-WiFi-Repeater/`](../ESP32-C3-WiFi-Repeater/).

Full documentation is in the [root README](../README.md).

## Board & toolchain

- **Board:** ESP32 Dev Module (or any standard ESP32 board with 4 MB flash)
- **Arduino IDE** with the esp32 by Espressif Systems board package
- **Target in IDE:** *Tools → Board → ESP32 Arduino → ESP32 Dev Module*
- **Compiles on:** Arduino-ESP32 core 2.x and 3.x (NAPT API is `#if`-switched)

## Pin map

| Channel | GPIO | Notes |
|---------|------|-------|
| I2C SDA | 21 | BME280 / BMP280 / BMP180 |
| I2C SCL | 22 | |
| Probe (DS18B20 / DHT) | 4 | 4.7 kΩ pull-up to 3V3 required |
| Analog input 1 | 34 | ADC1, input-only |
| Analog input 2 | 35 | ADC1, input-only |
| Touch / button input 1 | 32 | T9 — digital button or capacitive touch pad |
| Touch / button input 2 | 33 | T8 — digital button or capacitive touch pad |
| Relay / SSR output 1 | 26 | Active-high or active-low (selectable) |
| Relay / SSR output 2 | 27 | |

> **Analog pins must be on ADC1 (GPIO 32–39).** ADC2 pins read garbage while Wi-Fi is active.

## What makes this build distinct

- **Capacitive touch inputs** — GPIO 32/33 support `touchRead()`, so you can use a bare wire or metal pad as a touch-sensitive input (no button needed). The portal shows a live value to help you dial in the threshold.
- **Separate analog + digital channels** — two dedicated analog inputs (GPIO 34/35) and two separate digital/touch inputs (GPIO 32/33), for a total of 4 I/O channels.
- **More flash/RAM headroom** than the C3, comfortable for the full feature set.

## Required libraries

PubSubClient · Adafruit BME280 · Adafruit BMP280 Library · Adafruit BMP085 Library · DHT sensor library · Adafruit Unified Sensor · OneWire · DallasTemperature · **ArduinoJson v7** (v6 will not compile).

Core-shipped: `WiFi`, `WebServer`, `DNSServer`, `Preferences`, `Wire`.
