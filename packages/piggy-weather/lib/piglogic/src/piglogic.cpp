#include "piglogic.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include <algorithm>

namespace piglogic {

namespace {

constexpr int kDayStartHour = 7;
constexpr int kDayEndHour = 20;
constexpr int kRainCodeMinPct = 20;
constexpr int kRainLikelyPct = 50;

const char* const kSunny[] = {
    "Sunny and nice {d}! Go play outside!",
    "{D} is perfect for zoomies, {n}!",
    "Blue skies {d}! Oink oink!",
};
const char* const kCloudy[] = {
    "Cloudy {d}, but still great for playing!",
    "Gray skies {d}. Cozy snack time!",
    "Clouds {d}, {n}. Still a happy pig day!",
};
const char* const kHot[] = {
    "{D} will be toasty! Drink lots of water.",
    "Hot hot hot {d}! Find a mud puddle!",
    "Sizzling {d}! Sunscreen on that snout!",
};
const char* const kChilly[] = {
    "A little chilly {d}. Grab a jacket!",
    "Jacket weather {d}, {n}!",
    "Cool {d}! A hoodie is oink-cellent.",
};
const char* const kCold[] = {
    "Brrr! Super cold {d}. Wear a hat!",
    "Frosty {d}! Bundle up, {n}!",
    "Cold enough to freeze a snout! Coat on!",
};
const char* const kRain[] = {
    "{D} might rain. Bring a raincoat!",
    "Puddle day {d}! Rain boots on, {n}!",
    "Rainy {d}! Don't forget an umbrella.",
};
const char* const kStorm[] = {
    "Thunderstorms {d}! Stay cozy inside.",
    "Stormy {d}. Rumble rumble! Stay safe.",
};
const char* const kSnow[] = {
    "Snow {d}! Build a snow pig, {n}!",
    "Snowflakes {d}! Catch one on your snout!",
    "Snowy {d}. Mittens and boots, {n}!",
};
const char* const kWindy[] = {
    "Windy {d}! Hold on to your ears!",
    "Super windy {d}. Great kite weather!",
};
const char* const kFog[] = {
    "Foggy {d}. Like living in a cloud!",
    "Spooky foggy {d}! Stay close, {n}.",
};

struct Bank {
  const char* const* items;
  size_t count;
};

template <size_t N>
constexpr Bank bank(const char* const (&a)[N]) {
  return Bank{a, N};
}

Bank bankFor(Mood m) {
  switch (m) {
    case Mood::Sunny: return bank(kSunny);
    case Mood::Cloudy: return bank(kCloudy);
    case Mood::Hot: return bank(kHot);
    case Mood::Chilly: return bank(kChilly);
    case Mood::Cold: return bank(kCold);
    case Mood::Rain: return bank(kRain);
    case Mood::Storm: return bank(kStorm);
    case Mood::Snow: return bank(kSnow);
    case Mood::Windy: return bank(kWindy);
    case Mood::Fog: return bank(kFog);
  }
  return bank(kSunny);
}

struct Writer {
  char* out;
  size_t cap;
  size_t len;

