#include "inputs.h"

namespace Inputs {

// Debounced level (pressed = true) and transient/edge flags, per input.
static bool level[2]   = {false, false};
static bool changed[2] = {false, false};  // level change pending MQTT publish
static bool edged[2]   = {false, false};  // rising edge seen this tick

// Debounce state: last raw read and when it last changed.
static bool lastRaw[2] = {false, false};
static unsigned long rawChangedAt[2] = {0, 0};
static const unsigned long kDebounceMs = 40;

// Raw "pressed" read for one input, before debouncing.
static bool rawPressed(int i) {
  const InputConfig& c = config.inputs[i];
  if (c.type == INPUT_TOUCH) {
    // Classic ESP32: touchRead() drops when the pad is touched.
    return touchRead(PIN_INPUT[i]) < (int)c.touchThresh;
  }
  return digitalRead(PIN_INPUT[i]) == (c.activeLow ? LOW : HIGH);
}

void begin() {
  for (int i = 0; i < 2; i++) {
    if (!config.inputs[i].enabled) continue;
    if (config.inputs[i].type == INPUT_DIGITAL) {
      pinMode(PIN_INPUT[i],
              config.inputs[i].activeLow ? INPUT_PULLUP : INPUT);
    }
    // Touch inputs need no pin setup; touchRead() drives the pad itself.
  }
}

void update() {
  for (int i = 0; i < 2; i++) {
    edged[i] = false;  // edge is valid for the current tick only
    if (!config.inputs[i].enabled) { level[i] = false; continue; }

    bool raw = rawPressed(i);
    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      rawChangedAt[i] = millis();
    }
    if (millis() - rawChangedAt[i] > kDebounceMs && raw != level[i]) {
      level[i] = raw;
      changed[i] = true;
      if (raw) edged[i] = true;  // settled into "pressed" -> rising edge
    }
  }
}

bool getState(int i) { return (i >= 0 && i < 2) ? level[i] : false; }
bool edge(int i)     { return (i >= 0 && i < 2) ? edged[i] : false; }

bool consumeChanged(int i) {
  if (i < 0 || i >= 2) return false;
  bool c = changed[i];
  changed[i] = false;
  return c;
}

int touchRaw(int i) {
  if (i < 0 || i >= 2) return -1;
  if (config.inputs[i].type != INPUT_TOUCH) return -1;
  return touchRead(PIN_INPUT[i]);
}

}  // namespace Inputs
