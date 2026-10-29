#pragma once

#include <Arduino.h>

#include "touchlogic.h"

class TouchPad {
 public:
  using Event = TouchLogic::Event;

  static constexpr uint32_t kHoldMs = TouchLogic::kHoldMs;

  void begin(uint8_t pin);
  Event poll(uint32_t nowMs) { return logic_.feed(read(), nowMs); }
  bool pressed() const { return logic_.pressed(); }
  uint32_t heldFor(uint32_t nowMs) const { return logic_.heldFor(nowMs); }
  bool resetArmed(uint32_t nowMs) const { return logic_.resetArmed(nowMs); }
  float resetProgress(uint32_t nowMs) const { return logic_.resetProgress(nowMs); }
  uint32_t lastReading() const { return logic_.lastReading(); }
  uint32_t baseline() const { return logic_.baseline(); }

 private:
  uint32_t read() const;

  uint8_t pin_ = 0;
  TouchLogic logic_;
};
