#include "net.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include <atomic>

#include "portal.h"

namespace net {
namespace {

Settings current{};
piglogic::Place place{};
bool placeValid = false;
std::atomic<bool> placeDirty{false};

nmtv::PortalField fields[] = {
    {"name", "Kid's name", "Zed", current.name, sizeof current.name},
    {"city", "City or ZIP code (blank = guess)", "", current.city, sizeof current.city},
    {"units", "Units: F or C", "F", current.units, sizeof current.units},
};

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
    snprintf(query, sizeof query, "%s", current.city);
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

}  // namespace

void begin() {
  nmtv::portal::begin({"Piggy Weather", "Piggy-Weather", "piggy", fields, sizeof fields / sizeof fields[0]});
}

void loop() { nmtv::portal::loop(); }

bool online() { return nmtv::portal::online(); }

const Settings& settings() { return current; }

bool takeSettingsChanged() {
  if (!nmtv::portal::takeSettingsChanged()) return false;
  placeDirty = true;
  return true;
}

bool fetchForecast(piglogic::Forecast& out) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (placeDirty.exchange(false)) placeValid = false;
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

String ip() { return nmtv::portal::ip(); }

void resetAll() { nmtv::portal::resetAll(); }

}  // namespace net
