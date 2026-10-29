#pragma once

#include <stdint.h>

class TouchLogic {
 public:
  enum class Event { None, Tap, Hold, LongHold };

  static constexpr uint32_t kHoldMs = 3000;
  static constexpr uint32_t kLongHoldMs = 10000;
  static constexpr uint32_t kPressPct = 92;
  static constexpr uint32_t kReleasePct = 95;
  static constexpr uint32_t kTrackPct = 97;

  void begin(uint32_t baseline) {
    base256_ = static_cast<int32_t>(baseline) << 8;
    pressed_ = false;
    streak_ = 0;
  }

  Event feed(uint32_t value, uint32_t nowMs) {
    last_ = value;
    const uint32_t base = baseline();
    const bool below = value * 100 < base * kPressPct;
    const bool above = value * 100 > base * kReleasePct;

    if (!pressed_) {
      if (value * 100 >= base * kTrackPct) base256_ += ((static_cast<int32_t>(value) << 8) - base256_) / 64;
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
      return holdFired_ ? Event::None : Event::Tap;
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

  bool pressed() const { return pressed_; }
  uint32_t heldFor(uint32_t nowMs) const { return pressed_ ? nowMs - pressStartMs_ : 0; }
  uint32_t baseline() const { return static_cast<uint32_t>((base256_ + 128) >> 8); }
  uint32_t lastReading() const { return last_; }

 private:
  int32_t base256_ = 0;
  uint32_t last_ = 0;
  bool pressed_ = false;
  uint8_t streak_ = 0;
  uint32_t pressStartMs_ = 0;
  bool holdFired_ = false;
  bool longHoldFired_ = false;
};
