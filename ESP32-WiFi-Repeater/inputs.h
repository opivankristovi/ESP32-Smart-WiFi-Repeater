#ifndef INPUTS_H
#define INPUTS_H

#include "config.h"

// Two physical inputs (PIN_INPUT[2]), each a digital push button or a
// capacitive touch pad. Reads are debounced centrally so both the relay rules
// (RELAY_BUTTON mode) and the MQTT publisher share one source of truth.
namespace Inputs {

void begin();                 // configure pins for the enabled digital inputs
void update();                // sample + debounce each loop, before Relays::update()
bool getState(int i);         // current debounced pressed level (for MQTT)
bool edge(int i);             // rising edge this tick (relay button toggle)
bool consumeChanged(int i);   // true once after a level change (for MQTT)
int  touchRaw(int i);         // live touchRead() value, or -1 if not a touch input

}  // namespace Inputs

#endif  // INPUTS_H
