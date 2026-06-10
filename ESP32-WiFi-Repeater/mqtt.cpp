#include "mqtt.h"
#include "relays.h"
#include "inputs.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace Mqtt {

static WiFiClient wifiClient;
static PubSubClient client(wifiClient);

static unsigned long lastReconnect = 0;
static const unsigned long kReconnectEveryMs = 5000;

// Previous alert states, to publish only on transition.
// index: 0 i2c temp, 1 i2c hum, 2 i2c pres, 3 probe temp, 4 a1, 5 a2, 6 probe hum
static MetricState prevState[7];
static bool prevInit = false;

static String base() {
  return String(config.mqtt.baseTopic) + "/" + config.mqtt.clientId;
}

static const char* stateName(MetricState s) {
  return s == METRIC_HIGH ? "HIGH" : (s == METRIC_LOW ? "LOW" : "OK");
}

// ---- inbound: relay command topics -----------------------------------------
static void onMessage(char* topic, byte* payload, unsigned int len) {
  String t(topic);
  String b = base();
  for (int i = 0; i < 2; i++) {
    String cmd = b + "/relay/" + String(i + 1) + "/set";
    if (t != cmd) continue;
    if (!config.relays[i].allowMqtt) return;
    String msg;
    for (unsigned int k = 0; k < len; k++) msg += (char)payload[k];
    msg.trim();
    msg.toUpperCase();
    if (msg == "ON" || msg == "1" || msg == "TRUE")       Relays::setState(i, true);
    else if (msg == "OFF" || msg == "0" || msg == "FALSE") Relays::setState(i, false);
    else if (msg == "TOGGLE")                              Relays::toggle(i);
    return;
  }
}

void begin() {
  if (!config.mqtt.enabled || config.mqtt.host[0] == '\0') return;
  client.setServer(config.mqtt.host, config.mqtt.port);
  client.setBufferSize(1024);  // room for HA discovery payloads
  client.setCallback(onMessage);
}

// ---- Home Assistant MQTT discovery -----------------------------------------
// Adds the shared device block + availability so all entities group under one
// HA device and follow the LWT online/offline status.
static void addDeviceAndAvail(JsonDocument& doc) {
  doc["availability_topic"]    = base() + "/status";
  doc["payload_available"]     = "online";
  doc["payload_not_available"] = "offline";
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(config.mqtt.clientId);
  dev["name"]         = String("ESP32 Smart Repeater (") + config.mqtt.clientId + ")";
  dev["manufacturer"] = "Espressif";
  dev["model"]        = "ESP32 WiFi Repeater";
}

static String discTopic(const char* component, const String& key) {
  return String(config.mqtt.haPrefix) + "/" + component + "/" +
         config.mqtt.clientId + "/" + key + "/config";
}

static void pubDisc(const char* component, const String& key, JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  client.publish(discTopic(component, key).c_str(), payload.c_str(), true);
}

// Empty retained payload removes a previously-advertised entity from HA.
static void removeDisc(const char* component, const String& key) {
  client.publish(discTopic(component, key).c_str(), "", true);
}

static void sensorDisc(const String& key, const String& name,
                       const String& stateSubtopic, const char* devClass,
                       const String& unit, bool enabled) {
  if (!enabled) { removeDisc("sensor", key); return; }
  JsonDocument doc;
  doc["name"]        = name;
  doc["unique_id"]   = String(config.mqtt.clientId) + "_" + key;
  doc["state_topic"] = base() + stateSubtopic;
  if (devClass)        doc["device_class"]        = devClass;
  if (unit.length())   doc["unit_of_measurement"] = unit;
  addDeviceAndAvail(doc);
  pubDisc("sensor", key, doc);
}

static void relayDisc(int i) {
  String key  = "relay" + String(i + 1);
  String name = config.relays[i].name[0] ? String(config.relays[i].name)
                                         : ("Relay " + String(i + 1));
  String stateTopic = base() + "/relay/" + String(i + 1) + "/state";

  JsonDocument doc;
  doc["name"]        = name;
  doc["unique_id"]   = String(config.mqtt.clientId) + "_" + key;
  doc["state_topic"] = stateTopic;
  doc["payload_on"]  = "ON";
  doc["payload_off"] = "OFF";
  addDeviceAndAvail(doc);

  if (config.relays[i].allowMqtt) {
    doc["command_topic"] = base() + "/relay/" + String(i + 1) + "/set";
    pubDisc("switch", key, doc);
    removeDisc("binary_sensor", key);  // drop the read-only variant if present
  } else {
    pubDisc("binary_sensor", key, doc);
    removeDisc("switch", key);
  }
}

