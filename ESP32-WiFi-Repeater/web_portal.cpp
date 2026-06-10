#include "web_portal.h"
#include "web_page.h"
#include "config.h"
#include "net.h"
#include "timekeeper.h"
#include "sensors.h"
#include "relays.h"
#include "mqtt.h"

#include <WiFi.h>
#include <WebServer.h>

namespace WebPortal {

static WebServer server(80);

// ---------------------------------------------------------------------------
// Small HTML builder helpers
// ---------------------------------------------------------------------------
static String esc(const String& s) {
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

// Info badge with a CSS hover/tap tooltip (see .info in web_page.h).
static String info(const String& tip) {
  return " <span class=\"info\" tabindex=\"0\" data-tip=\"" + esc(tip) +
         "\">i</span>";
}

static String checkbox(const String& name, const String& label, bool on,
                       const String& tip = "") {
  return "<div class=\"inline\"><input type=\"checkbox\" id=\"" + name +
         "\" name=\"" + name + "\"" + (on ? " checked" : "") +
         "><label for=\"" + name + "\">" + label + "</label>" +
         (tip.length() ? info(tip) : "") + "</div>";
}

static String textField(const String& name, const String& label,
                        const String& value, const String& type = "text",
                        const String& extra = "", const String& tip = "") {
  return "<label for=\"" + name + "\">" + label +
         (tip.length() ? info(tip) : "") + "</label><input id=\"" + name +
         "\" name=\"" + name + "\" type=\"" + type + "\" value=\"" + esc(value) +
         "\" " + extra + ">";
}

// Alert-threshold fieldset, labelled with the quantity it sets and its unit.
static String thrFields(const String& prefix, const Threshold& t,
                        const String& quantity, const String& unit) {
  String u = unit.length() ? (" <span class=\"unit\">" + unit + "</span>") : "";
  String s = "<fieldset><legend>" + quantity + " alerts</legend>";
  s += "<div class=\"inline\"><input type=\"checkbox\" name=\"" + prefix +
       "_le\"" + (t.lowEnabled ? " checked" : "") +
       "><label>Low &le;</label><input type=\"number\" step=\"any\" name=\"" +
       prefix + "_lo\" value=\"" + String(t.low) + "\">" + u + "</div>";
  s += "<div class=\"inline\"><input type=\"checkbox\" name=\"" + prefix +
       "_he\"" + (t.highEnabled ? " checked" : "") +
       "><label>High &ge;</label><input type=\"number\" step=\"any\" name=\"" +
       prefix + "_hi\" value=\"" + String(t.high) + "\">" + u + "</div>";
  s += "</fieldset>";
  return s;
}

// Unit strings for the configured sensors.
static String bmeTempUnit() { return config.bme280.tempUnit == 'F' ? "°F" : "°C"; }
static String dsTempUnit()  { return config.ds18b20.tempUnit == 'F' ? "°F" : "°C"; }
static String presUnit()    { return config.bme280.pressureInHg ? "inHg" : "hPa"; }
static String analogUnit(AnalogScale s) {
  return s == ANALOG_VOLTAGE ? "V" : (s == ANALOG_PERCENT ? "%" : "");
}

static String sourceOptions(MetricSource sel) {
  struct { MetricSource v; const char* n; } items[] = {
      {SRC_NONE, "(none)"},       {SRC_BME_TEMP, "BME280 temperature"},
      {SRC_BME_HUM, "BME280 humidity"}, {SRC_BME_PRES, "BME280 pressure"},
      {SRC_DS_TEMP, "DS18B20 temperature"}, {SRC_ANALOG1, "Analog 1"},
      {SRC_ANALOG2, "Analog 2"}};
  String o;
  for (auto& it : items) {
    o += "<option value=\"" + String((int)it.v) + "\"" +
         (sel == it.v ? " selected" : "") + ">" + it.n + "</option>";
  }
  return o;
}

// ---- Time-of-day helpers (clock-schedule slots) -----------------------------
static String minToHHMM(uint16_t m) {
  char b[6];
  snprintf(b, sizeof(b), "%02u:%02u", m / 60 % 24, m % 60);
  return String(b);
}

static uint16_t parseHHMM(const String& s) {
  int colon = s.indexOf(':');
  if (colon < 1) return 0;
  int h = s.substring(0, colon).toInt();
  int m = s.substring(colon + 1).toInt();
  return constrain(h, 0, 23) * 60 + constrain(m, 0, 59);
}

// Common timezones, stored as POSIX TZ strings (DST rules included).
struct TzEntry { const char* name; const char* tz; };
static const TzEntry kTimezones[] = {
    {"Brussels / Paris / Berlin (CET)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"London / Dublin (GMT/BST)", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Athens / Helsinki (EET)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"UTC", "UTC0"},
    {"Moscow (MSK)", "MSK-3"},
    {"Dubai (GST)", "<+04>-4"},
    {"India (IST)", "IST-5:30"},
    {"China (CST)", "CST-8"},
    {"Japan (JST)", "JST-9"},
    {"Sydney (AEST/AEDT)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Auckland (NZST/NZDT)", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"New York (EST/EDT)", "EST5EDT,M3.2.0,M11.1.0"},
    {"Chicago (CST/CDT)", "CST6CDT,M3.2.0,M11.1.0"},
    {"Denver (MST/MDT)", "MST7MDT,M3.2.0,M11.1.0"},
    {"Phoenix (MST)", "MST7"},
    {"Los Angeles (PST/PDT)", "PST8PDT,M3.2.0,M11.1.0"},
    {"S&atilde;o Paulo (BRT)", "<-03>3"},
};

// ---------------------------------------------------------------------------
// Threshold parsing
// ---------------------------------------------------------------------------
static void parseThr(const String& prefix, Threshold& t) {
  t.lowEnabled  = server.hasArg(prefix + "_le");
  t.low         = server.arg(prefix + "_lo").toFloat();
  t.highEnabled = server.hasArg(prefix + "_he");
  t.high        = server.arg(prefix + "_hi").toFloat();
}

// ---------------------------------------------------------------------------
// Page sections
// ---------------------------------------------------------------------------
static String warningBanner() {
  if (Net::pwChanged) return "";
  return "<div class=\"warning\">&#9888; This repeater is using the "
         "<strong>default access-point password</strong>. Anyone nearby can "
         "join it. Change at least the <strong>AP password</strong> below "
         "&mdash; this notice disappears once you do.</div>";
}

// Sticky app bar with live connection dots, plus the tab pill bar.
static String appShell() {
  String wifiDot = Net::staConnected() ? "ok" : "";
  String mqttDot = (config.mqtt.enabled && Mqtt::connected()) ? "ok" : "";
  String s = "<header class=\"hdr\"><div class=\"appbar\">"
             "<h1>ESP32 Repeater</h1>"
             "<span class=\"dotlbl\"><span class=\"dot " + wifiDot +
             "\"></span>WiFi</span>"
             "<span class=\"dotlbl\"><span class=\"dot " + mqttDot +
             "\"></span>MQTT</span></div>";
  struct { const char* id; const char* label; } tabs[] = {
      {"home", "Home"},     {"network", "Network"}, {"mqtt", "MQTT"},
      {"sensors", "Sensors"}, {"relays", "Relays"}, {"system", "System"}};
  s += "<nav class=\"nav\">";
  for (auto& t : tabs) {
    s += "<button type=\"button\" data-tab=\"" + String(t.id) +
         "\" onclick=\"showTab('" + t.id + "')\">" + t.label + "</button>";
  }
  s += "</nav></header>";
  return s;
}

static String statusCard() {
  String s = "<div class=\"status\">Upstream ";
  if (Net::staConnected()) {
    s += "<span class=\"pill ok\">connected</span> <strong>" +
         esc(Net::staSsid) + "</strong> &middot; IP " +
         WiFi.localIP().toString();
  } else if (!Net::staSsid.isEmpty()) {
    s += "<span class=\"pill bad\">offline</span> <strong>" +
         esc(Net::staSsid) + "</strong> &middot; check password / range";
  } else {
    s += "<span class=\"pill idle\">not set up</span> pick a network in the "
         "Network tab";
  }
  s += "<br>MQTT ";
  s += config.mqtt.enabled
           ? (Mqtt::connected() ? "<span class=\"pill ok\">connected</span>"
                                : "<span class=\"pill bad\">offline</span>")
           : "<span class=\"pill idle\">disabled</span>";
  s += " &middot; Clock ";
  s += TimeKeeper::synced()
           ? "<span class=\"pill ok\">" + TimeKeeper::fmtNow() + "</span>"
           : "<span class=\"pill idle\">not synced</span>";
  s += "</div>";
  return "<div class=\"card\"><h2>Status</h2>" + s +
         "<h2 style=\"margin-top:1rem\">Live readings</h2>"
         "<div id=\"live\"></div></div>";
}

static String wifiCard() {
  int n = WiFi.scanNetworks();
  String opts;
  if (n <= 0) {
    opts = "<option value=\"\">(no networks found - reload to rescan)</option>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      opts += "<option value=\"" + esc(ssid) + "\"" +
              (ssid == Net::staSsid ? " selected" : "") + ">" + esc(ssid) +
              " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }
  }
  WiFi.scanDelete();
  return "<div class=\"card\"><h2>Connect to a network</h2>"
         "<form method=\"POST\" action=\"/save\">"
         "<label for=\"ssid\">WiFi network</label>"
         "<select id=\"ssid\" name=\"ssid\">" + opts + "</select>" +
         textField("pass", "Password", "", "password",
                   "placeholder=\"Network password\"") +
         "<button type=\"submit\">Connect</button></form></div>";
}

static String apCard() {
  return "<div class=\"card\"><h2>Repeater access point</h2>"
         "<form method=\"POST\" action=\"/apconfig\">" +
         textField("apssid", "AP name (SSID)", Net::apSsid, "text",
                   "maxlength=\"31\"") +
         textField("appass", "AP password", "", "password",
                   "minlength=\"8\" maxlength=\"63\" "
                   "placeholder=\"At least 8 characters\"",
                   "The Wi-Fi password devices use to join this repeater. "
                   "Minimum 8 characters. Change it from the default to clear "
                   "the security warning.") +
         "<button type=\"submit\">Save AP settings</button>"
         "<p class=\"muted\">Saving reboots the access point.</p>"
         "</form></div>";
}

static String mqttCard() {
  const MqttConfig& m = config.mqtt;
  String s = "<div class=\"card\"><h2>MQTT</h2>"
             "<form method=\"POST\" action=\"/mqtt\">";
  s += checkbox("mq_en", "Enable MQTT publishing", m.enabled,
                "Publish sensor and relay data to an MQTT broker over the "
                "upstream Wi-Fi. Only active once connected to your network.");
  s += textField("mq_host", "Broker host", m.host, "text",
                 "placeholder=\"192.168.1.10 or broker.local\"",
                 "IP address or hostname of your MQTT broker (e.g. the "
                 "Mosquitto add-on in Home Assistant).");
  s += "<div class=\"row\"><div>" +
       textField("mq_port", "Port", String(m.port), "number", "",
                 "MQTT broker port. Default 1883 (8883 is TLS, not used here).") +
       "</div><div>" +
       textField("mq_ival", "Publish every (s)", String(m.publishIntervalSec),
                 "number", "min=\"5\"",
                 "How often sensor values are published, in seconds.") +
       "</div></div>";
  s += "<div class=\"row\"><div>" +
       textField("mq_user", "Username", m.user, "text", "",
                 "Leave blank if your broker allows anonymous access.") +
       "</div><div>" +
       textField("mq_pass", "Password", "", "password",
                 "placeholder=\"(unchanged if blank)\"",
                 "Broker password. Leave blank to keep the stored one.") +
       "</div></div>";
  s += textField("mq_base", "Base topic", m.baseTopic, "text", "",
                 "Prefix for every topic, e.g. esp32repeater. Full topic is "
                 "<base>/<clientId>/...");
  s += textField("mq_cid", "Client ID", m.clientId, "text", "",
                 "Unique MQTT client id for this device. Also used in the "
                 "topic path and Home Assistant entity ids.");
  s += "<fieldset><legend>Home Assistant</legend>";
  s += checkbox("mq_ha", "Publish MQTT discovery (auto-add entities)",
                m.haDiscovery,
                "Automatically register sensors and relays in Home Assistant "
                "via MQTT discovery — no manual YAML needed.");
  s += textField("mq_hap", "Discovery prefix", m.haPrefix, "text", "",
                 "Must match Home Assistant's MQTT discovery prefix. The "
                 "default 'homeassistant' is correct for almost everyone.");
  s += "</fieldset>";
  s += "<button type=\"submit\">Save MQTT</button>"
       "<p class=\"muted\">Topics: &lt;base&gt;/&lt;clientId&gt;/sensor/... , "
       "/relay/N/state, /relay/N/set. Saving reboots.</p></form></div>";
  return s;
}

static String unitSelect(const String& name, char unit) {
  return "<label for=\"" + name + "\">Temperature unit</label><select id=\"" +
         name + "\" name=\"" + name + "\"><option value=\"C\"" +
         (unit == 'C' ? " selected" : "") + ">&deg;C</option><option value=\"F\"" +
         (unit == 'F' ? " selected" : "") + ">&deg;F</option></select>";
}

static String sensorsCard() {
  String s = "<div class=\"card\"><h2>Sensors</h2>"
             "<form method=\"POST\" action=\"/sensors\">";

  // BME280
  s += "<fieldset><legend>BME280 (I2C 21/22)</legend>";
  s += checkbox("bme_en", "Enabled", config.bme280.enabled);
  s += "<label for=\"bme_addr\">I2C address" +
       info("Most BME280 boards are 0x76; some are 0x77. If the sensor isn't "
            "detected, try the other address.") +
       "</label><select id=\"bme_addr\" "
       "name=\"bme_addr\"><option value=\"118\"" +
       String(config.bme280.address == 0x76 ? " selected" : "") +
       ">0x76</option><option value=\"119\"" +
       String(config.bme280.address == 0x77 ? " selected" : "") +
       ">0x77</option></select>";
  s += unitSelect("bme_tu", config.bme280.tempUnit);
  s += checkbox("bme_inhg", "Pressure in inHg (else hPa)",
                config.bme280.pressureInHg,
                "Unit for the pressure reading. Off = hPa (millibar), "
                "on = inches of mercury.");
  s += thrFields("bme_t", config.bme280.tTemp, "Temperature", bmeTempUnit());
  s += thrFields("bme_h", config.bme280.tHum, "Humidity", "%");
  s += thrFields("bme_p", config.bme280.tPres, "Pressure", presUnit());
  s += "</fieldset>";

  // DS18B20
  s += "<fieldset><legend>DS18B20 (1-wire GPIO4)</legend>";
  s += checkbox("ds_en", "Enabled", config.ds18b20.enabled,
                "Dallas 1-wire temperature sensor on GPIO4. Needs a 4.7k "
                "pull-up resistor from data to 3V3.");
  s += unitSelect("ds_tu", config.ds18b20.tempUnit);
  s += thrFields("ds_t", config.ds18b20.tTemp, "Temperature", dsTempUnit());
  s += "</fieldset>";

  // Analog 1 & 2
  const char* scaleNames[] = {"Raw (0-4095)", "Percent", "Voltage (V)"};
  for (int i = 0; i < 2; i++) {
    String p = "a" + String(i + 1);
    String n = String(i + 1);
    const AnalogConfig& a = config.analog[i];
    s += "<fieldset><legend>Analog " + n + " (GPIO" + String(PIN_ANALOG[i]) +
         ")</legend>";
    s += checkbox(p + "_en", "Enabled", a.enabled);
    s += textField(p + "_lbl", "Label", a.label, "text", "",
                   "Friendly name shown in live readings and Home Assistant "
                   "(e.g. \"Light level\").");
    s += "<label>Scaling" +
         info("Raw = ADC counts 0-4095. Percent = map raw min/max to 0-100%. "
              "Voltage = measured volts.") +
         "</label><select id=\"" + p + "_scale\" name=\"" + p +
         "_scale\" onchange=\"toggleAnalog(" + n + ")\">";
    for (int k = 0; k < 3; k++) {
      s += "<option value=\"" + String(k) + "\"" +
           ((int)a.scale == k ? " selected" : "") + ">" + scaleNames[k] +
           "</option>";
    }
    s += "</select>";
    // Percent-only calibration block (shown/hidden by toggleAnalog()).
    s += "<div id=\"" + p + "_cal\">";
    s += "<div class=\"row\"><div>" +
         textField(p + "_rmin", "Raw min (0%)", String(a.rawMin), "number", "",
                   "ADC count that maps to 0%. Tip: expose the sensor to its "
                   "lowest input, then press \"Set as min\".") +
         "</div><div>" +
         textField(p + "_rmax", "Raw max (100%)", String(a.rawMax), "number", "",
                   "ADC count that maps to 100%. Tip: drive the sensor to its "
                   "highest input, then press \"Set as max\".") +
         "</div></div>";
    s += "<div class=\"cal-btns\">"
         "<button type=\"button\" class=\"btn-sm\" onclick=\"setCal(" +
         String(i) + ",'rmin')\">Set as min</button>"
         "<button type=\"button\" class=\"btn-sm\" onclick=\"setCal(" +
         String(i) + ",'rmax')\">Set as max</button></div>";
    s += "</div>";  // end cal block
    s += thrFields(p + "_t", a.thr,
                   String(a.label).length() ? String(a.label) : ("Analog " + n),
                   analogUnit(a.scale));
    s += "</fieldset>";
  }

  // Button
  s += "<fieldset><legend>Button input (GPIO25)</legend>";
  s += checkbox("btn_en", "Enabled", config.button.enabled,
                "A momentary push button to GND. Set a relay's mode to "
                "\"Button toggle\" to flip it on each press.");
  s += checkbox("btn_al", "Active low (INPUT_PULLUP)", config.button.activeLow,
                "On (recommended): button wired between the pin and GND, using "
                "the internal pull-up. Off: button drives the pin HIGH.");
  s += "</fieldset>";

  s += "<button type=\"submit\">Save sensors</button>"
       "<p class=\"muted\">Saving reboots to apply pin changes.</p>"
       "</form></div>";
  return s;
}

// Wraps mode-specific relay options; modeChanged() in PAGE_FOOT shows only
// the block matching the selected mode. Inactive blocks render hidden so the
// page doesn't flash every option before the script runs.
static String modeSec(int relayN, int mode, RelayMode current,
                      const String& inner) {
  return "<div class=\"msec\" data-r=\"" + String(relayN) + "\" data-m=\"" +
         String(mode) + "\"" +
         ((int)current == mode ? "" : " style=\"display:none\"") + ">" +
         inner + "</div>";
}

static String scheduleSlots(const String& p, const RelayConfig& r) {
  const char* dayNames[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
  String s = "<p class=\"muted\" style=\"margin:.2rem 0 .4rem\">Switches by "
             "the clock (set the timezone in the System tab). Stays OFF until "
             "the time is NTP-synced. Spans past midnight are fine.</p>";
  for (int k = 0; k < kSlotsPerRelay; k++) {
    const ScheduleSlot& sl = r.sched[k];
    String sp = p + "_s" + String(k);
    s += "<fieldset><legend>Slot " + String(k + 1) + "</legend>";
    s += checkbox(sp + "_en", "Enabled", sl.enabled);
    s += "<div class=\"row\"><div><label>ON at</label><input type=\"time\" "
         "name=\"" + sp + "_on\" value=\"" + minToHHMM(sl.onMin) + "\"></div>"
         "<div><label>OFF at</label><input type=\"time\" name=\"" + sp +
         "_off\" value=\"" + minToHHMM(sl.offMin) + "\"></div></div>";
    s += "<div class=\"days\">";
    for (int d = 0; d < 7; d++) {
      s += "<label class=\"chip\"><input type=\"checkbox\" name=\"" + sp +
           "_d" + String(d) + "\"" + ((sl.days >> d) & 1 ? " checked" : "") +
           "><span>" + dayNames[d] + "</span></label>";
    }
    s += "</div></fieldset>";
  }
  return s;
}

static String relaysCard() {
  String s = "<div class=\"card\"><h2>Relays / SSR outputs</h2>"
             "<form method=\"POST\" action=\"/relays\">";
  const char* modeNames[] = {"Off",           "Manual",
                             "Timer (cyclic)", "Sensor threshold",
                             "Button toggle",  "Clock schedule"};
  for (int i = 0; i < 2; i++) {
    int n = i + 1;
    String p = "r" + String(n);
    const RelayConfig& r = config.relays[i];
    s += "<fieldset><legend>Relay " + String(n) + " (GPIO" +
         String(PIN_RELAY[i]) + ")</legend>";
    s += textField(p + "_name", "Name", r.name, "text", "",
                   "Friendly name for this output, shown in Home Assistant.");
    s += "<label>Mode" +
         info("Off = always off. Manual = controlled by you / MQTT. "
              "Timer = cycle on/off. Sensor threshold = switch on a reading. "
              "Button toggle = flip on each button press. Clock schedule = "
              "on/off at set times of day (needs NTP time).") +
         "</label><select id=\"" + p + "_mode\" name=\"" + p +
         "_mode\" onchange=\"modeChanged(" + String(n) + ")\">";
    for (int k = 0; k < 6; k++) {
      s += "<option value=\"" + String(k) + "\"" +
           ((int)r.mode == k ? " selected" : "") + ">" + modeNames[k] +
           "</option>";
    }
    s += "</select>";
    s += checkbox(p + "_al", "Active low", r.activeLow,
                  "Enable if your relay/SSR board switches ON when the pin is "
                  "LOW (common for blue relay boards).");

    // Manual mode
    s += modeSec(n, RELAY_MANUAL, r.mode,
                 checkbox(p + "_man", "Start ON at power-up", r.manualState,
                          "The output's state right after boot."));

    // Cyclic timer mode
    String timer =
        "<div class=\"row\"><div>" +
        textField(p + "_ton", "ON for (s)", String(r.timerOnSec), "number", "",
                  "Seconds the output stays ON each cycle.") +
        "</div><div>" +
        textField(p + "_toff", "OFF for (s)", String(r.timerOffSec), "number",
                  "", "Seconds the output stays OFF each cycle.") +
        "</div></div>";
    s += modeSec(n, RELAY_TIMER, r.mode, timer);

    // Sensor-threshold mode
    String sensor =
        "<label>Sensor source" +
        info("Which reading drives this output.") + "</label><select name=\"" +
        p + "_src\">" + sourceOptions(r.src) + "</select>";
    sensor += "<label>Comparator" +
              info("Whether the output turns ON above or below the level.") +
              "</label><select name=\"" + p + "_cmp\">"
              "<option value=\"0\"" + String(r.cmp == 0 ? " selected" : "") +
              ">ON when above level</option><option value=\"1\"" +
              String(r.cmp == 1 ? " selected" : "") +
              ">ON when below level</option></select>";
    sensor += "<div class=\"row\"><div>" +
              textField(p + "_lvl", "Level", String(r.level), "number",
                        "step=\"any\"",
                        "Threshold value, in the source sensor's unit.") +
              "</div><div>" +
              textField(p + "_hyst", "Hysteresis", String(r.hyst), "number",
                        "step=\"any\"",
                        "Dead-band around the level to stop rapid on/off "
                        "chatter, e.g. ON at 28°, OFF at 26° = level 27, "
                        "hysteresis 1.") +
              "</div></div>";
    s += modeSec(n, RELAY_SENSOR, r.mode, sensor);

    // Clock-schedule mode
    s += modeSec(n, RELAY_SCHEDULE, r.mode, scheduleSlots(p, r));

    s += checkbox(p + "_mqtt",
                  "Allow MQTT control (.../relay/" + String(n) + "/set)",
                  r.allowMqtt,
                  "Let an MQTT ON/OFF/TOGGLE message switch this output. In "
                  "Home Assistant it then appears as a switch (else read-only).");
    s += "</fieldset>";
  }
  s += "<button type=\"submit\">Save relays</button>"
       "<p class=\"muted\">Saving reboots to apply pin changes.</p>"
       "</form></div>";
  return s;
}

static String timeCard() {
  const TimeConfig& t = config.timecfg;
  String s = "<div class=\"card\"><h2>Time &amp; timezone</h2>"
             "<form method=\"POST\" action=\"/time\">";
  s += checkbox("tm_en", "Sync time via NTP", t.enabled,
                "Fetch the current time from the internet once the repeater "
                "is connected to your WiFi. Needed for clock-schedule relays.");
  s += textField("tm_srv", "NTP server", t.server, "text", "",
                 "Time server to query. The default pool.ntp.org is fine for "
                 "almost everyone.");
  s += "<label for=\"tm_tz\">Timezone" +
       info("Local timezone for the clock and relay schedules. Daylight "
            "saving is handled automatically.") +
       "</label><select id=\"tm_tz\" name=\"tm_tz\">";
  bool matched = false;
  for (auto& z : kTimezones) {
    bool sel = !matched && strcmp(t.tz, z.tz) == 0;
    if (sel) matched = true;
    s += "<option value=\"" + esc(z.tz) + "\"" + (sel ? " selected" : "") +
         ">" + z.name + "</option>";
  }
  s += "</select>";
  s += "<p class=\"muted\">Current device time: " +
       (TimeKeeper::synced() ? TimeKeeper::fmtNow() : String("not synced")) +
       "</p>";
  s += "<button type=\"submit\">Save time settings</button>"
       "<p class=\"muted\">Saving reboots.</p></form></div>";
  return s;
}

static String dangerCard() {
  return "<div class=\"card\"><h2>Maintenance</h2>"
         "<form method=\"POST\" action=\"/factoryreset\" "
         "onsubmit=\"return confirm('Erase all settings and reboot to "
         "defaults?');\">"
         "<button type=\"submit\" class=\"danger\">Factory reset</button>"
         "<p class=\"muted\">Clears WiFi, AP, MQTT, sensor and relay settings "
         "and restarts.</p></form></div>";
}

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------
// "anchor" is the tab to return to after the reboot (e.g. "relays").
static void sendRebootNotice(const String& msg, const String& anchor = "") {
  String url = anchor.length() ? ("/#" + anchor) : "/";
  String html = FPSTR(PAGE_HEAD);
  html += "<div class=\"card\"><h2>Saved</h2><p>" + msg +
          "</p><p class=\"muted\">Rebooting&hellip; rejoin the AP and this page "
          "reloads.</p><meta http-equiv=\"refresh\" content=\"6;url=" + url +
          "\"></div>";
  html += FPSTR(PAGE_FOOT);
  server.send(200, "text/html", html);
  delay(400);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------
// The full page is large; stream it card by card to keep peak heap low.
static void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(PAGE_HEAD);
  server.sendContent(appShell());
  server.sendContent("<section class=\"tab\" id=\"tab-home\">" +
                     warningBanner() + statusCard() + "</section>");
  server.sendContent("<section class=\"tab\" id=\"tab-network\">" +
                     wifiCard() + apCard() + "</section>");
  server.sendContent("<section class=\"tab\" id=\"tab-mqtt\">" + mqttCard() +
                     "</section>");
  server.sendContent("<section class=\"tab\" id=\"tab-sensors\">" +
                     sensorsCard() + "</section>");
  server.sendContent("<section class=\"tab\" id=\"tab-relays\">" +
                     relaysCard() + "</section>");
  server.sendContent("<section class=\"tab\" id=\"tab-system\">" + timeCard() +
                     dangerCard() + "</section>");
  server.sendContent_P(PAGE_FOOT);
  server.sendContent("");  // terminate chunked response
}

static void handleSaveWifi() {
  String ssid = server.arg("ssid");
  if (ssid.isEmpty()) {
    server.send(400, "text/html",
                "<p>No network selected. <a href=\"/\">Back</a></p>");
    return;
  }
  Net::saveStaCreds(ssid, server.arg("pass"));
  String html = FPSTR(PAGE_HEAD);
  html += "<div class=\"card\"><h2>Connecting&hellip;</h2><p>Joining <b>" +
          esc(ssid) +
          "</b>. This page reloads shortly.</p>"
          "<meta http-equiv=\"refresh\" content=\"8;url=/\"></div>";
  html += FPSTR(PAGE_FOOT);
  server.send(200, "text/html", html);
  WiFi.disconnect();
  Net::beginStaConnect();
}

static void handleApConfig() {
  String ssid = server.arg("apssid");
  String pass = server.arg("appass");
  if (ssid.isEmpty() || pass.length() < 8) {
    server.send(400, "text/html",
                "<p>AP name required and password must be at least 8 "
                "characters. <a href=\"/\">Back</a></p>");
    return;
  }
  Net::saveApCreds(ssid, pass);
  sendRebootNotice("Access point updated to <b>" + esc(ssid) + "</b>.",
                   "network");
}

static void handleMqtt() {
  MqttConfig& m = config.mqtt;
  m.enabled = server.hasArg("mq_en");
  strlcpy(m.host, server.arg("mq_host").c_str(), sizeof(m.host));
  m.port = server.arg("mq_port").toInt();
  if (m.port == 0) m.port = 1883;
  strlcpy(m.user, server.arg("mq_user").c_str(), sizeof(m.user));
  if (server.arg("mq_pass").length())  // keep existing if left blank
    strlcpy(m.pass, server.arg("mq_pass").c_str(), sizeof(m.pass));
  strlcpy(m.baseTopic, server.arg("mq_base").c_str(), sizeof(m.baseTopic));
  strlcpy(m.clientId, server.arg("mq_cid").c_str(), sizeof(m.clientId));
  m.publishIntervalSec = max(5, (int)server.arg("mq_ival").toInt());
  m.haDiscovery = server.hasArg("mq_ha");
  strlcpy(m.haPrefix, server.arg("mq_hap").c_str(), sizeof(m.haPrefix));
  if (m.haPrefix[0] == '\0')
    strlcpy(m.haPrefix, "homeassistant", sizeof(m.haPrefix));
  config.save();
  sendRebootNotice("MQTT settings saved.", "mqtt");
}

static void handleSensors() {
  config.bme280.enabled = server.hasArg("bme_en");
  config.bme280.address = server.arg("bme_addr").toInt();
  config.bme280.tempUnit = server.arg("bme_tu")[0] == 'F' ? 'F' : 'C';
  config.bme280.pressureInHg = server.hasArg("bme_inhg");
  parseThr("bme_t", config.bme280.tTemp);
  parseThr("bme_h", config.bme280.tHum);
  parseThr("bme_p", config.bme280.tPres);

  config.ds18b20.enabled = server.hasArg("ds_en");
  config.ds18b20.tempUnit = server.arg("ds_tu")[0] == 'F' ? 'F' : 'C';
  parseThr("ds_t", config.ds18b20.tTemp);

  for (int i = 0; i < 2; i++) {
    String p = "a" + String(i + 1);
    AnalogConfig& a = config.analog[i];
    a.enabled = server.hasArg(p + "_en");
    strlcpy(a.label, server.arg(p + "_lbl").c_str(), sizeof(a.label));
    a.scale = (AnalogScale)server.arg(p + "_scale").toInt();
    a.rawMin = server.arg(p + "_rmin").toInt();
    a.rawMax = server.arg(p + "_rmax").toInt();
    parseThr(p + "_t", a.thr);
  }

  config.button.enabled = server.hasArg("btn_en");
  config.button.activeLow = server.hasArg("btn_al");

  config.save();
  sendRebootNotice("Sensor settings saved.", "sensors");
}

static void handleRelays() {
  for (int i = 0; i < 2; i++) {
    String p = "r" + String(i + 1);
    RelayConfig& r = config.relays[i];
    strlcpy(r.name, server.arg(p + "_name").c_str(), sizeof(r.name));
    r.mode = (RelayMode)server.arg(p + "_mode").toInt();
    r.activeLow = server.hasArg(p + "_al");
    r.manualState = server.hasArg(p + "_man");
    r.timerOnSec = server.arg(p + "_ton").toInt();
    r.timerOffSec = server.arg(p + "_toff").toInt();
    r.src = (MetricSource)server.arg(p + "_src").toInt();
    r.cmp = server.arg(p + "_cmp").toInt();
    r.level = server.arg(p + "_lvl").toFloat();
    r.hyst = server.arg(p + "_hyst").toFloat();
    r.allowMqtt = server.hasArg(p + "_mqtt");
    for (int k = 0; k < kSlotsPerRelay; k++) {
      String sp = p + "_s" + String(k);
      ScheduleSlot& sl = r.sched[k];
      sl.enabled = server.hasArg(sp + "_en");
      sl.onMin   = parseHHMM(server.arg(sp + "_on"));
      sl.offMin  = parseHHMM(server.arg(sp + "_off"));
      uint8_t days = 0;
      for (int d = 0; d < 7; d++) {
        if (server.hasArg(sp + "_d" + String(d))) days |= (1 << d);
      }
      sl.days = days;
    }
  }
  config.save();
  sendRebootNotice("Relay settings saved.", "relays");
}

static void handleTime() {
  TimeConfig& t = config.timecfg;
  t.enabled = server.hasArg("tm_en");
  strlcpy(t.server, server.arg("tm_srv").c_str(), sizeof(t.server));
  if (t.server[0] == '\0') strlcpy(t.server, "pool.ntp.org", sizeof(t.server));
  strlcpy(t.tz, server.arg("tm_tz").c_str(), sizeof(t.tz));
  if (t.tz[0] == '\0')
    strlcpy(t.tz, "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(t.tz));
  config.save();
  sendRebootNotice("Time settings saved.", "system");
}

static void handleFactoryReset() {
  String html = FPSTR(PAGE_HEAD);
  html += "<div class=\"card\"><h2>Factory reset</h2><p>Erasing settings and "
          "rebooting to defaults (ESP32-repeaterAP / 12345678).</p></div>";
  html += FPSTR(PAGE_FOOT);
  server.send(200, "text/html", html);
  delay(400);
  Config::factoryReset();  // clears NVS + restarts
}

static String fmt(float v, int dec = 2) {
  char b[16];
  dtostrf(v, 0, dec, b);
  return String(b);
}

static void handleReadings() {
  Readings r = Sensors::readAll();
  String j = "{";
  bool first = true;
  auto add = [&](const String& k, const String& v) {
    if (!first) j += ",";
    j += "\"" + k + "\":\"" + v + "\"";
    first = false;
  };

  add("Time", TimeKeeper::synced() ? TimeKeeper::fmtNow() : "not synced");
  if (r.bmeOk) {
    add("BME temp", fmt(r.temp) + " " + config.bme280.tempUnit);
    add("BME humidity", fmt(r.hum) + " %");
    add("BME pressure",
        fmt(r.pres) + (config.bme280.pressureInHg ? " inHg" : " hPa"));
  }
  if (r.dsOk) add("DS18B20", fmt(r.dsTemp) + " " + config.ds18b20.tempUnit);
  const char* aUnit[] = {"", " %", " V"};
  if (r.a1Ok)
    add(String(config.analog[0].label),
        fmt(r.a1, config.analog[0].scale == ANALOG_RAW ? 0 : 2) +
            aUnit[config.analog[0].scale]);
  if (r.a2Ok)
    add(String(config.analog[1].label),
        fmt(r.a2, config.analog[1].scale == ANALOG_RAW ? 0 : 2) +
            aUnit[config.analog[1].scale]);
  for (int i = 0; i < 2; i++) {
    add("Relay " + String(i + 1), Relays::getState(i) ? "ON" : "OFF");
  }
  j += "}";
  server.send(200, "application/json", j);
}

// Live raw ADC reading for analog calibration (Set as min/max buttons).
static void handleRaw() {
  int ch = server.arg("ch").toInt();
  if (ch < 0 || ch > 1) {
    server.send(400, "text/plain", "bad channel");
    return;
  }
  pinMode(PIN_ANALOG[ch], INPUT);
  server.send(200, "text/plain", String(analogRead(PIN_ANALOG[ch])));
}

// Captive-portal: bounce unknown hosts / OS probes to the setup page.
static void handleCaptive() {
  server.sendHeader("Location", "http://" + Net::apIp().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
void begin() {
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSaveWifi);
  server.on("/apconfig", HTTP_POST, handleApConfig);
  server.on("/mqtt", HTTP_POST, handleMqtt);
  server.on("/sensors", HTTP_POST, handleSensors);
  server.on("/relays", HTTP_POST, handleRelays);
  server.on("/time", HTTP_POST, handleTime);
  server.on("/factoryreset", HTTP_POST, handleFactoryReset);
  server.on("/readings", handleReadings);
  server.on("/raw", handleRaw);
  // OS connectivity-check probes -> portal.
  server.on("/generate_204", handleCaptive);
  server.on("/hotspot-detect.html", handleCaptive);
  server.on("/connecttest.txt", handleCaptive);
  server.onNotFound(handleCaptive);
  server.begin();
  Serial.printf("Setup portal ready at http://%s/\n",
                Net::apIp().toString().c_str());
}

void handle() { server.handleClient(); }

}  // namespace WebPortal
