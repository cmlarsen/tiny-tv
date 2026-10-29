#pragma once

#include <Arduino.h>

constexpr const char* kApName = "Stats-HUD";
constexpr const char* kApQr = "WIFI:T:nopass;S:Stats-HUD;;";

struct Settings {
  char machines[160];
  char tz[48];
};

namespace net {

void begin();
void loop();
bool online();

const Settings& settings();
bool takeSettingsChanged();
String ip();

void resetAll();

}  // namespace net
