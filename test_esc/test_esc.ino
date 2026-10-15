#include <Arduino.h>

String esc_old(const String& s) {
  String o;
  o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default:  o += c;
    }
  }
  return o;
}

String esc_new(const String& s) {
  size_t len = s.length();
  size_t outLen = len;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (c == '&') outLen += 4;
    else if (c == '<' || c == '>') outLen += 3;
    else if (c == '"') outLen += 5;
  }
  if (outLen == len) return s;

  String o;
  o.reserve(outLen);
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default:  o += c;
    }
  }
  return o;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  String testStr = "ESP32-repeaterAP";

  unsigned long start = micros();
  for (int i = 0; i < 10000; i++) {
    String res = esc_old(testStr);
  }
  unsigned long end = micros();
  Serial.printf("Time old: %f us\n", (end - start) / 10000.0);

  start = micros();
  for (int i = 0; i < 10000; i++) {
    String res = esc_new(testStr);
  }
  end = micros();
  Serial.printf("Time new: %f us\n", (end - start) / 10000.0);
}

void loop() {}
