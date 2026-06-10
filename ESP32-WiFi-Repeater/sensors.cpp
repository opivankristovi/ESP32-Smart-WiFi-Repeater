#include "sensors.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BMP085.h>  // also covers the register-compatible BMP180
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

namespace Sensors {

// ---- I2C sensor (one of BME280 / BMP280 / BMP180) --------------------------
static Adafruit_BME280 bme;
static Adafruit_BMP280 bmp280;
static Adafruit_BMP085 bmp180;
static bool i2cReady = false;

// ---- probe: DS18B20 (1-wire) or DHT11/DHT22 (single-wire), shared pin -------
static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature ds(&oneWire);
static bool dsReady = false;
static DHT* dht = nullptr;   // constructed in begin() with the configured model
static bool dhtReady = false;

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
  if (config.i2c.enabled) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    switch (config.i2c.type) {
      case I2C_BMP280:
        i2cReady = bmp280.begin(config.i2c.address);
        Serial.printf("BMP280 %s\n", i2cReady ? "ready" : "NOT found");
        break;
      case I2C_BMP180:
        i2cReady = bmp180.begin();  // fixed I2C address 0x77, uses global Wire
        Serial.printf("BMP180 %s\n", i2cReady ? "ready" : "NOT found");
        break;
      case I2C_BME280:
      default:
        i2cReady = bme.begin(config.i2c.address, &Wire);
        Serial.printf("BME280 %s\n", i2cReady ? "ready" : "NOT found");
        break;
    }
  }

  if (config.probe.enabled) {
    if (config.probe.type == PROBE_DS18B20) {
      ds.begin();
      ds.setWaitForConversion(false);  // async conversions
      dsReady = ds.getDeviceCount() > 0;
      Serial.printf("DS18B20 %s\n", dsReady ? "ready" : "NOT found");
    } else {
      dht = new DHT(PIN_ONEWIRE,
                    config.probe.type == PROBE_DHT11 ? DHT11 : DHT22);
      dht->begin();
      dhtReady = true;  // DHT has no presence check; reads validate themselves
      Serial.printf("%s started\n",
                    config.probe.type == PROBE_DHT11 ? "DHT11" : "DHT22");
    }
  }

  for (int i = 0; i < 2; i++) {
    if (config.analog[i].enabled) {
      pinMode(PIN_ANALOG[i], INPUT);
    }
  }
}

// ---- non-blocking DS18B20 --------------------------------------------------
void tick() {
  if (!dsReady) return;  // only the 1-wire probe uses the async state machine
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

  if (config.i2c.enabled && i2cReady) {
    float c = NAN, h = NAN, pPa = NAN;
    switch (config.i2c.type) {
      case I2C_BMP280:
        c = bmp280.readTemperature();
        pPa = bmp280.readPressure();
        break;
      case I2C_BMP180:
        c = bmp180.readTemperature();
        pPa = bmp180.readPressure();
        break;
      case I2C_BME280:
      default:
        c = bme.readTemperature();
        h = bme.readHumidity();
        pPa = bme.readPressure();
        break;
    }
    if (!isnan(c) && !isnan(pPa) && pPa > 0) {
      r.i2cOk = true;
      r.temp = cToUnit(c, config.i2c.tempUnit);
      r.pres = config.i2c.pressureInHg ? (pPa * 0.0002953f)
                                       : (pPa / 100.0f);  // inHg or hPa
      r.st_temp = evalThr(r.temp, config.i2c.tTemp);
      r.st_pres = evalThr(r.pres, config.i2c.tPres);
      if (config.i2c.hasHumidity() && !isnan(h)) {
        r.hum = h;
        r.st_hum = evalThr(r.hum, config.i2c.tHum);
      }
    }
  }

  if (config.probe.enabled) {
    if (config.probe.type == PROBE_DS18B20) {
      if (dsReady && !isnan(dsLastC)) {
        r.probeOk = true;
        r.probeTemp = cToUnit(dsLastC, config.probe.tempUnit);
        r.st_probe = evalThr(r.probeTemp, config.probe.tTemp);
      }
    } else if (dht) {
      float c = dht->readTemperature();  // library self-throttles to >=2 s
      float h = dht->readHumidity();
      if (!isnan(c)) {
        r.probeOk = true;
        r.probeTemp = cToUnit(c, config.probe.tempUnit);
        r.st_probe = evalThr(r.probeTemp, config.probe.tTemp);
      }
      if (!isnan(h)) {
        r.probeHumOk = true;
        r.probeHum = h;
        r.st_probeHum = evalThr(r.probeHum, config.probe.tHum);
      }
    }
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
    case SRC_I2C_TEMP:   ok = last.i2cOk;      return last.temp;
    case SRC_I2C_HUM:    ok = last.i2cOk && config.i2c.hasHumidity();
                                                return last.hum;
    case SRC_I2C_PRES:   ok = last.i2cOk;      return last.pres;
    case SRC_PROBE_TEMP: ok = last.probeOk;    return last.probeTemp;
    case SRC_PROBE_HUM:  ok = last.probeHumOk; return last.probeHum;
    case SRC_ANALOG1:    ok = last.a1Ok;       return last.a1;
    case SRC_ANALOG2:    ok = last.a2Ok;       return last.a2;
    case SRC_NONE:
    default:             ok = false;           return 0.0f;
  }
}

}  // namespace Sensors
