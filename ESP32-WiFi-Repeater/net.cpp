#include "net.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>

// NAPT (NAT) headers differ between Arduino-ESP32 core 3.x and 2.x.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #include "esp_netif.h"
#else
  #include "lwip/lwip_napt.h"
#endif

namespace Net {

String staSsid, staPass, apSsid, apPass;
bool   pwChanged = false;

static const char* kDefaultApSsid = "ESP32-repeaterAP";
static const char* kDefaultApPass = "12345678";

static const IPAddress kApIp(192, 168, 4, 1);
static const IPAddress kApGateway(192, 168, 4, 1);
static const IPAddress kApSubnet(255, 255, 255, 0);
static const byte kDnsPort = 53;

static const unsigned long kConnectTimeoutMs = 15000;
static const unsigned long kReconnectEveryMs = 20000;

static DNSServer dnsServer;
static bool naptEnabled = false;
static unsigned long lastReconnectAttempt = 0;

// ---- Persistent credentials ------------------------------------------------
static void loadCreds() {
  Preferences prefs;
  prefs.begin("repeater", true);
  staSsid   = prefs.getString("sta_ssid", "");
  staPass   = prefs.getString("sta_pass", "");
  apSsid    = prefs.getString("ap_ssid", kDefaultApSsid);
  apPass    = prefs.getString("ap_pass", kDefaultApPass);
  pwChanged = prefs.getBool("pw_changed", false);
  prefs.end();
}

void saveStaCreds(const String& ssid, const String& pass) {
  Preferences prefs;
  prefs.begin("repeater", false);
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  prefs.end();
  staSsid = ssid;
  staPass = pass;
}

void saveApCreds(const String& ssid, const String& pass) {
  Preferences prefs;
  prefs.begin("repeater", false);
  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", pass);
  pwChanged = (pass != kDefaultApPass);
  prefs.putBool("pw_changed", pwChanged);
  prefs.end();
  apSsid = ssid;
  apPass = pass;
}

// ---- NAPT (the actual repeating) ------------------------------------------
static void enableNapt() {
  if (naptEnabled) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap != nullptr && esp_netif_napt_enable(ap) == ESP_OK) {
    naptEnabled = true;
  }
#else
  ip_napt_enable_no(SOFTAP_IF, 1);
  naptEnabled = true;
#endif
  Serial.println(naptEnabled ? "NAPT enabled - AP clients routed to upstream"
                             : "NAPT enable FAILED");
}

// ---- WiFi ------------------------------------------------------------------
void startAp() {
  WiFi.softAPConfig(kApIp, kApGateway, kApSubnet);
  WiFi.softAP(apSsid.c_str(), apPass.c_str());
  Serial.printf("AP started: %s  (IP %s)\n", apSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
}

void beginStaConnect() {
  if (staSsid.isEmpty()) return;
  naptEnabled = false;  // re-enable once the new link is up
  Serial.printf("Connecting to upstream '%s'...\n", staSsid.c_str());
  WiFi.begin(staSsid.c_str(), staPass.c_str());
}

bool      staConnected() { return WiFi.status() == WL_CONNECTED; }
IPAddress apIp()         { return kApIp; }

// ---- Lifecycle -------------------------------------------------------------
void begin() {
  loadCreds();
  WiFi.mode(WIFI_AP_STA);
  startAp();
  beginStaConnect();

  unsigned long start = millis();
  while (!staSsid.isEmpty() && WiFi.status() != WL_CONNECTED &&
         millis() - start < kConnectTimeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Upstream connected, IP %s\n",
                  WiFi.localIP().toString().c_str());
    enableNapt();
  }

  dnsServer.start(kDnsPort, "*", kApIp);
}

void loop() {
  dnsServer.processNextRequest();

  if (!staSsid.isEmpty() && WiFi.status() != WL_CONNECTED) {
    naptEnabled = false;
    if (millis() - lastReconnectAttempt > kReconnectEveryMs) {
      lastReconnectAttempt = millis();
      Serial.println("Upstream down, retrying...");
      WiFi.disconnect();
      beginStaConnect();
    }
  } else if (WiFi.status() == WL_CONNECTED && !naptEnabled) {
    enableNapt();
  }
}

}  // namespace Net
