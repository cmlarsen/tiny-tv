#pragma once

#include <Arduino.h>

class TouchPad {
 public:
  enum class Event { None, Tap, Hold, LongHold };

  static constexpr uint32_t kHoldMs = 3000;
  static constexpr uint32_t kLongHoldMs = 10000;

  void begin(uint8_t pin);
  Event poll(uint32_t nowMs);
  bool pressed() const { return pressed_; }
  uint32_t heldFor(uint32_t nowMs) const { return pressed_ ? nowMs - pressStartMs_ : 0; }

 private:
  uint32_t read() const;

  uint8_t pin_ = 0;
  uint32_t baseline_ = 0;
  bool pressed_ = false;
  uint8_t streak_ = 0;
  uint32_t pressStartMs_ = 0;
  bool holdFired_ = false;
  bool longHoldFired_ = false;
};
