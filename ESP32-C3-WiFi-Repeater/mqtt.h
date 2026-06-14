#ifndef MQTT_H
#define MQTT_H

#include "config.h"
#include "sensors.h"

namespace Mqtt {

void begin();
void loop();
void publishReadings(const Readings& r);
void publishRelayState(int idx, bool on);
void publishInputState(int idx, bool on);
void publishInputValue(int idx, float val);
bool connected();

}  // namespace Mqtt

#endif  // MQTT_H
