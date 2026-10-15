#include <Arduino.h>
#include <time.h>

static time_t cachedTime = 0;
static struct tm cachedTm;

bool localNow_old(struct tm& out, time_t now) {
  localtime_r(&now, &out);
  return true;
}

bool localNow_new(struct tm& out, time_t now) {
  if (now != cachedTime) {
    localtime_r(&now, &cachedTm);
    cachedTime = now;
  }
  out = cachedTm;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  time_t now = 1700000000;
  struct tm out;

  unsigned long start = micros();
  for (int i = 0; i < 10000; i++) {
    localNow_old(out, now);
  }
  unsigned long end = micros();
  Serial.printf("Time old (same sec): %f us\n", (end - start) / 10000.0);

  start = micros();
  for (int i = 0; i < 10000; i++) {
    localNow_new(out, now);
  }
  end = micros();
  Serial.printf("Time new (same sec): %f us\n", (end - start) / 10000.0);
}

void loop() {}
