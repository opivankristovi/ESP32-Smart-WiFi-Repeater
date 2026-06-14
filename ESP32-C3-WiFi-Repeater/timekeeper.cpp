#include "timekeeper.h"
#include "config.h"
#include "net.h"

namespace TimeKeeper {

// Anything before 2021 means the clock is still at its boot default.
static const time_t kSaneEpoch = 1609459200;

static bool sntpStarted = false;

void begin() {
  setenv("TZ", config.timecfg.tz, 1);
  tzset();
}

void loop() {
  if (sntpStarted || !config.timecfg.enabled) return;
  if (!Net::staConnected()) return;
  // SNTP keeps itself in sync from here on (periodic resync, reconnects).
  configTzTime(config.timecfg.tz, config.timecfg.server);
  sntpStarted = true;
  Serial.printf("NTP sync started (%s, TZ %s)\n", config.timecfg.server,
                config.timecfg.tz);
}

bool synced() { return time(nullptr) > kSaneEpoch; }

bool localNow(struct tm& out) {
  if (!synced()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, &out);
  return true;
}

String fmtNow() {
  struct tm t;
  if (!localNow(t)) return "--:--";
  char buf[24];
  strftime(buf, sizeof(buf), "%a %d %b %H:%M", &t);
  return String(buf);
}

}  // namespace TimeKeeper
