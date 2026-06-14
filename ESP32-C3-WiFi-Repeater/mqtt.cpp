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

// index: 0 i2c temp, 1 i2c hum, 2 i2c pres, 3 probe temp, 4 probe hum,
//        5..5+NUM_INPUTS-1 inputs
static const int kAlertSlots = 5 + NUM_INPUTS;
static MetricState prevState[kAlertSlots];
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
  for (int i = 0; i < NUM_RELAYS; i++) {
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
  client.setBufferSize(1024);
  client.setCallback(onMessage);
}

// ---- Home Assistant MQTT discovery -----------------------------------------
static void addDeviceAndAvail(JsonDocument& doc) {
  doc["availability_topic"]    = base() + "/status";
  doc["payload_available"]     = "online";
  doc["payload_not_available"] = "offline";
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(config.mqtt.clientId);
  dev["name"]         = String("ESP32-C3 Smart Repeater (") + config.mqtt.clientId + ")";
  dev["manufacturer"] = "Espressif";
  dev["model"]        = "ESP32-C3 WiFi Repeater";
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
    removeDisc("binary_sensor", key);
  } else {
    pubDisc("binary_sensor", key, doc);
    removeDisc("switch", key);
  }
}

static void inputDisc(int i) {
  String key = "input" + String(i + 1);
  if (!config.inputs[i].enabled) {
    removeDisc("binary_sensor", key);
    removeDisc("sensor", key);
    return;
  }

  String name = config.inputs[i].name[0] ? String(config.inputs[i].name)
                                         : ("Input " + String(i + 1));

  if (config.inputs[i].type == INPUT_DIGITAL) {
    JsonDocument doc;
    doc["name"]        = name;
    doc["unique_id"]   = String(config.mqtt.clientId) + "_" + key;
    doc["state_topic"] = base() + "/input/" + String(i + 1) + "/state";
    doc["payload_on"]  = "ON";
    doc["payload_off"] = "OFF";
    addDeviceAndAvail(doc);
    pubDisc("binary_sensor", key, doc);
    removeDisc("sensor", key);
  } else {
    const InputConfig& ic = config.inputs[i];
    const char* dc = (ic.scale == ANALOG_VOLTAGE) ? "voltage" : nullptr;
    String unit = (ic.scale == ANALOG_VOLTAGE) ? "V"
                  : (ic.scale == ANALOG_PERCENT) ? "%" : "";
    sensorDisc(key, name, "/input/" + String(i + 1) + "/value", dc, unit, true);
    removeDisc("binary_sensor", key);
  }
}

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
  const char* tempUnit = (config.i2c.tempUnit == 'F') ? "\xC2\xB0""F" : "\xC2\xB0""C";
  const char* probeUnit = (config.probe.tempUnit == 'F') ? "\xC2\xB0""F" : "\xC2\xB0""C";
  const char* presUnit = config.i2c.pressureInHg ? "inHg" : "hPa";
  String chip = i2cChipName();
  String probe = probeChipName();

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

  for (int i = 0; i < NUM_INPUTS; i++) inputDisc(i);
  for (int i = 0; i < NUM_RELAYS; i++) relayDisc(i);

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

  client.publish(willTopic.c_str(), "online", true);
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (config.relays[i].allowMqtt) {
      String sub = base() + "/relay/" + String(i + 1) + "/set";
      client.subscribe(sub.c_str());
    }
  }
  if (config.mqtt.haDiscovery) publishDiscovery();
  for (int i = 0; i < NUM_RELAYS; i++) publishRelayState(i, Relays::getState(i));
  for (int i = 0; i < NUM_INPUTS; i++) {
    if (config.inputs[i].enabled && config.inputs[i].type == INPUT_DIGITAL)
      publishInputState(i, Inputs::getState(i));
  }
  prevInit = false;
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
    pubAlert(4, "probe/humidity", r.st_probeHum);
  }

  for (int i = 0; i < NUM_INPUTS; i++) {
    if (!r.inputOk[i]) continue;
    pubFloat(b + "/input/" + String(i + 1) + "/value", r.inputVal[i]);
    pubAlert(5 + i, "input/" + String(i + 1), r.st_input[i]);
  }

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
  client.publish(topic.c_str(), on ? "ON" : "OFF", true);
}

void publishInputState(int idx, bool on) {
  if (!client.connected()) return;
  String topic = base() + "/input/" + String(idx + 1) + "/state";
  client.publish(topic.c_str(), on ? "ON" : "OFF", true);
}

void publishInputValue(int idx, float val) {
  if (!client.connected()) return;
  pubFloat(base() + "/input/" + String(idx + 1) + "/value", val);
}

}  // namespace Mqtt
