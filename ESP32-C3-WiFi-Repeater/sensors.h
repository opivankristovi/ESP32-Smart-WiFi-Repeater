#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

struct Readings {
  bool  i2cOk = false;
  float temp = 0, hum = 0, pres = 0;
  bool  probeOk = false;
  float probeTemp = 0;
  bool  probeHumOk = false;
  float probeHum = 0;

  bool        inputOk[NUM_INPUTS]  = {};
  float       inputVal[NUM_INPUTS] = {};

  MetricState st_temp = METRIC_OK, st_hum = METRIC_OK, st_pres = METRIC_OK;
  MetricState st_probe = METRIC_OK, st_probeHum = METRIC_OK;
  MetricState st_input[NUM_INPUTS] = {};
};

namespace Sensors {

void begin();
void tick();
Readings readAll();
float metricValue(MetricSource src, bool& ok);

}  // namespace Sensors

#endif  // SENSORS_H
