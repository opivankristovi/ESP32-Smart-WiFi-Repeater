#ifndef INPUTS_H
#define INPUTS_H

#include "config.h"

// NUM_INPUTS physical inputs (PIN_INPUT[]), each a digital push button or an
// analog sensor. Digital inputs are debounced centrally so both the relay rules
// (RELAY_BUTTON mode) and the MQTT publisher share one source of truth.
// Analog inputs are read by the sensors module instead.
namespace Inputs {

void begin();
void update();
bool getState(int i);
bool edge(int i);
bool consumeChanged(int i);

}  // namespace Inputs

#endif  // INPUTS_H