// Touch/button input -> read-only binary_sensor (so HA automations can react).
static void inputDisc(int i) {
  String key = "input" + String(i + 1);
  if (!config.inputs[i].enabled) { removeDisc("binary_sensor", key); return; }
  String name = config.inputs[i].name[0] ? String(config.inputs[i].name)
                                         : ("Input " + String(i + 1));
  JsonDocument doc;
  doc["name"]        = name;
  doc["unique_id"]   = String(config.mqtt.clientId) + "_" + key;
  doc["state_topic"] = base() + "/input/" + String(i + 1) + "/state";
  doc["payload_on"]  = "ON";
  doc["payload_off"] = "OFF";
  addDeviceAndAvail(doc);
  pubDisc("binary_sensor", key, doc);
}

// Diagnostic sensor (RSSI / uptime / heap) -- HA "diagnostic" category.
static void diagDisc(const String& key, const String& name, const String& sub,
                     const char* devClass, const String& unit) {
  JsonDocument doc;
  doc["name"]            = name;
  doc["unique_id"]       = String(config.mqtt.clientId) + "_" + key;
  doc["state_topic"]     = base() + sub;
  doc["entity_category"] = "diagnostic";
  doc["state_class"]     = "measurement";
  if (devClass)      doc["device_class"]        = devClass;
  if (unit.length()) doc["unit_of_measurement"] = unit;
  addDeviceAndAvail(doc);
  pubDisc("sensor", key, doc);
}

// Display name for the currently selected chip on each bus.
static const char* i2cChipName() {
  switch (config.i2c.type) {
    case I2C_BMP280: return "BMP280";
    case I2C_BMP180: return "BMP180";
    default:         return "BME280";
  }
}
static const char* probeChipName() {
  switch (config.probe.type) {
    case PROBE_DHT11: return "DHT11";
    case PROBE_DHT22: return "DHT22";
    default:          return "DS18B20";
  }
}

static void publishDiscovery() {
  const char* tempUnit = (config.i2c.tempUnit == 'F') ? "°F" : "°C";
  const char* probeUnit = (config.probe.tempUnit == 'F') ? "°F" : "°C";
  const char* presUnit = config.i2c.pressureInHg ? "inHg" : "hPa";
  String chip = i2cChipName();
  String probe = probeChipName();

  // Type-neutral topics; entity names follow the selected chip. Humidity is
  // advertised only when the selected chip provides it (else removed from HA).
  sensorDisc("i2c_temp", chip + " temperature", "/sensor/i2c/temperature",
             "temperature", tempUnit, config.i2c.enabled);
  sensorDisc("i2c_hum", chip + " humidity", "/sensor/i2c/humidity",
             "humidity", "%", config.i2c.enabled && config.i2c.hasHumidity());
  sensorDisc("i2c_pres", chip + " pressure", "/sensor/i2c/pressure",
             "pressure", presUnit, config.i2c.enabled);
  sensorDisc("probe_temp", probe + " temperature", "/sensor/probe/temperature",
             "temperature", probeUnit, config.probe.enabled);
  sensorDisc("probe_hum", probe + " humidity", "/sensor/probe/humidity",
             "humidity", "%", config.probe.enabled && config.probe.hasHumidity());

  // Remove entities advertised under the old chip-specific keys (pre-upgrade).
  removeDisc("sensor", "bme_temp");
  removeDisc("sensor", "bme_hum");
  removeDisc("sensor", "bme_pres");
  removeDisc("sensor", "ds_temp");

  for (int i = 0; i < 2; i++) {
    String key = "analog" + String(i + 1);
    const AnalogConfig& a = config.analog[i];
    const char* dc = (a.scale == ANALOG_VOLTAGE) ? "voltage" : nullptr;
    String unit = (a.scale == ANALOG_VOLTAGE) ? "V"
                  : (a.scale == ANALOG_PERCENT) ? "%" : "";
    sensorDisc(key, a.label, "/sensor/" + key, dc, unit, a.enabled);
  }

  for (int i = 0; i < 2; i++) relayDisc(i);
  for (int i = 0; i < 2; i++) inputDisc(i);

  diagDisc("rssi", "WiFi signal", "/diag/rssi", "signal_strength", "dBm");
  diagDisc("uptime", "Uptime", "/diag/uptime", "duration", "s");
  diagDisc("heap", "Free memory", "/diag/heap", "data_size", "B");
}

