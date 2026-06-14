#ifndef NET_H
#define NET_H

#include <Arduino.h>
#include <IPAddress.h>

// WiFi: concurrent AP + STA with NAPT (the repeater function) and a captive
// portal DNS. Credentials live in the Phase-1 "repeater" NVS namespace.
namespace Net {

// Current credentials / state (read by the web UI for status + prefill).
extern String staSsid, staPass, apSsid, apPass;
extern bool   pwChanged;   // has the default AP password been changed?

void begin();        // load creds, start AP+STA, wait briefly, enable NAPT, start DNS
void loop();         // DNS pump + reconnect watchdog + NAPT re-enable

void startAp();              // (re)start the soft AP with current apSsid/apPass
void beginStaConnect();      // kick off a non-blocking upstream connection
void saveStaCreds(const String& ssid, const String& pass);
void saveApCreds(const String& ssid, const String& pass);

bool      staConnected();
IPAddress apIp();

}  // namespace Net

#endif  // NET_H
