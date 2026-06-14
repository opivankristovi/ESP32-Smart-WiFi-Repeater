#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin map — ESP32-C3 SuperMini.
// Inputs are on ADC1 (GPIO 0-4) so they can serve as analog while WiFi is
// active. GPIO2 is skipped (SPI-boot strapping pin). GPIO5 is ADC2 (unusable
// for analog with WiFi) but fine for the digital probe bus.
// ---------------------------------------------------------------------------
static const int NUM_INPUTS    = 4;
static const int NUM_RELAYS    = 2;

static const int PIN_I2C_SDA   = 6;
static const int PIN_I2C_SCL   = 7;
static const int PIN_ONEWIRE   = 5;
static const int PIN_INPUT[NUM_INPUTS] = {0, 1, 3, 4};
static const int PIN_RELAY[NUM_RELAYS] = {10, 20};

// ---------------------------------------------------------------------------
// Shared types
// ---------------------------------------------------------------------------
enum MetricState { METRIC_OK = 0, METRIC_LOW = 1, METRIC_HIGH = 2 };

// Identifies a sensor metric, used as a relay rule source. The integer values
// are persisted in the relay config blob, so they are FIXED — only append new
// sources at the end; never reorder or reuse a value.
enum MetricSource {
  SRC_NONE = 0,
  SRC_I2C_TEMP  = 1, SRC_I2C_HUM = 2, SRC_I2C_PRES = 3,
  SRC_PROBE_TEMP = 4,
  SRC_INPUT1 = 5, SRC_INPUT2 = 6, SRC_INPUT3 = 7, SRC_INPUT4 = 8,
  SRC_PROBE_HUM = 9
};

// I2C barometric/humidity sensor selectable on the shared I2C bus.
enum I2cType { I2C_BME280 = 0, I2C_BMP280, I2C_BMP180 };

// Temperature probe on the shared data pin: 1-wire (DS18B20) or single-wire DHT.
enum ProbeType { PROBE_DS18B20 = 0, PROBE_DHT11, PROBE_DHT22 };

enum AnalogScale { ANALOG_RAW = 0, ANALOG_PERCENT = 1, ANALOG_VOLTAGE = 2 };

enum RelayMode {
  RELAY_OFF = 0, RELAY_MANUAL, RELAY_TIMER, RELAY_SENSOR, RELAY_BUTTON,
  RELAY_SCHEDULE
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
  uint8_t   address      = 0x76;
  char      tempUnit     = 'C';
  bool      pressureInHg = false;
  Threshold tTemp, tHum, tPres;
  bool hasHumidity() const { return type == I2C_BME280; }
};

// Shared-pin temperature probe: DS18B20 (1-wire) or DHT11/DHT22 (single-wire).
struct ProbeConfig {
  bool      enabled  = false;
  ProbeType type     = PROBE_DS18B20;
  char      tempUnit = 'C';
  Threshold tTemp;
  Threshold tHum;
  bool hasHumidity() const { return type != PROBE_DS18B20; }
};

// One unified input: a digital push button or an analog sensor. Each of the
// NUM_INPUTS channels can be configured independently. Digital inputs drive
// the selected relay (RELAY_BUTTON) and publish to MQTT / HA as binary_sensor.
// Analog inputs are sampled, scaled, and published as an HA sensor.
enum InputType { INPUT_DIGITAL = 0, INPUT_ANALOG = 1 };

struct InputConfig {
  bool        enabled   = false;
  InputType   type      = INPUT_DIGITAL;
  char        name[24]  = "input";
  // digital-specific
  bool        activeLow = true;
  // analog-specific
  AnalogScale scale     = ANALOG_RAW;
  int         rawMin    = 0;
  int         rawMax    = 4095;
  Threshold   thr;
};

// One clock-schedule entry.
struct ScheduleSlot {
  bool     enabled = false;
  uint16_t onMin   = 480;
  uint16_t offMin  = 1320;
  uint8_t  days    = 0x7F;
};

static const int kSlotsPerRelay = 4;

struct RelayConfig {
  char        name[24]    = "relay";
  RelayMode   mode        = RELAY_OFF;
  bool        activeLow   = false;
  bool        manualState = false;
  uint32_t    timerOnSec  = 5;
  uint32_t    timerOffSec = 5;
  MetricSource src        = SRC_NONE;
  uint8_t     cmp         = 0;
  float       level       = 0.0f;
  float       hyst        = 0.0f;
  bool        allowMqtt   = true;
  uint8_t     buttonInput = 0;
  ScheduleSlot sched[kSlotsPerRelay];
};

struct TimeConfig {
  bool enabled    = true;
  char server[48] = "pool.ntp.org";
  char tz[48]     = "CET-1CEST,M3.5.0,M10.5.0/3";
};

struct MqttConfig {
  bool     enabled            = false;
  char     host[64]           = "";
  uint16_t port               = 1883;
  char     user[40]           = "";
  char     pass[40]           = "";
  char     baseTopic[40]      = "esp32repeater";
  char     clientId[32]       = "";
  uint16_t publishIntervalSec = 30;
  bool     haDiscovery        = true;
  char     haPrefix[24]       = "homeassistant";
};

// ---------------------------------------------------------------------------
// Top-level config. Persisted as one JSON blob in Preferences (namespace
// "settings", key "json"). WiFi/AP credentials are NOT here.
// ---------------------------------------------------------------------------
struct Config {
  MqttConfig       mqtt;
  TimeConfig       timecfg;
  I2cSensorConfig  i2c;
  ProbeConfig      probe;
  InputConfig      inputs[NUM_INPUTS];
  RelayConfig      relays[NUM_RELAYS];

  void load();
  void save();
  void ensureClientId();
  static void factoryReset();
};

extern Config config;

#endif  // CONFIG_H
