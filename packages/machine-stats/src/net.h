#pragma once

#include <Arduino.h>

struct Settings {
  char machines[160];
  char tz[48];
};

namespace net {

void begin();
const Settings& settings();

}  // namespace net
