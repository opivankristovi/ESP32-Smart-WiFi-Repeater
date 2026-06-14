# CLAUDE.md — ESP32-C3 Fork

This file provides guidance to Claude Code when working with this fork.

## What this is

A fork of the ESP32 Smart WiFi Repeater project, tuned for the **ESP32-C3 SuperMini** board. Same functionality — true NAT Wi-Fi repeater (concurrent AP+STA with NAPT), IoT edge node with MQTT and Home Assistant auto-discovery — but with a pin map, I/O model, and web portal matched to the C3's smaller GPIO set and RISC-V single-core architecture.

Key differences from the original ESP32 build:

- **No capacitive touch** — the C3 has no touch hardware.
- **4 unified inputs** — each individually configurable as a digital push-button OR an analog sensor (replaces the original's separate 2× digital/touch + 2× analog design).
- **Configurable button→relay mapping** — each relay in Button mode can select which of the 4 inputs drives it (via `buttonInput`).
- **C3 SuperMini pin map** — GPIO0-4 on ADC1 for inputs, GPIO6/7 for I2C, GPIO5 for probe, GPIO10/20 for relays.

## Build & flash

- **Toolchain:** Arduino IDE with the ESP32 board package. Target **ESP32C3 Dev Module**.
- **Open** `ESP32-C3-WiFi-Repeater.ino` — all `.h`/`.cpp` files plus `web_page.h` must stay in this one sketch folder.
- **Required libraries** (Arduino Library Manager): PubSubClient, Adafruit BME280, Adafruit BMP280 Library, Adafruit BMP085 Library (covers BMP180), DHT sensor library (Adafruit), Adafruit Unified Sensor, OneWire, DallasTemperature, **ArduinoJson v7** (v6 will not compile). `WiFi`, `WebServer`, `DNSServer`, `Preferences`, `Wire` ship with the core.
- **Serial monitor:** 115200 baud.
- **Compiles on both Arduino-ESP32 core 2.x and 3.x** — the NAPT API differs and is `#if`-switched in `net.cpp`.
- **First-run access:** join Wi-Fi `ESP32-repeaterAP` / `12345678`, captive portal pops up.

## Pin map (ESP32-C3 SuperMini)

```
PIN_I2C_SDA   = 6       I2C data (BME280/BMP280/BMP180)
PIN_I2C_SCL   = 7       I2C clock
PIN_ONEWIRE   = 5       probe bus (DS18B20/DHT11/DHT22) — on ADC2, fine for digital
PIN_INPUT[4]  = {0, 1, 3, 4}   all ADC1 — work as analog while WiFi is active
                                GPIO2 skipped (SPI-boot strapping pin)
PIN_RELAY[2]  = {10, 20}
```

10 GPIOs used out of 13 available on the SuperMini (GPIO0-10, 20-21).

**ADC constraint:** only ADC1 (GPIO0-4) reads correctly while Wi-Fi is active. ADC2 (GPIO5) is unusable for analog but fine for the digital probe bus.

## Architecture

Same module structure as the parent project. `.ino` is orchestration only; logic lives in namespaced modules:

| Module | Namespace / type | Responsibility |
|--------|------------------|----------------|
| `config` | `Config config` | Settings structs + NVS JSON persistence + factory reset |
| `net` | `Net::` | AP+STA, NAPT, reconnect watchdog, captive DNS |
| `timekeeper` | `TimeKeeper::` | SNTP time sync + POSIX timezone from config |
| `sensors` | `Sensors::`, `Readings` | I2C sensor + probe + analog input sampling + threshold eval |
| `inputs` | `Inputs::` | 4 unified inputs (digital button or analog); debounced; `edge(i)` drives relays |
| `relays` | `Relays::` | 2 outputs driven by timer/sensor/button/schedule rules + MQTT override |
| `mqtt` | `Mqtt::` | PubSubClient; publish readings/alerts/state, subscribe relay commands, HA discovery |
| `web_portal` | `WebPortal::` | HTTP server: tabbed setup page, config form handlers, live readings |
| `web_page.h` | — | HTML/CSS/JS page shell (PROGMEM) |

### Unified input model

Each of the 4 inputs is configured as either `INPUT_DIGITAL` (debounced push-button, published as HA binary_sensor) or `INPUT_ANALOG` (ADC reading scaled to raw/percent/voltage, published as HA sensor). The `InputConfig` struct holds both digital-specific fields (`activeLow`) and analog-specific fields (`scale`, `rawMin`, `rawMax`, `thr`).

### Configurable button→relay mapping

`RelayConfig.buttonInput` (0-3) selects which input drives the relay in `RELAY_BUTTON` mode. The original assumed relay N follows input N; this fork lets any input drive any relay.

### Two-namespace persistence split

Same as the parent project:
- **`"repeater"`** — Wi-Fi/AP credentials only (owned by `net.cpp`).
- **`"settings"`** — everything else as one JSON blob (owned by `config.cpp`).

### MetricSource values are persisted

The integer values of `MetricSource` are stored in the relay config blob. Only **append** new sources; never reorder or reuse values. Current layout: `SRC_NONE=0`, `SRC_I2C_TEMP=1`, `SRC_I2C_HUM=2`, `SRC_I2C_PRES=3`, `SRC_PROBE_TEMP=4`, `SRC_INPUT1=5..SRC_INPUT4=8`, `SRC_PROBE_HUM=9`.

### Reboot-on-save model

The web portal applies most config changes by rebooting. Don't try to make sensor/relay/MQTT edits apply without a reboot unless you also re-run the relevant `begin()`.

## MQTT topic tree

Base is `<baseTopic>/<clientId>` (clientId defaults to `esp32-<chipid>`).

```
<base>/<clientId>/
  status                           (retained: online/offline LWT)
  sensor/i2c/{temperature,pressure,humidity}
  sensor/probe/{temperature,humidity}
  input/{1..4}/state               (retained: ON/OFF — digital inputs)
  input/{1..4}/value               (analog readings)
  alert/i2c/{temperature,humidity,pressure}
  alert/probe/{temperature,humidity}
  alert/input/{1..4}               (analog threshold alerts)
  relay/{1,2}/state                (retained: ON/OFF)
  relay/{1,2}/set                  (subscribe: ON/OFF/TOGGLE)
  diag/{rssi,uptime,heap}
```

## Conventions

- Modules use C++ namespaces; cross-module data flows through `Readings` or the global `config`.
- String→struct copies use `strlcpy`/`copyStr` with `sizeof(dst)`.
- HTML is emitted as concatenated C++ strings / PROGMEM.
- Loop bounds use `NUM_INPUTS` (4) and `NUM_RELAYS` (2) constants.
- JSON config fields use `key | default` for backward-safe deserialization.
- I2C config uses legacy JSON key `"bme"`, probe uses `"ds"` — for storage stability.
