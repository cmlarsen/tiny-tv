#include "air.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include <atomic>

#include "portal.h"

namespace air {
namespace {

using aqilogic::History;
using aqilogic::HourObs;

constexpr const char* kHost = "http://files.airnowtech.org";
constexpr float kMaxKm = 80;
constexpr int kDefaultHours = 24;
constexpr uint32_t kRefreshMs = 10UL * 60 * 1000;
constexpr uint32_t kRetryMs = 60UL * 1000;
constexpr uint32_t kOfflinePollMs = 1000;
constexpr uint32_t kStallMs = 15000;

char where[41];
char hoursText[4];
nmtv::PortalField fields[] = {
    {"where", "ZIP code or city (blank = guess)", "", where, sizeof where},
    {"hours", "Average over N hours (1-24)", "24", hoursText, sizeof hoursText},
};

SemaphoreHandle_t lock;
SemaphoreHandle_t serialLock;
TaskHandle_t handle;
Snapshot current{};
std::atomic<bool> dirty{true};
std::atomic<int> demoAqi{-1};

History history;
aqilogic::Place place{};
bool placeValid = false;
int32_t newestHour = -1;
HourObs newestObs{};
String newestModified;

template <typename... Args>
void log(const char* fmt, Args... args) {
  xSemaphoreTake(serialLock, portMAX_DELAY);
  Serial.printf(fmt, args...);
  xSemaphoreGive(serialLock);
}

enum class Fetch { Ok, NotModified, Missing, Error };

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
  if (where[0]) {
    char query[41];
    snprintf(query, sizeof query, "%s", where);
    char* comma = strchr(query, ',');
    if (comma) *comma = '\0';
    const String url = String("http://geocoding-api.open-meteo.com/v1/search?count=1&name=") + urlEncode(query);
    if (httpGet(url, body) && aqilogic::parseGeocode(body.c_str(), body.length(), place)) {
      log("[air] geocoded '%s' -> %s %.3f,%.3f\n", query, place.name, place.lat, place.lon);
      return true;
    }
    log("[air] geocode failed for '%s', falling back to IP location\n", query);
  }
  if (httpGet("http://ip-api.com/json/?fields=status,lat,lon,city", body) &&
      aqilogic::parseIpLocation(body.c_str(), body.length(), place)) {
    log("[air] IP location -> %s %.3f,%.3f\n", place.name, place.lat, place.lon);
    return true;
  }
  return false;
}

Fetch fetchHour(int32_t hour, const char* ifModified, String* lastModified, HourObs& out) {
  static char lineBuf[512];
  char path[64];
  aqilogic::obsPath(hour, path, sizeof path);
  const uint32_t t0 = millis();

  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(kStallMs);
  if (!http.begin(String(kHost) + path)) return Fetch::Error;
  const char* keys[] = {"Last-Modified"};
  http.collectHeaders(keys, 1);
  if (ifModified && *ifModified) http.addHeader("If-Modified-Since", ifModified);
  const int code = http.GET();
  if (code == 304 || code == 404 || code == 403) {
    http.end();
    log("[air] %s -> %d\n", path, code);
    return code == 304 ? Fetch::NotModified : Fetch::Missing;
  }
  if (code != 200) {
    http.end();
    log("[air] %s -> %d\n", path, code);
    return Fetch::Error;
  }
  if (lastModified) *lastModified = http.header("Last-Modified");

  aqilogic::Scanner scanner(place.lat, place.lon, kMaxKm);
  WiFiClient* stream = http.getStreamPtr();
  int remaining = http.getSize();
  uint8_t chunk[1024];
  size_t lineLen = 0;
  uint32_t lastData = millis();
  while (remaining != 0) {
    const size_t avail = stream->available();
    if (!avail) {
      if (!http.connected() || millis() - lastData > kStallMs) break;
      delay(2);
      continue;
    }
    const int n = stream->read(chunk, avail < sizeof chunk ? avail : sizeof chunk);
    if (n <= 0) continue;
    lastData = millis();
    if (remaining > 0) remaining -= n;
    for (int i = 0; i < n; i++) {
      if (chunk[i] == '\n') {
        lineBuf[lineLen] = '\0';
        scanner.line(lineBuf);
        lineLen = 0;
      } else if (lineLen < sizeof lineBuf - 1) {
        lineBuf[lineLen++] = static_cast<char>(chunk[i]);
      }
    }
  }
  if (lineLen) {
    lineBuf[lineLen] = '\0';
    scanner.line(lineBuf);
  }
  http.end();

  const HourObs& r = scanner.result();
  log("[air] %s -> 200 in %lums, pm25 %s %d, o3 %s %d%s\n", path, (unsigned long)(millis() - t0),
                r.pm25.have ? r.pm25.site : "-", r.pm25.have ? r.pm25.aqi : -1, r.o3.have ? r.o3.site : "-",
                r.o3.have ? r.o3.aqi : -1, remaining > 0 ? " TRUNCATED" : "");
  if (remaining > 0 || !scanner.headerOk()) return Fetch::Error;
  out = r;
  return Fetch::Ok;
}

int hours() { return aqilogic::parseHours(hoursText, kDefaultHours); }

void publish(bool failed) {
  const int n = hours();
  xSemaphoreTake(lock, portMAX_DELAY);
  current.located = placeValid;
  current.place = place;
  current.failed = failed;
  current.hours = n;
  if (newestHour >= 0) {
    current.have = newestObs.any();
    current.noMonitor = !newestObs.any();
    current.newestHour = newestHour;
    current.now = newestObs;
    current.avg = history.average(newestHour, n);
    for (int i = 0; i < n; i++) current.bars[i] = history.hourlyAqi(newestHour - (n - 1 - i));
    current.lastFetchMs = millis();
  } else {
    current.have = false;
    current.noMonitor = false;
  }
  xSemaphoreGive(lock);
}

bool ensureTime() {
  static bool started = false;
  if (!started) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    started = true;
  }
  return time(nullptr) > 1700000000;
}

