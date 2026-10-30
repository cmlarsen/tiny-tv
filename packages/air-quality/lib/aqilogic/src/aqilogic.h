#pragma once

#include <stddef.h>
#include <stdint.h>

namespace aqilogic {

enum class Level { Good, Moderate, Sensitive, Unhealthy, VeryUnhealthy, Hazardous };

Level level(int aqi);
const char* levelLine1(Level l);
const char* levelLine2(Level l);

int pm25Aqi(float ugm3);
int o3Aqi(float ppb);

float distanceKm(float lat1, float lon1, float lat2, float lon2);

struct Place {
  float lat;
  float lon;
  char name[40];
};

bool parseGeocode(const char* json, size_t len, Place& out);
bool parseIpLocation(const char* json, size_t len, Place& out);
int parseHours(const char* text, int fallback);

void obsPath(int32_t hour, char* out, size_t len);
void ageLabel(int64_t nowSec, int32_t hour, char* out, size_t len);

struct Pick {
  bool have;
  char site[32];
  float km;
  int aqi;
  float conc;
};

struct HourObs {
  Pick pm25;
  Pick o3;

  bool any() const { return pm25.have || o3.have; }
  int aqi() const;
  const char* dominant() const;
  const char* site() const;
};

class Scanner {
 public:
  Scanner(float lat, float lon, float maxKm);
  void line(char* text);
  bool headerOk() const { return cols_.lat >= 0; }
  const HourObs& result() const { return obs_; }

 private:
  struct Columns {
    int site = -1, status = -1, lat = -1, lon = -1;
    int pm25Aqi = -1, pm25 = -1, o3Aqi = -1, o3 = -1;
  };
  void consider(Pick& p, const char* site, float km, const char* aqi, const char* conc);

  float lat_, lon_, maxKm_;
  bool sawHeader_ = false;
  Columns cols_;
  HourObs obs_{};
};

struct Average {
  bool have;
  int aqi;
  const char* dominant;
  int hours;
};

class History {
 public:
  static constexpr int kMax = 24;

  void put(int32_t hour, const HourObs& obs);
  bool has(int32_t hour) const;
  int hourlyAqi(int32_t hour) const;
  Average average(int32_t newest, int hours) const;
  int missing(int32_t newest, int hours, int32_t* out, int max) const;
  void clear();

 private:
  struct Slot {
    int32_t hour = -1;
    bool pm = false;
    float pm25 = 0;
    bool oz = false;
    float o3 = 0;
  };
  const Slot* find(int32_t hour) const;
  Slot slots_[kMax];
};

}  // namespace aqilogic
