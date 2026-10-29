#pragma once

#include <stddef.h>
#include <stdint.h>

namespace piglogic {

enum class Mood { Sunny, Cloudy, Hot, Chilly, Cold, Rain, Storm, Snow, Windy, Fog };
constexpr int kMoodCount = 10;

struct DayWeather {
  int code;
  float maxF;
  float minF;
  int precipPct;
  float windMph;
  int cloudPct;
};

struct Forecast {
  char date[11];
  int utcOffsetSec;
  float nowF;
  int nowCode;
  bool isDay;
  DayWeather days[2];
};

struct Place {
  float lat;
  float lon;
  char name[40];
};

Mood classify(const DayWeather& d);
const char* moodName(Mood m);

void formatMessage(Mood m, bool tomorrow, const char* name, uint32_t seed, char* out, size_t outLen);
uint32_t seedFromDate(const char* date);

bool parseForecast(const char* json, size_t len, Forecast& out);
bool parseGeocode(const char* json, size_t len, Place& out);
bool parseIpLocation(const char* json, size_t len, Place& out);

int toDisplayTemp(float f, bool celsius);

}  // namespace piglogic
