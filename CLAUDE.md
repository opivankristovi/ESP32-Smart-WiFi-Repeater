# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single Arduino sketch for the **ESP32** that turns the board into a **true NAT Wi-Fi repeater** (concurrent AP+STA with NAPT, so AP clients route out to the upstream internet) plus an IoT edge node: reads BME280 / DS18B20 / 2× analog sensors, drives two relay/SSR outputs from local rules, publishes to MQTT, and auto-registers in Home Assistant. Everything is configured at runtime through a captive-portal web page — **nothing is hard-coded and no re-flashing is needed to change settings.**

There is no test suite, no build script, and no package manifest. This is firmware compiled and flashed from the Arduino IDE.

## Build & flash

- **Toolchain:** Arduino IDE with the ESP32 board package. Target e.g. *ESP32 Dev Module*.
- **Open** `ESP32-WiFi-Repeater/ESP32-WiFi-Repeater.ino` — all `.h`/`.cpp` files plus `web_page.h` must stay in that one sketch folder; the IDE compiles them together as module "tabs".
- **Required libraries** (Arduino Library Manager): PubSubClient, Adafruit BME280, Adafruit Unified Sensor, OneWire, DallasTemperature, **ArduinoJson v7** (v6 will not compile — uses `JsonDocument`). `WiFi`, `WebServer`, `DNSServer`, `Preferences`, `Wire` ship with the core.
- **Serial monitor:** 115200 baud.
- **Compiles on both Arduino-ESP32 core 2.x and 3.x** — the NAPT API differs between them and is `#if ESP_ARDUINO_VERSION_MAJOR`-switched in `net.cpp`. Any networking change must preserve both branches.
- **First-run access:** join Wi-Fi `ESP32-repeaterAP` / `12345678`, captive portal pops up (or browse `http://192.168.4.1`).

## Architecture

`.ino` is orchestration only: `setup()` calls each module's `begin()`, `loop()` pumps each module's tick. All real logic lives in namespaced modules, each a header (public interface) + cpp:

| Module | Namespace / type | Responsibility |
|--------|------------------|----------------|
| `config` | `Config config` (global) | All settings structs + NVS persistence + factory reset |
| `net` | `Net::` | AP+STA, NAPT (the repeating), reconnect watchdog, captive DNS |
| `sensors` | `Sensors::`, `Readings` | Init buses, non-blocking DS18B20 conversion, sample + threshold eval |
| `relays` | `Relays::` | Two outputs driven by timer/sensor/button rules + MQTT override |
| `mqtt` | `Mqtt::` | PubSubClient over the STA link; publish readings/alerts/relay state, subscribe to relay commands, HA discovery |
| `web_portal` | `WebPortal::` | HTTP server: setup page, all config form handlers, live-readings JSON, captive redirects, factory reset |
| `web_page.h` | — | The HTML/CSS page shell (PROGMEM strings) |

Data flows one direction through `loop()`: sensors are sampled into a `Readings` snapshot, which `Relays::update()` consults (via `Sensors::metricValue`) and `Mqtt::publishReadings()` ships. Relay state changes are drained each loop with `Relays::consumeChanged(i)` and published immediately.

### Two-namespace persistence split (important)

Settings live in **two separate NVS (`Preferences`) namespaces**, and this separation is deliberate:

- **`"repeater"`** — Wi-Fi/AP credentials only (`sta_ssid`, `sta_pass`, `ap_ssid`, `ap_pass`, `pw_changed`). Owned entirely by `net.cpp`; stored as discrete keys. This is "Phase 1" config the device needs just to come up and serve the portal.
- **`"settings"`** — everything else (MQTT, sensors, relays, button), serialized as **one JSON blob** under key `"json"` via ArduinoJson. Owned by `config.cpp` (`toJson`/`fromJson`).

`Config` does **not** hold Wi-Fi credentials. `Config::factoryReset()` clears **both** namespaces and reboots.

When extending the JSON config: add the field to the struct in `config.h`, then to **both** `toJson` (serialize) and `fromJson` (deserialize, always with a `| default` fallback so old/absent blobs still load), and surface it in the relevant `web_portal.cpp` form + save handler.

### Reboot-on-save model

The web portal applies most config changes by **rebooting**. `handleMqtt`/`handleSensors`/`handleRelays` call `config.save()` then `sendRebootNotice(...)`; `handleApConfig` does the same after `Net::saveApCreds`. This is intentional — pin assignments and bus init only happen in each module's `begin()`, so a reboot is how new pins/buses take effect. Wi-Fi *station* changes (`handleSaveWifi`) are the exception: they reconnect live without a reboot. Don't try to make sensor/relay/MQTT edits apply without a reboot unless you also re-run the relevant `begin()`.

## Hardware constraints that affect code

- **Analog pins must be ADC1 (GPIO 32–39).** ADC2 pins read garbage while Wi-Fi is active. Defaults are GPIO 34/35 (input-only). The fixed pin map is `config.h` (`PIN_*`).
- **DS18B20 conversion is slow**, so it's driven asynchronously via `Sensors::tick()` in the main loop — never block on it.
- **NAPT must actually come up** for the repeater function; `net.cpp` re-enables it whenever the STA link transitions to connected. Serial prints "NAPT enabled" on success.

## MQTT topic tree

Base is `<baseTopic>/<clientId>` (clientId defaults to `esp32-<chipid>` via `ensureClientId()`). Publishes: `.../status` (retained LWT online/offline), `.../sensor/<...>`, `.../diag/{rssi,uptime,heap}`, `.../alert/<metric>`, `.../relay/<1|2>/state` (retained). Subscribes: `.../relay/<1|2>/set` (`ON`/`OFF`/`TOGGLE`, gated by each relay's `allowMqtt`). HA discovery publishes retained config under `haPrefix` (default `homeassistant`); disabling a channel publishes an empty config to remove its entity.

## Conventions

- Each module exposes a small C++ namespace; cross-module state is passed by value (`Readings`) or via the single global `config`, not shared mutable globals.
- String→struct copies use `strlcpy`/`copyStr` with `sizeof(dst)` — keep that pattern for the fixed-size `char[]` fields.
- HTML is emitted as concatenated C++ strings / PROGMEM (`FPSTR`, `PAGE_HEAD`/`PAGE_FOOT`); there's no template engine.
