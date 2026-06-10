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

// Identifies a sensor metric, used as a relay rule source. The integer values
// are persisted in the relay config blob, so they are FIXED -- only append new
// sources at the end; never reorder or reuse a value.
enum MetricSource {
  SRC_NONE = 0,
  SRC_I2C_TEMP  = 1, SRC_I2C_HUM = 2, SRC_I2C_PRES = 3,  // I2C baro/humidity chip
  SRC_PROBE_TEMP = 4,                                    // 1-wire / single-wire probe
  SRC_ANALOG1 = 5, SRC_ANALOG2 = 6,
  SRC_PROBE_HUM = 7                                       // DHT humidity (appended)
};

// I2C barometric/humidity sensor selectable on the shared I2C bus.
enum I2cType { I2C_BME280 = 0, I2C_BMP280, I2C_BMP180 };

// Temperature probe on the shared data pin: 1-wire (DS18B20) or single-wire DHT.
enum ProbeType { PROBE_DS18B20 = 0, PROBE_DHT11, PROBE_DHT22 };

enum AnalogScale { ANALOG_RAW = 0, ANALOG_PERCENT = 1, ANALOG_VOLTAGE = 2 };

enum RelayMode {
  RELAY_OFF = 0, RELAY_MANUAL, RELAY_TIMER, RELAY_SENSOR, RELAY_BUTTON,
  RELAY_SCHEDULE  // clock-based on/off slots (needs NTP-synced time)
};

// Optional low/high trip points for a single metric.
struct Threshold {
  bool  lowEnabled  = false;
  float low         = 0.0f;
  bool  highEnabled = false;
  float high        = 0.0f;
};

// I2C sensor: BME280 (T/RH/P), BMP280 (T/P) or BMP180 (T/P, fixed addr 0x77).
struct I2cSensorConfig {
  bool      enabled      = false;
  I2cType   type         = I2C_BME280;
  uint8_t   address      = 0x76;   // 0x76 or 0x77 (forced 0x77 for BMP180)
  char      tempUnit     = 'C';    // 'C' or 'F'
  bool      pressureInHg = false;  // false = hPa, true = inHg
  Threshold tTemp, tHum, tPres;
  bool hasHumidity() const { return type == I2C_BME280; }
};

// Shared-pin temperature probe: DS18B20 (1-wire) or DHT11/DHT22 (single-wire).
struct ProbeConfig {
  bool      enabled  = false;
  ProbeType type     = PROBE_DS18B20;
  char      tempUnit = 'C';        // 'C' or 'F'
  Threshold tTemp;
  Threshold tHum;                  // only meaningful for DHT
  bool hasHumidity() const { return type != PROBE_DS18B20; }
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

// One clock-schedule entry. Spans where offMin <= onMin wrap past midnight
// and belong to the day they start on.
struct ScheduleSlot {
  bool     enabled = false;
  uint16_t onMin   = 480;    // minutes since midnight (08:00)
  uint16_t offMin  = 1320;   // 22:00
  uint8_t  days    = 0x7F;   // bit0 = Mon ... bit6 = Sun
};

static const int kSlotsPerRelay = 4;

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
  ScheduleSlot sched[kSlotsPerRelay];
};

// NTP / timezone settings. tz holds a POSIX TZ string (DST rules included);
// the portal picks it from a dropdown of common zones.
struct TimeConfig {
  bool enabled    = true;
  char server[48] = "pool.ntp.org";
  char tz[48]     = "CET-1CEST,M3.5.0,M10.5.0/3";  // Brussels / central Europe
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
  MqttConfig       mqtt;
  TimeConfig       timecfg;
  I2cSensorConfig  i2c;
  ProbeConfig      probe;
  AnalogConfig     analog[2];
  ButtonConfig     button;
  RelayConfig      relays[2];

  void load();         // read from NVS (applies defaults if absent)
  void save();         // write to NVS
  void ensureClientId(); // fill mqtt.clientId from chip id if empty
  static void factoryReset(); // clear all NVS namespaces, then restart
};

extern Config config;

#endif  // CONFIG_H
