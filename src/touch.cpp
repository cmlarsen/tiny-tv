#include "touch.h"

void TouchPad::begin(uint8_t pin) {
  pin_ = pin;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += touchRead(pin_);
  baseline_ = sum / 16;
}

uint32_t TouchPad::read() const { return (touchRead(pin_) + touchRead(pin_)) / 2; }

TouchPad::Event TouchPad::poll(uint32_t nowMs) {
  const uint32_t v = read();
  const bool below = v < baseline_ * 90 / 100;
  const bool above = v > baseline_ * 95 / 100;

  if (!pressed_) {
    if (above) baseline_ = (baseline_ * 63 + v) / 64;
    streak_ = below ? streak_ + 1 : 0;
    if (streak_ >= 2) {
      pressed_ = true;
      streak_ = 0;
      pressStartMs_ = nowMs;
      holdFired_ = false;
      longHoldFired_ = false;
    }
    return Event::None;
  }

  streak_ = above ? streak_ + 1 : 0;
  if (streak_ >= 2) {
    pressed_ = false;
    streak_ = 0;
    if (!holdFired_) return Event::Tap;
    return Event::None;
  }

  const uint32_t held = nowMs - pressStartMs_;
  if (!longHoldFired_ && held >= kLongHoldMs) {
    longHoldFired_ = true;
    return Event::LongHold;
  }
  if (!holdFired_ && held >= kHoldMs) {
    holdFired_ = true;
    return Event::Hold;
  }
  return Event::None;
}
