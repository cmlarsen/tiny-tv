#include "aqilogic.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace aqilogic {

namespace {

struct Breakpoint {
  float lo, hi;
  int aqiLo, aqiHi;
};

constexpr Breakpoint kPm25[] = {
    {0.0f, 9.0f, 0, 50},       {9.1f, 35.4f, 51, 100},    {35.5f, 55.4f, 101, 150},
    {55.5f, 125.4f, 151, 200}, {125.5f, 225.4f, 201, 300}, {225.5f, 325.4f, 301, 500},
};

constexpr Breakpoint kO3[] = {
    {0, 54, 0, 50},       {55, 70, 51, 100},    {71, 85, 101, 150},   {86, 105, 151, 200},
    {106, 200, 201, 300}, {201, 404, 300, 300}, {405, 504, 301, 400}, {505, 604, 401, 500},
};

template <size_t N>
int scale(const Breakpoint (&table)[N], float c) {
  if (c < 0) c = 0;
  for (const Breakpoint& b : table) {
    if (c <= b.hi) {
      const float t = c < b.lo ? b.lo : c;
      return static_cast<int>(lroundf((b.aqiHi - b.aqiLo) * (t - b.lo) / (b.hi - b.lo) + b.aqiLo));
    }
  }
  return table[N - 1].aqiHi;
}

constexpr int kMaxFields = 40;

int splitCsv(char* line, char** fields) {
  int n = 0;
  char* p = line;
  while (*p && n < kMaxFields) {
    if (*p == '"') {
      p++;
      fields[n++] = p;
      while (*p && *p != '"') p++;
      if (*p) *p++ = '\0';
      while (*p && *p != ',') p++;
    } else {
      fields[n++] = p;
      while (*p && *p != ',') p++;
    }
    if (*p == ',') *p++ = '\0';
  }
  return n;
}

void trimEol(char* s) {
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
}

const char* field(char** f, int n, int i) { return i >= 0 && i < n ? f[i] : ""; }

void copyStr(char* dst, size_t cap, const char* src) {
  snprintf(dst, cap, "%s", src);
}

}  // namespace

Level level(int aqi) {
  if (aqi <= 50) return Level::Good;
  if (aqi <= 100) return Level::Moderate;
  if (aqi <= 150) return Level::Sensitive;
  if (aqi <= 200) return Level::Unhealthy;
  if (aqi <= 300) return Level::VeryUnhealthy;
  return Level::Hazardous;
}

const char* levelLine1(Level l) {
  switch (l) {
    case Level::Good: return "Good";
    case Level::Moderate: return "Moderate";
    case Level::Sensitive: return "Unhealthy for";
    case Level::Unhealthy: return "Unhealthy";
    case Level::VeryUnhealthy: return "Very unhealthy";
    case Level::Hazardous: return "Hazardous";
  }
  return "";
}

const char* levelLine2(Level l) { return l == Level::Sensitive ? "sensitive groups" : ""; }

int pm25Aqi(float ugm3) { return scale(kPm25, floorf(ugm3 * 10 + 0.0001f) / 10); }

int o3Aqi(float ppb) { return scale(kO3, floorf(ppb + 0.0001f)); }

float distanceKm(float lat1, float lon1, float lat2, float lon2) {
  constexpr float kRad = 3.14159265f / 180;
  const float dLat = (lat2 - lat1) * kRad;
  const float dLon = (lon2 - lon1) * kRad;
  const float a = sinf(dLat / 2) * sinf(dLat / 2) +
                  cosf(lat1 * kRad) * cosf(lat2 * kRad) * sinf(dLon / 2) * sinf(dLon / 2);
  return 6371.0f * 2 * atan2f(sqrtf(a), sqrtf(1 - a));
}

bool parseGeocode(const char* json, size_t len, Place& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  JsonObjectConst r = doc["results"][0];
  if (r.isNull() || !r["latitude"].is<float>()) return false;
  out.lat = r["latitude"];
  out.lon = r["longitude"];
  copyStr(out.name, sizeof out.name, r["name"] | "");
  return true;
}

bool parseIpLocation(const char* json, size_t len, Place& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  if (!doc["lat"].is<float>()) return false;
  out.lat = doc["lat"];
  out.lon = doc["lon"];
  copyStr(out.name, sizeof out.name, doc["city"] | "");
  return true;
}

int parseHours(const char* text, int fallback) {
  const int h = atoi(text);
  if (h < 1) return fallback;
  return h > History::kMax ? History::kMax : h;
}

void obsPath(int32_t hour, char* out, size_t len) {
  const time_t t = static_cast<time_t>(hour) * 3600;
  struct tm tm;
  gmtime_r(&t, &tm);
  snprintf(out, len, "/airnow/%04d/%04d%02d%02d/HourlyAQObs_%04d%02d%02d%02d.dat", tm.tm_year + 1900,
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour);
}

void ageLabel(int64_t nowSec, int32_t hour, char* out, size_t len) {
  const int64_t mins = (nowSec - (static_cast<int64_t>(hour) + 1) * 3600) / 60;
  if (mins < 1) snprintf(out, len, "just now");
  else if (mins < 60) snprintf(out, len, "%d min ago", static_cast<int>(mins));
  else snprintf(out, len, "%d h ago", static_cast<int>(mins / 60));
}

