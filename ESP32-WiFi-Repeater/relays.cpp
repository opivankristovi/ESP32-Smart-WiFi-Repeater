#include "relays.h"
#include "sensors.h"
#include "timekeeper.h"
#include "inputs.h"

namespace Relays {

static bool state[2]   = {false, false};
static bool changed[2] = {false, false};

// Per-relay timer phase anchor.
static unsigned long timerAnchor[2] = {0, 0};

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
  for (int i = 0; i < 2; i++) {
    pinMode(PIN_RELAY[i], OUTPUT);
    state[i] = (config.relays[i].mode == RELAY_MANUAL)
                   ? config.relays[i].manualState
                   : false;
    writePin(i, state[i]);
    timerAnchor[i] = millis();
  }
}

// Sensor-threshold mode with hysteresis. Holds previous state inside the band.
static bool evalSensor(int idx, bool prev) {
  const RelayConfig& rc = config.relays[idx];
  bool ok = false;
  float v = Sensors::metricValue(rc.src, ok);
  if (!ok) return prev;
  float onLvl, offLvl;
  if (rc.cmp == 0) {            // ON when above level
    onLvl  = rc.level + rc.hyst;
    offLvl = rc.level - rc.hyst;
    if (v >= onLvl) return true;
    if (v <= offLvl) return false;
  } else {                     // ON when below level
    onLvl  = rc.level - rc.hyst;
    offLvl = rc.level + rc.hyst;
    if (v <= onLvl) return true;
    if (v >= offLvl) return false;
  }
  return prev;  // inside hysteresis band -> hold
}

// Clock-schedule mode: ON while local time is inside any enabled slot.
// Stays OFF until NTP has set the clock. Slots with offMin <= onMin wrap
// past midnight and belong to the day they start on.
static bool evalSchedule(int idx) {
  struct tm t;
  if (!TimeKeeper::localNow(t)) return false;
  int day  = (t.tm_wday + 6) % 7;        // tm_wday 0=Sun -> 0=Mon..6=Sun
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
  for (int i = 0; i < 2; i++) {
    const RelayConfig& rc = config.relays[i];
    bool desired = state[i];
    switch (rc.mode) {
      case RELAY_OFF:
        desired = false;
        break;
      case RELAY_MANUAL:
        // Driven by setState()/MQTT; keep current runtime state.
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
      case RELAY_BUTTON:
        if (Inputs::edge(i)) desired = !state[i];  // input N toggles relay N
        break;
      case RELAY_SCHEDULE:
        desired = evalSchedule(i);
        break;
    }
    apply(i, desired);
  }
}

void setState(int idx, bool on) {
  if (idx < 0 || idx > 1) return;
  apply(idx, on);
}

void toggle(int idx) {
  if (idx < 0 || idx > 1) return;
  apply(idx, !state[idx]);
}

bool getState(int idx) { return (idx >= 0 && idx <= 1) ? state[idx] : false; }

bool consumeChanged(int idx) {
  if (idx < 0 || idx > 1) return false;
  bool c = changed[idx];
  changed[idx] = false;
  return c;
}

}  // namespace Relays