bool cycle() {
  if (dirty.exchange(false)) {
    history.clear();
    placeValid = false;
    newestHour = -1;
    newestObs = HourObs{};
    newestModified = "";
    publish(false);
  }
  if (!placeValid) placeValid = locate();
  if (!placeValid) return false;

  const int32_t nowHour = static_cast<int32_t>(time(nullptr) / 3600);
  bool found = false;
  for (int32_t h = nowHour - 1; h >= nowHour - 4 && !found; h--) {
    HourObs obs{};
    String modified;
    const Fetch r = fetchHour(h, h == newestHour ? newestModified.c_str() : nullptr, &modified, obs);
    if (r == Fetch::Missing) continue;
    if (r == Fetch::Error) return false;
    found = true;
    if (r == Fetch::Ok && h >= newestHour) {
      newestHour = h;
      newestObs = obs;
      newestModified = modified;
      history.put(h, obs);
    }
  }
  if (!found) return false;
  publish(false);

  int32_t missing[History::kMax];
  const int n = history.missing(newestHour, hours(), missing, History::kMax);
  for (int i = 0; i < n && !dirty; i++) {
    HourObs obs{};
    const Fetch r = fetchHour(missing[i], nullptr, nullptr, obs);
    if (r == Fetch::Ok) {
      history.put(missing[i], obs);
      publish(false);
    }
  }
  return true;
}

void task(void*) {
  for (;;) {
    uint32_t waitMs = kOfflinePollMs;
    if (nmtv::portal::online() && ensureTime()) {
      const bool ok = cycle();
      if (!ok) publish(true);
      waitMs = ok ? kRefreshMs : kRetryMs;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
  }
}

void applyDemo(Snapshot& s, int aqi) {
  s.located = true;
  s.have = true;
  s.failed = false;
  s.noMonitor = false;
  s.hours = kDefaultHours;
  s.newestHour = static_cast<int32_t>(time(nullptr) / 3600) - 1;
  snprintf(s.place.name, sizeof s.place.name, "Kalamazoo");
  s.now = HourObs{};
  s.now.pm25.have = true;
  s.now.pm25.aqi = aqi;
  snprintf(s.now.pm25.site, sizeof s.now.pm25.site, "Kalamazoo");
  s.now.o3.have = true;
  s.now.o3.aqi = aqi / 4;
  int sum = 0;
  for (int i = 0; i < s.hours; i++) {
    const int v = aqi / 2 + (aqi * i) / (2 * (s.hours - 1)) + ((i * 37) % 11) - 5;
    s.bars[i] = i == 6 || i == 7 ? -1 : (v < 0 ? 0 : v);
    if (s.bars[i] >= 0) sum += s.bars[i];
  }
  s.avg = aqilogic::Average{true, sum / (s.hours - 2), "PM2.5", s.hours - 2};
}

}  // namespace

void begin() {
  lock = xSemaphoreCreateMutex();
  serialLock = xSemaphoreCreateMutex();
  nmtv::portal::begin({"Air Quality", "Air-Quality", "air-quality", fields, sizeof fields / sizeof fields[0]});
  xTaskCreatePinnedToCore(task, "air", 10240, nullptr, 1, &handle, 0);
}

void loop() {
  nmtv::portal::loop();
  if (nmtv::portal::takeSettingsChanged()) {
    dirty = true;
    refreshNow();
  }
}

bool online() { return nmtv::portal::online(); }

void refreshNow() { xTaskNotifyGive(handle); }

Snapshot snapshot() {
  xSemaphoreTake(lock, portMAX_DELAY);
  Snapshot s = current;
  xSemaphoreGive(lock);
  if (demoAqi >= 0) applyDemo(s, demoAqi);
  return s;
}

void demo(int aqi) { demoAqi = aqi; }

void lockSerial() { xSemaphoreTake(serialLock, portMAX_DELAY); }

void unlockSerial() { xSemaphoreGive(serialLock); }

}  // namespace air
