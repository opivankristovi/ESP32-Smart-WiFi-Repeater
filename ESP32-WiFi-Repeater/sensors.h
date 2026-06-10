#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

// Snapshot of the latest sensor values (already converted to configured units)
// plus each metric's threshold state.
struct Readings {
  bool  i2cOk = false;                       // BME280 / BMP280 / BMP180
  float temp = 0, hum = 0, pres = 0;
  bool  probeOk = false;                      // DS18B20 / DHT temperature
  float probeTemp = 0;
  bool  probeHumOk = false;                   // DHT humidity only
  float probeHum = 0;
  bool  a1Ok = false, a2Ok = false;
  float a1 = 0, a2 = 0;

  MetricState st_temp = METRIC_OK, st_hum = METRIC_OK, st_pres = METRIC_OK;
  MetricState st_probe = METRIC_OK, st_probeHum = METRIC_OK;
  MetricState st_a1   = METRIC_OK, st_a2 = METRIC_OK;
};

namespace Sensors {

void begin();              // init enabled buses according to config
void tick();               // drives the non-blocking DS18B20 conversion
Readings readAll();        // sample everything + evaluate thresholds (caches)

// Latest value for a metric, in its configured unit. ok=false if unavailable.
float metricValue(MetricSource src, bool& ok);

}  // namespace Sensors

#endif  // SENSORS_H
