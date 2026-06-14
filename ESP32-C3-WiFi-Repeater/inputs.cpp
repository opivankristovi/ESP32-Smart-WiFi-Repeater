#include "inputs.h"

namespace Inputs {

static bool level[NUM_INPUTS]   = {};
static bool changed[NUM_INPUTS] = {};
static bool edged[NUM_INPUTS]   = {};

static bool lastRaw[NUM_INPUTS] = {};
static unsigned long rawChangedAt[NUM_INPUTS] = {};
static const unsigned long kDebounceMs = 40;

static bool rawPressed(int i) {
  const InputConfig& c = config.inputs[i];
  if (c.type == INPUT_ANALOG) return false;
  return digitalRead(PIN_INPUT[i]) == (c.activeLow ? LOW : HIGH);
}

void begin() {
  for (int i = 0; i < NUM_INPUTS; i++) {
    if (!config.inputs[i].enabled) continue;
    if (config.inputs[i].type == INPUT_DIGITAL) {
      pinMode(PIN_INPUT[i],
              config.inputs[i].activeLow ? INPUT_PULLUP : INPUT);
    }
  }
}

void update() {
  for (int i = 0; i < NUM_INPUTS; i++) {
    edged[i] = false;
    if (!config.inputs[i].enabled || config.inputs[i].type != INPUT_DIGITAL) {
      level[i] = false;
      continue;
    }

    bool raw = rawPressed(i);
    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      rawChangedAt[i] = millis();
    }
    if (millis() - rawChangedAt[i] > kDebounceMs && raw != level[i]) {
      level[i] = raw;
      changed[i] = true;
      if (raw) edged[i] = true;
    }
  }
}

bool getState(int i) { return (i >= 0 && i < NUM_INPUTS) ? level[i] : false; }
bool edge(int i)     { return (i >= 0 && i < NUM_INPUTS) ? edged[i] : false; }

bool consumeChanged(int i) {
  if (i < 0 || i >= NUM_INPUTS) return false;
  bool c = changed[i];
  changed[i] = false;
  return c;
}

}  // namespace Inputs
