#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <Arduino.h>
#include <time.h>

// NTP time sync. Starts SNTP once the upstream (STA) link is up and keeps
// local time in the configured POSIX timezone. Named "timekeeper" to avoid
// clashing with the libc <time.h> header.
namespace TimeKeeper {

void begin();                 // apply TZ from config (call after config.load())
void loop();                  // kick off SNTP once STA is connected
bool synced();                // true once a plausible wall-clock time is set
bool localNow(struct tm& out);  // local time; false while unsynced
String fmtNow();              // "Wed 10 Jun 14:32", or "--:--" while unsynced

}  // namespace TimeKeeper

#endif  // TIMEKEEPER_H