  void put(char c) {
    if (len + 1 < cap) out[len++] = c;
  }
  void put(const char* s) {
    while (*s) put(*s++);
  }
};

int cloudFromCode(int code) {
  if (code == 3 || code == 45 || code == 48) return 100;
  if (code == 2) return 50;
  return 0;
}

void applyHourlyWindow(JsonObjectConst hourly, int first, int last, DayWeather& d) {
  int worst = 0, cloudSum = 0, rainMax = 0, count = 0;
  for (int h = first; h <= last; h++) {
    worst = std::max(worst, hourly["weather_code"][h] | 0);
    cloudSum += hourly["cloud_cover"][h] | 0;
    rainMax = std::max(rainMax, hourly["precipitation_probability"][h] | 0);
    count++;
  }
  d.code = worst;
  d.cloudPct = cloudSum / count;
  d.precipPct = rainMax;
}

void copyStr(char* dst, size_t cap, const char* src) {
  if (cap == 0) return;
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

}  // namespace

Mood classify(const DayWeather& d) {
  const int c = d.code;
  if (c >= 95 && c <= 99) return Mood::Storm;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return Mood::Snow;
  const bool rainCode = (c >= 51 && c <= 67) || (c >= 80 && c <= 82);
  if ((rainCode && d.precipPct >= kRainCodeMinPct) || d.precipPct >= kRainLikelyPct) return Mood::Rain;
  if (d.maxF >= 88) return Mood::Hot;
  if (d.maxF <= 40) return Mood::Cold;
  if (d.windMph >= 25) return Mood::Windy;
  if (d.maxF <= 58) return Mood::Chilly;
  if ((c == 45 || c == 48) && d.cloudPct >= 50) return Mood::Fog;
  if (d.cloudPct >= 60) return Mood::Cloudy;
  return Mood::Sunny;
}

const char* moodName(Mood m) {
  switch (m) {
    case Mood::Sunny: return "sunny";
    case Mood::Cloudy: return "cloudy";
    case Mood::Hot: return "hot";
    case Mood::Chilly: return "chilly";
    case Mood::Cold: return "cold";
    case Mood::Rain: return "rain";
    case Mood::Storm: return "storm";
    case Mood::Snow: return "snow";
    case Mood::Windy: return "windy";
    case Mood::Fog: return "fog";
  }
  return "?";
}

void formatMessage(Mood m, bool tomorrow, const char* name, uint32_t seed, char* out, size_t outLen) {
  if (outLen == 0) return;
  const Bank b = bankFor(m);
  const char* tpl = b.items[seed % b.count];
  const char* who = (name && *name) ? name : "friend";
  Writer w{out, outLen, 0};
  for (const char* p = tpl; *p; p++) {
    if (p[0] == '{' && p[1] && p[2] == '}') {
      switch (p[1]) {
        case 'D': w.put(tomorrow ? "Tomorrow" : "Today"); break;
        case 'd': w.put(tomorrow ? "tomorrow" : "today"); break;
        case 'n': w.put(who); break;
      }
      p += 2;
      continue;
    }
    w.put(*p);
  }
  out[w.len] = '\0';
}

uint32_t seedFromDate(const char* date) {
  uint32_t h = 2166136261u;
  for (const char* p = date; p && *p; p++) {
    h ^= static_cast<uint8_t>(*p);
    h *= 16777619u;
  }
  return h;
}

bool parseForecast(const char* json, size_t len, Forecast& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  JsonObjectConst cur = doc["current"];
  JsonObjectConst daily = doc["daily"];
  if (cur.isNull() || daily.isNull()) return false;
  if (!cur["temperature_2m"].is<float>()) return false;
  JsonArrayConst maxes = daily["temperature_2m_max"];
  if (daily["time"].size() < 2 || maxes.size() < 2) return false;

  Forecast f{};
  copyStr(f.date, sizeof f.date, daily["time"][0] | "");
  f.utcOffsetSec = doc["utc_offset_seconds"] | 0;
  f.nowF = cur["temperature_2m"];
  f.nowCode = cur["weather_code"] | 0;
  f.isDay = (cur["is_day"] | 1) != 0;
  for (int i = 0; i < 2; i++) {
    DayWeather& d = f.days[i];
    d.code = daily["weather_code"][i] | 0;
    d.maxF = daily["temperature_2m_max"][i] | 0.0f;
    d.minF = daily["temperature_2m_min"][i] | 0.0f;
    d.precipPct = daily["precipitation_probability_max"][i] | 0;
    d.windMph = daily["wind_speed_10m_max"][i] | 0.0f;
    d.cloudPct = cloudFromCode(d.code);
  }

  JsonObjectConst hourly = doc["hourly"];
  if (hourly["weather_code"].size() >= 48 && hourly["cloud_cover"].size() >= 48) {
    const char* now = cur["time"] | "";
    const int nowHour = strlen(now) >= 13 ? (now[11] - '0') * 10 + (now[12] - '0') : 0;
    int first = std::max(nowHour, kDayStartHour);
    int last = kDayEndHour;
    if (first > last) {
      first = nowHour;
      last = 23;
    }
    applyHourlyWindow(hourly, first, last, f.days[0]);
    applyHourlyWindow(hourly, 24 + kDayStartHour, 24 + kDayEndHour, f.days[1]);
  }
  out = f;
  return true;
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

int toDisplayTemp(float f, bool celsius) {
  const float v = celsius ? (f - 32.0f) * 5.0f / 9.0f : f;
  return static_cast<int>(lroundf(v));
}

}  // namespace piglogic
