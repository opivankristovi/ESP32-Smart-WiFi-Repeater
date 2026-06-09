#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin map (see README). Analog inputs MUST be on ADC1 (GPIO32-39): ADC2 pins
// do not work while WiFi is active.
// ---------------------------------------------------------------------------
static const int PIN_I2C_SDA   = 21;
static const int PIN_I2C_SCL   = 22;
static const int PIN_ONEWIRE   = 4;
static const int PIN_ANALOG[2] = {34, 35};
static const int PIN_BUTTON    = 25;
static const int PIN_RELAY[2]  = {26, 27};

// ---------------------------------------------------------------------------
// Shared types
// ---------------------------------------------------------------------------
enum MetricState { METRIC_OK = 0, METRIC_LOW = 1, METRIC_HIGH = 2 };

// Identifies a sensor metric, used as a relay rule source.
enum MetricSource {
  SRC_NONE = 0,
  SRC_BME_TEMP, SRC_BME_HUM, SRC_BME_PRES,
  SRC_DS_TEMP,
  SRC_ANALOG1, SRC_ANALOG2
};

enum AnalogScale { ANALOG_RAW = 0, ANALOG_PERCENT = 1, ANALOG_VOLTAGE = 2 };

enum RelayMode {
  RELAY_OFF = 0, RELAY_MANUAL, RELAY_TIMER, RELAY_SENSOR, RELAY_BUTTON
};

// Optional low/high trip points for a single metric.
struct Threshold {
  bool  lowEnabled  = false;
  float low         = 0.0f;
  bool  highEnabled = false;
  float high        = 0.0f;
};

struct Bme280Config {
  bool      enabled      = false;
  uint8_t   address      = 0x76;   // 0x76 or 0x77
  char      tempUnit     = 'C';    // 'C' or 'F'
  bool      pressureInHg = false;  // false = hPa, true = inHg
  Threshold tTemp, tHum, tPres;
};

struct Ds18b20Config {
  bool      enabled  = false;
  char      tempUnit = 'C';        // 'C' or 'F'
  Threshold tTemp;
};

struct AnalogConfig {
  bool        enabled = false;
  char        label[24] = "analog";
  AnalogScale scale   = ANALOG_RAW;
  int         rawMin  = 0;          // maps to 0% for ANALOG_PERCENT
  int         rawMax  = 4095;       // maps to 100% for ANALOG_PERCENT
  Threshold   thr;
};

struct ButtonConfig {
  bool enabled  = false;
  bool activeLow = true;           // pressed = LOW (INPUT_PULLUP)
};

struct RelayConfig {
  char        name[24]    = "relay";
  RelayMode   mode        = RELAY_OFF;
  bool        activeLow   = false;  // relay board logic
  bool        manualState = false;  // initial state for RELAY_MANUAL
  uint32_t    timerOnSec  = 5;
  uint32_t    timerOffSec = 5;
  MetricSource src        = SRC_NONE;
  uint8_t     cmp         = 0;      // 0 = ON when above level, 1 = ON when below
  float       level       = 0.0f;
  float       hyst        = 0.0f;   // hysteresis band around level
  bool        allowMqtt   = true;   // accept .../relay/N/set commands
};

struct MqttConfig {
  bool     enabled            = false;
  char     host[64]           = "";
  uint16_t port               = 1883;
  char     user[40]           = "";
  char     pass[40]           = "";
  char     baseTopic[40]      = "esp32repeater";
  char     clientId[32]       = "";    // defaults to esp32-<chipid>
  uint16_t publishIntervalSec = 30;
  bool     haDiscovery        = true;  // publish Home Assistant MQTT discovery
  char     haPrefix[24]       = "homeassistant";
};

// ---------------------------------------------------------------------------
// Top-level config. Persisted as one JSON blob in Preferences (namespace
// "settings", key "json"). WiFi/AP credentials are NOT here -- they stay in
// the Phase-1 "repeater" namespace handled by the net module.
// ---------------------------------------------------------------------------
struct Config {
  MqttConfig    mqtt;
  Bme280Config  bme280;
  Ds18b20Config ds18b20;
  AnalogConfig  analog[2];
  ButtonConfig  button;
  RelayConfig   relays[2];

  void load();         // read from NVS (applies defaults if absent)
  void save();         // write to NVS
  void ensureClientId(); // fill mqtt.clientId from chip id if empty
  static void factoryReset(); // clear all NVS namespaces, then restart
};

extern Config config;

#endif  // CONFIG_H
