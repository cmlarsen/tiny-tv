#pragma once

#include <Arduino.h>

#include "piglogic.h"

namespace weather {

struct Snapshot {
  bool have;
  bool failed;
  uint32_t lastOkMs;
  piglogic::Forecast forecast;
  char place[40];
};

void begin();
void refreshNow();
Snapshot snapshot();

}  // namespace weather
