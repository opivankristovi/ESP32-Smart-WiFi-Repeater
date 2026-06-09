#ifndef MQTT_H
#define MQTT_H

#include "config.h"
#include "sensors.h"

// MQTT client over the upstream (STA) link. Publishes sensor values + alerts +
// relay state, and subscribes to per-relay command topics.
namespace Mqtt {

void begin();                          // configure client from config
void loop();                           // maintain connection (non-blocking)
void publishReadings(const Readings& r);  // values + alert transitions
void publishRelayState(int idx, bool on);
bool connected();

}  // namespace Mqtt

#endif  // MQTT_H
