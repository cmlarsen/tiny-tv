#pragma once

#include <Arduino.h>

#include "piglogic.h"

struct Settings {
  char name[21];
  char city[41];
  char units[2];

  bool celsius() const { return units[0] == 'C' || units[0] == 'c'; }
};

namespace net {

void begin();
void loop();
bool online();

const Settings& settings();
bool takeSettingsChanged();

bool fetchForecast(piglogic::Forecast& out);
const char* placeName();
String ip();

void resetAll();

}  // namespace net
