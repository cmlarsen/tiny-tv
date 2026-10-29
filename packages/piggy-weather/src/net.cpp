#include "net.h"

#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace net {
namespace {

WiFiManager wm;
Preferences prefs;
Settings current{};
bool changed = false;
piglogic::Place place{};
bool placeValid = false;
bool isOnline = false;

WiFiManagerParameter* nameParam = nullptr;
WiFiManagerParameter* cityParam = nullptr;
WiFiManagerParameter* unitsParam = nullptr;

void copyField(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

void loadSettings() {
  prefs.begin("piggy", true);
  copyField(current.name, sizeof current.name, prefs.getString("name", "Zed").c_str());
  copyField(current.city, sizeof current.city, prefs.getString("city", "").c_str());
  current.celsius = prefs.getBool("celsius", false);
  prefs.end();
}

void saveParams() {
  copyField(current.name, sizeof current.name, nameParam->getValue());
  copyField(current.city, sizeof current.city, cityParam->getValue());
  const char u = unitsParam->getValue()[0];
  current.celsius = (u == 'C' || u == 'c');
  prefs.begin("piggy", false);
  prefs.putString("name", current.name);
  prefs.putString("city", current.city);
  prefs.putBool("celsius", current.celsius);
  prefs.end();
  placeValid = false;
  changed = true;
}

String urlEncode(const char* s) {
  String out;
  for (const char* p = s; *p; p++) {
    const char c = *p;
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.' || c == '_') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof buf, "%%%02X", static_cast<uint8_t>(c));
      out += buf;
    }
  }
  return out;
}

bool httpGet(const String& url, String& body) {
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(url)) return false;
  const int code = http.GET();
  if (code == 200) body = http.getString();
  http.end();
  return code == 200;
}

bool locate() {
  String body;
  if (current.city[0]) {
    char query[41];
    copyField(query, sizeof query, current.city);
    char* comma = strchr(query, ',');
    if (comma) *comma = '\0';
    const String url = String("http://geocoding-api.open-meteo.com/v1/search?count=1&name=") + urlEncode(query);
    if (httpGet(url, body) && piglogic::parseGeocode(body.c_str(), body.length(), place)) {
      Serial.printf("[net] geocoded '%s' -> %s %.3f,%.3f\n", query, place.name, place.lat, place.lon);
      return true;
    }
    Serial.printf("[net] geocode failed for '%s', falling back to IP location\n", query);
  }
  if (httpGet("http://ip-api.com/json/?fields=status,lat,lon,city", body) &&
      piglogic::parseIpLocation(body.c_str(), body.length(), place)) {
    Serial.printf("[net] IP location -> %s %.3f,%.3f\n", place.name, place.lat, place.lon);
    return true;
  }
  return false;
}

void onConnected() {
  isOnline = true;
  changed = false;
  MDNS.begin("piggy");
  std::vector<const char*> menu = {"param", "wifi", "info", "restart"};
  wm.setMenu(menu);
  wm.startWebPortal();
  Serial.printf("[net] connected, IP %s\n", WiFi.localIP().toString().c_str());
}

}  // namespace

void begin() {
  loadSettings();
  wm.setDebugOutput(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("piggy-weather");

  nameParam = new WiFiManagerParameter("name", "Kid's name", current.name, 20);
  cityParam = new WiFiManagerParameter("city", "City or ZIP code (blank = guess)", current.city, 40);
  unitsParam = new WiFiManagerParameter("units", "Units: F or C", current.celsius ? "C" : "F", 1);
  wm.addParameter(nameParam);
  wm.addParameter(cityParam);
  wm.addParameter(unitsParam);
  wm.setSaveParamsCallback(saveParams);
  wm.setTitle("Piggy Weather");
  wm.setConnectTimeout(20);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(wm.getWiFiIsSaved() ? 300 : 0);

  if (wm.autoConnect(kApName)) onConnected();
}

void loop() {
  const bool connectedNow = wm.process();
  if (isOnline) return;
  if (connectedNow || WiFi.status() == WL_CONNECTED) {
    onConnected();
  } else if (!wm.getConfigPortalActive()) {
    ESP.restart();
  }
}

bool online() { return isOnline; }

const Settings& settings() { return current; }

bool takeSettingsChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

bool fetchForecast(piglogic::Forecast& out) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!placeValid) placeValid = locate();
  if (!placeValid) return false;
  char url[400];
  snprintf(url, sizeof url,
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day"
           "&hourly=weather_code,cloud_cover,precipitation_probability"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,wind_speed_10m_max"
           "&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto&forecast_days=2",
           place.lat, place.lon);
  String body;
  if (!httpGet(url, body)) return false;
  return piglogic::parseForecast(body.c_str(), body.length(), out);
}

const char* placeName() { return placeValid ? place.name : ""; }

String ip() { return WiFi.localIP().toString(); }

void resetAll() {
  wm.resetSettings();
  prefs.begin("piggy", false);
  prefs.clear();
  prefs.end();
  delay(200);
  ESP.restart();
}

}  // namespace net
