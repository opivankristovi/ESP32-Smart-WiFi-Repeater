#ifndef RELAYS_H
#define RELAYS_H

#include "config.h"

// Two relay/SSR outputs driven by per-relay rules (timer / sensor threshold /
// button) and optionally by MQTT commands.
namespace Relays {

void begin();                       // configure pins + initial state
void update();                      // evaluate rules each loop, drive pins
void setState(int idx, bool on);    // external override (MQTT / manual form)
void toggle(int idx);
bool getState(int idx);
bool consumeChanged(int idx);       // true once after a state change (for MQTT)

}  // namespace Relays

#endif  // RELAYS_H
