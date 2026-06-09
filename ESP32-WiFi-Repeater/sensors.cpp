#include "sensors.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

namespace Sensors {

static Adafruit_BME280 bme;
static bool bmeReady = false;

static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature ds(&oneWire);
static bool dsReady = false;

// Non-blocking DS18B20 conversion state machine.
static bool dsConverting = false;
static unsigned long dsRequestedAt = 0;
static const unsigned long kDsConvMs = 800;  // 12-bit conversion ~750 ms
static float dsLastC = NAN;

static Readings last;  // cached snapshot for metricValue()

// ---- helpers ---------------------------------------------------------------
static float cToUnit(float c, char unit) {
  return (unit == 'F') ? (c * 9.0f / 5.0f + 32.0f) : c;
}

static MetricState evalThr(float v, const Threshold& t) {
  if (t.highEnabled && v >= t.high) return METRIC_HIGH;
  if (t.lowEnabled && v <= t.low)   return METRIC_LOW;
  return METRIC_OK;
}

static float scaleAnalog(const AnalogConfig& cfg, int raw, int mv) {
  switch (cfg.scale) {
    case ANALOG_VOLTAGE: return mv / 1000.0f;  // volts
    case ANALOG_PERCENT: {
      int span = cfg.rawMax - cfg.rawMin;
      if (span == 0) return 0.0f;
      float pct = (raw - cfg.rawMin) * 100.0f / span;
      return constrain(pct, 0.0f, 100.0f);
    }
    case ANALOG_RAW:
    default: return raw;
  }
}

// ---- init ------------------------------------------------------------------
void begin() {
  if (config.bme280.enabled) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    bmeReady = bme.begin(config.bme280.address, &Wire);
    Serial.printf("BME280 %s\n", bmeReady ? "ready" : "NOT found");
  }
  if (config.ds18b20.enabled) {
    ds.begin();
    ds.setWaitForConversion(false);  // async conversions
    dsReady = ds.getDeviceCount() > 0;
    Serial.printf("DS18B20 %s\n", dsReady ? "ready" : "NOT found");
  }
  for (int i = 0; i < 2; i++) {
    if (config.analog[i].enabled) {
      pinMode(PIN_ANALOG[i], INPUT);
    }
  }
}

// ---- non-blocking DS18B20 --------------------------------------------------
void tick() {
  if (!dsReady) return;
  if (!dsConverting) {
    ds.requestTemperatures();
    dsConverting = true;
    dsRequestedAt = millis();
  } else if (millis() - dsRequestedAt >= kDsConvMs) {
    float c = ds.getTempCByIndex(0);
    if (c != DEVICE_DISCONNECTED_C) dsLastC = c;
    dsConverting = false;  // next tick starts a fresh conversion
  }
}

// ---- sampling --------------------------------------------------------------
Readings readAll() {
  Readings r;

  if (config.bme280.enabled && bmeReady) {
    float c = bme.readTemperature();
    float h = bme.readHumidity();
    float pPa = bme.readPressure();
    if (!isnan(c) && !isnan(h) && !isnan(pPa) && pPa > 0) {
      r.bmeOk = true;
      r.temp = cToUnit(c, config.bme280.tempUnit);
      r.hum  = h;
      r.pres = config.bme280.pressureInHg ? (pPa * 0.0002953f)
                                          : (pPa / 100.0f);  // inHg or hPa
      r.st_temp = evalThr(r.temp, config.bme280.tTemp);
      r.st_hum  = evalThr(r.hum,  config.bme280.tHum);
      r.st_pres = evalThr(r.pres, config.bme280.tPres);
    }
  }

  if (config.ds18b20.enabled && dsReady && !isnan(dsLastC)) {
    r.dsOk = true;
    r.dsTemp = cToUnit(dsLastC, config.ds18b20.tempUnit);
    r.st_ds = evalThr(r.dsTemp, config.ds18b20.tTemp);
  }

  for (int i = 0; i < 2; i++) {
    if (!config.analog[i].enabled) continue;
    int raw = analogRead(PIN_ANALOG[i]);
    int mv  = analogReadMilliVolts(PIN_ANALOG[i]);
    float v = scaleAnalog(config.analog[i], raw, mv);
    MetricState st = evalThr(v, config.analog[i].thr);
    if (i == 0) { r.a1Ok = true; r.a1 = v; r.st_a1 = st; }
    else        { r.a2Ok = true; r.a2 = v; r.st_a2 = st; }
  }

  last = r;
  return r;
}

// ---- relay rule source -----------------------------------------------------
float metricValue(MetricSource src, bool& ok) {
  ok = true;
  switch (src) {
    case SRC_BME_TEMP: ok = last.bmeOk; return last.temp;
    case SRC_BME_HUM:  ok = last.bmeOk; return last.hum;
    case SRC_BME_PRES: ok = last.bmeOk; return last.pres;
    case SRC_DS_TEMP:  ok = last.dsOk;  return last.dsTemp;
    case SRC_ANALOG1:  ok = last.a1Ok;  return last.a1;
    case SRC_ANALOG2:  ok = last.a2Ok;  return last.a2;
    case SRC_NONE:
    default:           ok = false;      return 0.0f;
  }
}

}  // namespace Sensors
