#pragma once

#include <Arduino.h>

#include "statslogic.h"

constexpr int kMaxMachines = 6;
constexpr int kHistory = 60;
constexpr uint32_t kPollMs = 3000;

struct Machine {
  statslogic::Endpoint ep;
  statslogic::Stats stats;
  bool everSeen;
  bool online;
  uint8_t failures;
  uint32_t lastOkMs;
  statslogic::History<kHistory> cpu;
  statslogic::History<kHistory> mem;
};

namespace fleet {

void begin(const char* endpointList);
void reconfigure(const char* endpointList);
int snapshot(Machine* out, int max);

}  // namespace fleet
