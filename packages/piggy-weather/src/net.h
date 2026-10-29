#pragma once

#include <Arduino.h>

#include "piglogic.h"

constexpr const char* kApName = "Piggy-Weather";
constexpr const char* kApQr = "WIFI:T:nopass;S:Piggy-Weather;;";

struct Settings {
  char name[21];
  char city[41];
  bool celsius;
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
