#pragma once

#include <Arduino.h>

#include "aqilogic.h"

namespace air {

struct Snapshot {
  bool located;
  bool have;
  bool failed;
  bool noMonitor;
  int32_t newestHour;
  aqilogic::HourObs now;
  aqilogic::Average avg;
  int hours;
  int bars[aqilogic::History::kMax];
  aqilogic::Place place;
  uint32_t lastFetchMs;
};

void begin();
void loop();
bool online();
void refreshNow();
Snapshot snapshot();

void demo(int aqi);

void lockSerial();
void unlockSerial();

}  // namespace air