static bool tryConnect() {
  String willTopic = base() + "/status";
  bool ok;
  const char* user = config.mqtt.user[0] ? config.mqtt.user : nullptr;
  const char* pass = config.mqtt.pass[0] ? config.mqtt.pass : nullptr;
  ok = client.connect(config.mqtt.clientId, user, pass,
                      willTopic.c_str(), 0, true, "offline");
  if (!ok) return false;

  client.publish(willTopic.c_str(), "online", true);  // retained availability
  for (int i = 0; i < 2; i++) {
    if (config.relays[i].allowMqtt) {
      String sub = base() + "/relay/" + String(i + 1) + "/set";
      client.subscribe(sub.c_str());
    }
  }
  if (config.mqtt.haDiscovery) publishDiscovery();
  // Seed current relay + input states so HA reflects them on (re)connect.
  for (int i = 0; i < 2; i++) publishRelayState(i, Relays::getState(i));
  for (int i = 0; i < 2; i++) publishInputState(i, Inputs::getState(i));
  prevInit = false;  // re-publish alert states on the next readings cycle
  Serial.println("MQTT connected");
  return true;
}

void loop() {
  if (!config.mqtt.enabled || config.mqtt.host[0] == '\0') return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    if (millis() - lastReconnect >= kReconnectEveryMs) {
      lastReconnect = millis();
      tryConnect();
    }
    return;
  }
  client.loop();
}

bool connected() { return client.connected(); }

static void pubFloat(const String& topic, float v) {
  char buf[16];
  dtostrf(v, 0, 2, buf);
  client.publish(topic.c_str(), buf);
}

static void pubAlert(int idx, const String& metric, MetricState s) {
  if (!prevInit || prevState[idx] != s) {
    prevState[idx] = s;
    client.publish((base() + "/alert/" + metric).c_str(), stateName(s));
  }
}

void publishReadings(const Readings& r) {
  if (!client.connected()) return;
  String b = base();

  if (r.i2cOk) {
    pubFloat(b + "/sensor/i2c/temperature", r.temp);
    pubFloat(b + "/sensor/i2c/pressure", r.pres);
    pubAlert(0, "i2c/temperature", r.st_temp);
    pubAlert(2, "i2c/pressure", r.st_pres);
    if (config.i2c.hasHumidity()) {
      pubFloat(b + "/sensor/i2c/humidity", r.hum);
      pubAlert(1, "i2c/humidity", r.st_hum);
    }
  }
  if (r.probeOk) {
    pubFloat(b + "/sensor/probe/temperature", r.probeTemp);
    pubAlert(3, "probe/temperature", r.st_probe);
  }
  if (r.probeHumOk) {
    pubFloat(b + "/sensor/probe/humidity", r.probeHum);
    pubAlert(6, "probe/humidity", r.st_probeHum);
  }
  if (r.a1Ok) {
    pubFloat(b + "/sensor/analog1", r.a1);
    pubAlert(4, "analog1", r.st_a1);
  }
  if (r.a2Ok) {
    pubFloat(b + "/sensor/analog2", r.a2);
    pubAlert(5, "analog2", r.st_a2);
  }

  // Diagnostics (always available).
  client.publish((b + "/diag/rssi").c_str(), String(WiFi.RSSI()).c_str());
  client.publish((b + "/diag/uptime").c_str(),
                 String(millis() / 1000UL).c_str());
  client.publish((b + "/diag/heap").c_str(),
                 String(ESP.getFreeHeap()).c_str());

  prevInit = true;
}

void publishRelayState(int idx, bool on) {
  if (!client.connected()) return;
  String topic = base() + "/relay/" + String(idx + 1) + "/state";
  client.publish(topic.c_str(), on ? "ON" : "OFF", true);  // retained
}

void publishInputState(int idx, bool on) {
  if (!client.connected()) return;
  String topic = base() + "/input/" + String(idx + 1) + "/state";
  client.publish(topic.c_str(), on ? "ON" : "OFF", true);  // retained
}

}  // namespace Mqtt
