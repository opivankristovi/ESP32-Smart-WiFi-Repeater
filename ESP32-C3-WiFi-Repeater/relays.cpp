#include "relays.h"
#include "sensors.h"
#include "timekeeper.h"
#include "inputs.h"

namespace Relays {

static bool state[NUM_RELAYS]   = {};
static bool changed[NUM_RELAYS] = {};

static unsigned long timerAnchor[NUM_RELAYS] = {};

static void writePin(int idx, bool on) {
  bool level = config.relays[idx].activeLow ? !on : on;
  digitalWrite(PIN_RELAY[idx], level ? HIGH : LOW);
}

static void apply(int idx, bool on) {
  if (state[idx] != on) {
    state[idx] = on;
    changed[idx] = true;
  }
  writePin(idx, on);
}

void begin() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(PIN_RELAY[i], OUTPUT);
    state[i] = (config.relays[i].mode == RELAY_MANUAL)
                   ? config.relays[i].manualState
                   : false;
    writePin(i, state[i]);
    timerAnchor[i] = millis();
  }
}

static bool evalSensor(int idx, bool prev) {
  const RelayConfig& rc = config.relays[idx];
  bool ok = false;
  float v = Sensors::metricValue(rc.src, ok);
  if (!ok) return prev;
  float onLvl, offLvl;
  if (rc.cmp == 0) {
    onLvl  = rc.level + rc.hyst;
    offLvl = rc.level - rc.hyst;
    if (v >= onLvl) return true;
    if (v <= offLvl) return false;
  } else {
    onLvl  = rc.level - rc.hyst;
    offLvl = rc.level + rc.hyst;
    if (v <= onLvl) return true;
    if (v >= offLvl) return false;
  }
  return prev;
}

static bool evalSchedule(int idx) {
  struct tm t;
  if (!TimeKeeper::localNow(t)) return false;
  int day  = (t.tm_wday + 6) % 7;
  int prev = (day + 6) % 7;
  uint16_t m = t.tm_hour * 60 + t.tm_min;

  for (int k = 0; k < kSlotsPerRelay; k++) {
    const ScheduleSlot& s = config.relays[idx].sched[k];
    if (!s.enabled) continue;
    if (s.onMin < s.offMin) {
      if ((s.days & (1 << day)) && m >= s.onMin && m < s.offMin) return true;
    } else {
      if ((s.days & (1 << day)) && m >= s.onMin) return true;
      if ((s.days & (1 << prev)) && m < s.offMin) return true;
    }
  }
  return false;
}

void update() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    const RelayConfig& rc = config.relays[i];
    bool desired = state[i];
    switch (rc.mode) {
      case RELAY_OFF:
        desired = false;
        break;
      case RELAY_MANUAL:
        break;
      case RELAY_TIMER: {
        uint32_t on = rc.timerOnSec, off = rc.timerOffSec;
        uint32_t period = on + off;
        if (period == 0) { desired = false; break; }
        uint32_t pos = ((millis() - timerAnchor[i]) / 1000UL) % period;
        desired = pos < on;
        break;
      }
      case RELAY_SENSOR:
        desired = evalSensor(i, state[i]);
        break;
      case RELAY_BUTTON: {
        int bi = config.relays[i].buttonInput;
        if (bi >= 0 && bi < NUM_INPUTS && Inputs::edge(bi))
          desired = !state[i];
        break;
      }
      case RELAY_SCHEDULE:
        desired = evalSchedule(i);
        break;
    }
    apply(i, desired);
  }
}

void setState(int idx, bool on) {
  if (idx < 0 || idx >= NUM_RELAYS) return;
  apply(idx, on);
}

void toggle(int idx) {
  if (idx < 0 || idx >= NUM_RELAYS) return;
  apply(idx, !state[idx]);
}

bool getState(int idx) { return (idx >= 0 && idx < NUM_RELAYS) ? state[idx] : false; }

bool consumeChanged(int idx) {
  if (idx < 0 || idx >= NUM_RELAYS) return false;
  bool c = changed[idx];
  changed[idx] = false;
  return c;
}

}  // namespace Relays
