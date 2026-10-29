#pragma once

#include <Arduino.h>

#include "display.h"
#include "fleet.h"

enum class View { Overview, Detail, Info, Setup, Connecting };

struct SceneState {
  View view = View::Connecting;
  int detail = 0;
  const Machine* machines = nullptr;
  int count = 0;
  char clock[8] = "";
  char info[5][40] = {};
  float resetProgress = -1;
  const char* qr = nullptr;
};

class Scene {
 public:
  explicit Scene(LGFX& lcd) : band_(&lcd) {}
  bool begin();
  void render(const SceneState& s, uint32_t nowMs, Print* dump = nullptr);

 private:
  LGFX_Sprite band_;
  float shownCpu_[kMaxMachines] = {};
  float shownMem_[kMaxMachines] = {};
};