int HourObs::aqi() const {
  const int a = pm25.have ? pm25.aqi : -1;
  const int b = o3.have ? o3.aqi : -1;
  return a > b ? a : b;
}

const char* HourObs::dominant() const {
  if (!any()) return "";
  return pm25.have && (!o3.have || pm25.aqi >= o3.aqi) ? "PM2.5" : "Ozone";
}

const char* HourObs::site() const {
  if (pm25.have) return pm25.site;
  return o3.have ? o3.site : "";
}

Scanner::Scanner(float lat, float lon, float maxKm) : lat_(lat), lon_(lon), maxKm_(maxKm) {}

void Scanner::line(char* text) {
  trimEol(text);
  char* f[kMaxFields];
  const int n = splitCsv(text, f);
  if (!sawHeader_) {
    sawHeader_ = true;
    for (int i = 0; i < n; i++) {
      if (!strcmp(f[i], "SiteName")) cols_.site = i;
      else if (!strcmp(f[i], "Status")) cols_.status = i;
      else if (!strcmp(f[i], "Latitude")) cols_.lat = i;
      else if (!strcmp(f[i], "Longitude")) cols_.lon = i;
      else if (!strcmp(f[i], "PM25_AQI")) cols_.pm25Aqi = i;
      else if (!strcmp(f[i], "PM25")) cols_.pm25 = i;
      else if (!strcmp(f[i], "OZONE_AQI")) cols_.o3Aqi = i;
      else if (!strcmp(f[i], "OZONE")) cols_.o3 = i;
    }
    if (cols_.lon < 0 || cols_.pm25Aqi < 0 || cols_.o3Aqi < 0) cols_.lat = -1;
    return;
  }
  if (!headerOk() || n <= cols_.lon) return;
  if (cols_.status >= 0 && strcmp(field(f, n, cols_.status), "Active") != 0) return;
  const char* latStr = field(f, n, cols_.lat);
  const char* lonStr = field(f, n, cols_.lon);
  if (!*latStr || !*lonStr) return;
  const float km = distanceKm(lat_, lon_, atof(latStr), atof(lonStr));
  if (km > maxKm_) return;
  const char* site = field(f, n, cols_.site);
  consider(obs_.pm25, site, km, field(f, n, cols_.pm25Aqi), field(f, n, cols_.pm25));
  consider(obs_.o3, site, km, field(f, n, cols_.o3Aqi), field(f, n, cols_.o3));
}

void Scanner::consider(Pick& p, const char* site, float km, const char* aqi, const char* conc) {
  if (!*aqi || !*conc) return;
  if (p.have && p.km <= km) return;
  p.have = true;
  p.km = km;
  p.aqi = atoi(aqi);
  p.conc = static_cast<float>(atof(conc));
  copyStr(p.site, sizeof p.site, site);
}

void History::put(int32_t hour, const HourObs& obs) {
  Slot& s = slots_[((hour % kMax) + kMax) % kMax];
  if (s.hour > hour) return;
  s.hour = hour;
  s.pm = obs.pm25.have;
  s.pm25 = obs.pm25.conc;
  s.oz = obs.o3.have;
  s.o3 = obs.o3.conc;
}

const History::Slot* History::find(int32_t hour) const {
  const Slot& s = slots_[((hour % kMax) + kMax) % kMax];
  return s.hour == hour ? &s : nullptr;
}

bool History::has(int32_t hour) const { return find(hour) != nullptr; }

int History::hourlyAqi(int32_t hour) const {
  const Slot* s = find(hour);
  if (!s || (!s->pm && !s->oz)) return -1;
  const int a = s->pm ? pm25Aqi(s->pm25) : -1;
  const int b = s->oz ? o3Aqi(s->o3) : -1;
  return a > b ? a : b;
}

Average History::average(int32_t newest, int hours) const {
  if (hours > kMax) hours = kMax;
  float pmSum = 0, ozSum = 0;
  int pmN = 0, ozN = 0, n = 0;
  for (int i = 0; i < hours; i++) {
    const Slot* s = find(newest - i);
    if (!s || (!s->pm && !s->oz)) continue;
    n++;
    if (s->pm) {
      pmSum += s->pm25;
      pmN++;
    }
    if (s->oz) {
      ozSum += s->o3;
      ozN++;
    }
  }
  Average a{n > 0, -1, "", n};
  if (!a.have) return a;
  const int pm = pmN ? pm25Aqi(pmSum / pmN) : -1;
  const int oz = ozN ? o3Aqi(ozSum / ozN) : -1;
  a.aqi = pm >= oz ? pm : oz;
  a.dominant = pm >= oz ? "PM2.5" : "Ozone";
  return a;
}

int History::missing(int32_t newest, int hours, int32_t* out, int max) const {
  if (hours > kMax) hours = kMax;
  int n = 0;
  for (int i = 0; i < hours && n < max; i++)
    if (!has(newest - i)) out[n++] = newest - i;
  return n;
}

void History::clear() {
  for (Slot& s : slots_) s = Slot{};
}

}  // namespace aqilogic
