#pragma once

#include <Arduino.h>

#include "display.h"
#include "piglogic.h"

enum class View { Today, Tomorrow, Night, Info, Setup, Loading, Offline };

struct SceneState {
  View view = View::Loading;
  piglogic::Mood mood = piglogic::Mood::Sunny;
  char message[96] = "";
  bool haveWeather = false;
  int bigTemp = 0;
  const char* bigLabel = "";
  int hi = 0;
  int lo = 0;
  int precip = 0;
  char clock[8] = "";
  uint32_t bounceAtMs = 0;
  float resetProgress = -1;
  bool resetArmed = false;
  char info[5][40] = {};
  const char* qr = nullptr;
};

class Scene {
 public:
  explicit Scene(LGFX& lcd) : lcd_(lcd), band_(&lcd) {}
  bool begin();
  void render(const SceneState& s, uint32_t nowMs, Print* dump = nullptr);

 private:
  LGFX& lcd_;
  LGFX_Sprite band_;
};
