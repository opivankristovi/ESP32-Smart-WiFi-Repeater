#include <Arduino.h>
#include <time.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  time_t now = 1700000000;
  struct tm out;

  unsigned long start = micros();
  for (int i = 0; i < 10000; i++) {
    localtime_r(&now, &out);
  }
  unsigned long end = micros();

  Serial.printf("Time per localtime_r: %f us\n", (end - start) / 10000.0);
}

void loop() {}
