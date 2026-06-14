/*
 * ESP32-C3 WiFi Repeater + sensor/MQTT/relay edge node.
 * Tuned for the ESP32-C3 SuperMini board.
 *
 * - Concurrent AP+STA with NAPT: devices on the repeater AP are routed out
 *   through the upstream network (a true repeater).
 * - Configured entirely over WiFi via a captive-portal setup page; settings
 *   persist in NVS. Factory reset available from the page.
 * - Optional I2C sensor (BME280/BMP280/BMP180), probe (DS18B20/DHT11/DHT22),
 *   4 unified inputs (each digital button or analog sensor), published to
 *   MQTT with per-metric alert thresholds.
 * - Two relay/SSR outputs driven by timer / sensor threshold / button rules,
 *   and controllable + reportable over MQTT.
 *
 * This .ino is orchestration only; the logic lives in the module tabs:
 *   config, net, sensors, relays, mqtt, web_portal (+ web_page.h).
 *
 * Default repeater AP: ESP32-repeaterAP / 12345678 (change it on the page).
 */

#include "config.h"
#include "net.h"
#include "timekeeper.h"
#include "sensors.h"
#include "inputs.h"
#include "relays.h"
#include "mqtt.h"
#include "web_portal.h"

static unsigned long lastPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(200);

  config.load();      // settings from NVS (defaults if first boot)
  TimeKeeper::begin();// apply timezone (SNTP starts once upstream is up)
  Net::begin();       // AP + STA + NAPT + captive DNS
  Sensors::begin();   // init enabled sensor buses
  Inputs::begin();    // configure digital/analog input pins
  Relays::begin();    // configure relay pins + initial state
  Mqtt::begin();      // configure client (connects in loop)
  WebPortal::begin(); // routes + web server
}

void loop() {
  Net::loop();         // DNS pump + upstream reconnect watchdog + NAPT
  TimeKeeper::loop();  // start NTP sync once the upstream link is up
  WebPortal::handle(); // serve the setup portal
  Sensors::tick();     // non-blocking DS18B20 conversion
  Inputs::update();    // sample digital/analog inputs (before relay rules)
  Relays::update();    // evaluate relay rules every tick
  Mqtt::loop();        // maintain MQTT connection

  // Publish relay state changes immediately (rules, button, MQTT command).
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (Relays::consumeChanged(i)) Mqtt::publishRelayState(i, Relays::getState(i));
  }

  // Publish input state changes (so HA can switch other devices off them).
  for (int i = 0; i < NUM_INPUTS; i++) {
    if (Inputs::consumeChanged(i)) Mqtt::publishInputState(i, Inputs::getState(i));
  }

  // Periodic sensor publish.
  unsigned long intervalMs = (unsigned long)config.mqtt.publishIntervalSec * 1000UL;
  if (millis() - lastPublish >= intervalMs) {
    lastPublish = millis();
    Readings r = Sensors::readAll();
    Mqtt::publishReadings(r);
  }
}
