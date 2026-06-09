#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

Config config;

// ---- JSON helpers ----------------------------------------------------------
static void thrToJson(JsonObject o, const Threshold& t) {
  o["le"] = t.lowEnabled;
  o["lo"] = t.low;
  o["he"] = t.highEnabled;
  o["hi"] = t.high;
}

static void thrFromJson(JsonObjectConst o, Threshold& t) {
  t.lowEnabled  = o["le"] | false;
  t.low         = o["lo"] | 0.0f;
  t.highEnabled = o["he"] | false;
  t.high        = o["hi"] | 0.0f;
}

static void copyStr(char* dst, size_t cap, const char* src) {
  strlcpy(dst, src, cap);
}

// ---- Serialize -------------------------------------------------------------
static String toJson() {
  JsonDocument doc;

  JsonObject m = doc["mqtt"].to<JsonObject>();
  m["en"]   = config.mqtt.enabled;
  m["host"] = config.mqtt.host;
  m["port"] = config.mqtt.port;
  m["user"] = config.mqtt.user;
  m["pass"] = config.mqtt.pass;
  m["base"] = config.mqtt.baseTopic;
  m["cid"]  = config.mqtt.clientId;
  m["ival"] = config.mqtt.publishIntervalSec;
  m["ha"]   = config.mqtt.haDiscovery;
  m["hap"]  = config.mqtt.haPrefix;

  JsonObject b = doc["bme"].to<JsonObject>();
  b["en"]   = config.bme280.enabled;
  b["addr"] = config.bme280.address;
  b["tu"]   = String(config.bme280.tempUnit);
  b["inhg"] = config.bme280.pressureInHg;
  thrToJson(b["t"].to<JsonObject>(), config.bme280.tTemp);
  thrToJson(b["h"].to<JsonObject>(), config.bme280.tHum);
  thrToJson(b["p"].to<JsonObject>(), config.bme280.tPres);

  JsonObject d = doc["ds"].to<JsonObject>();
  d["en"] = config.ds18b20.enabled;
  d["tu"] = String(config.ds18b20.tempUnit);
  thrToJson(d["t"].to<JsonObject>(), config.ds18b20.tTemp);

  JsonArray aArr = doc["an"].to<JsonArray>();
  for (int i = 0; i < 2; i++) {
    JsonObject a = aArr.add<JsonObject>();
    a["en"]    = config.analog[i].enabled;
    a["lbl"]   = config.analog[i].label;
    a["scale"] = (int)config.analog[i].scale;
    a["rmin"]  = config.analog[i].rawMin;
    a["rmax"]  = config.analog[i].rawMax;
    thrToJson(a["t"].to<JsonObject>(), config.analog[i].thr);
  }

  JsonObject btn = doc["btn"].to<JsonObject>();
  btn["en"] = config.button.enabled;
  btn["al"] = config.button.activeLow;

  JsonArray rArr = doc["relays"].to<JsonArray>();
  for (int i = 0; i < 2; i++) {
    const RelayConfig& rc = config.relays[i];
    JsonObject r = rArr.add<JsonObject>();
    r["name"]  = rc.name;
    r["mode"]  = (int)rc.mode;
    r["al"]    = rc.activeLow;
    r["man"]   = rc.manualState;
    r["ton"]   = rc.timerOnSec;
    r["toff"]  = rc.timerOffSec;
    r["src"]   = (int)rc.src;
    r["cmp"]   = rc.cmp;
    r["lvl"]   = rc.level;
    r["hyst"]  = rc.hyst;
    r["mqtt"]  = rc.allowMqtt;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// ---- Deserialize -----------------------------------------------------------
static void fromJson(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return;  // parse error -> keep defaults

  JsonObjectConst m = doc["mqtt"];
  if (m) {
    config.mqtt.enabled            = m["en"] | false;
    copyStr(config.mqtt.host, sizeof(config.mqtt.host), m["host"] | "");
    config.mqtt.port               = m["port"] | 1883;
    copyStr(config.mqtt.user, sizeof(config.mqtt.user), m["user"] | "");
    copyStr(config.mqtt.pass, sizeof(config.mqtt.pass), m["pass"] | "");
    copyStr(config.mqtt.baseTopic, sizeof(config.mqtt.baseTopic),
            m["base"] | "esp32repeater");
    copyStr(config.mqtt.clientId, sizeof(config.mqtt.clientId), m["cid"] | "");
    config.mqtt.publishIntervalSec = m["ival"] | 30;
    config.mqtt.haDiscovery        = m["ha"] | true;
    copyStr(config.mqtt.haPrefix, sizeof(config.mqtt.haPrefix),
            m["hap"] | "homeassistant");
  }

  JsonObjectConst b = doc["bme"];
  if (b) {
    config.bme280.enabled      = b["en"] | false;
    config.bme280.address      = b["addr"] | 0x76;
    config.bme280.tempUnit     = (b["tu"] | "C")[0];
    config.bme280.pressureInHg = b["inhg"] | false;
    thrFromJson(b["t"], config.bme280.tTemp);
    thrFromJson(b["h"], config.bme280.tHum);
    thrFromJson(b["p"], config.bme280.tPres);
  }

  JsonObjectConst d = doc["ds"];
  if (d) {
    config.ds18b20.enabled  = d["en"] | false;
    config.ds18b20.tempUnit = (d["tu"] | "C")[0];
    thrFromJson(d["t"], config.ds18b20.tTemp);
  }

  JsonArrayConst aArr = doc["an"];
  if (aArr) {
    int i = 0;
    for (JsonObjectConst a : aArr) {
      if (i >= 2) break;
      config.analog[i].enabled = a["en"] | false;
      copyStr(config.analog[i].label, sizeof(config.analog[i].label),
              a["lbl"] | "analog");
      config.analog[i].scale  = (AnalogScale)(a["scale"] | 0);
      config.analog[i].rawMin = a["rmin"] | 0;
      config.analog[i].rawMax = a["rmax"] | 4095;
      thrFromJson(a["t"], config.analog[i].thr);
      i++;
    }
  }

  JsonObjectConst btn = doc["btn"];
  if (btn) {
    config.button.enabled  = btn["en"] | false;
    config.button.activeLow = btn["al"] | true;
  }

  JsonArrayConst rArr = doc["relays"];
  if (rArr) {
    int i = 0;
    for (JsonObjectConst r : rArr) {
      if (i >= 2) break;
      RelayConfig& rc = config.relays[i];
      copyStr(rc.name, sizeof(rc.name), r["name"] | "relay");
      rc.mode        = (RelayMode)(r["mode"] | 0);
      rc.activeLow   = r["al"] | false;
      rc.manualState = r["man"] | false;
      rc.timerOnSec  = r["ton"] | 5;
      rc.timerOffSec = r["toff"] | 5;
      rc.src         = (MetricSource)(r["src"] | 0);
      rc.cmp         = r["cmp"] | 0;
      rc.level       = r["lvl"] | 0.0f;
      rc.hyst        = r["hyst"] | 0.0f;
      rc.allowMqtt   = r["mqtt"] | true;
      i++;
    }
  }
}

// ---- Public API ------------------------------------------------------------
void Config::ensureClientId() {
  if (mqtt.clientId[0] != '\0') return;
  uint64_t chip = ESP.getEfuseMac();
  snprintf(mqtt.clientId, sizeof(mqtt.clientId), "esp32-%06llX",
           chip & 0xFFFFFF);
}

void Config::load() {
  Preferences p;
  p.begin("settings", true);  // read-only
  String json = p.getString("json", "");
  p.end();
  if (json.length()) fromJson(json);
  ensureClientId();
}

void Config::save() {
  ensureClientId();
  String json = toJson();
  Preferences p;
  p.begin("settings", false);
  p.putString("json", json);
  p.end();
}

void Config::factoryReset() {
  Preferences p;
  p.begin("settings", false);
  p.clear();
  p.end();
  p.begin("repeater", false);  // Phase-1 WiFi/AP credentials
  p.clear();
  p.end();
  delay(200);
  ESP.restart();
}
