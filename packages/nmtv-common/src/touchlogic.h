#pragma once

#include <stdint.h>

class TouchLogic {
 public:
  enum class Event { None, Tap, Hold, Reset };

  static constexpr uint32_t kHoldMs = 3000;
  static constexpr uint32_t kResetHoldMs = 5000;
  static constexpr uint32_t kArmWindowMs = 30000;
  static constexpr uint32_t kStuckMs = 60000;
  static constexpr uint32_t kPressPct = 92;
  static constexpr uint32_t kReleasePct = 95;
  static constexpr uint32_t kTrackPct = 97;

  void begin(uint32_t baseline) {
    base256_ = static_cast<int32_t>(baseline) << 8;
    pressed_ = false;
    armed_ = false;
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
        armedPress_ = resetArmed(nowMs);
        armed_ = false;
        holdFired_ = false;
        resetFired_ = false;
      }
      return Event::None;
    }

    const uint32_t held = nowMs - pressStartMs_;
    if (held >= kStuckMs) {
      base256_ = static_cast<int32_t>(value) << 8;
      pressed_ = false;
      streak_ = 0;
      return Event::None;
    }

    streak_ = above ? streak_ + 1 : 0;
    if (streak_ >= 2) {
      pressed_ = false;
      streak_ = 0;
      if (holdFired_ && !armedPress_) {
        armed_ = true;
        armedUntilMs_ = nowMs + kArmWindowMs;
      }
      return held < kHoldMs ? Event::Tap : Event::None;
    }

    if (armedPress_) {
      if (!resetFired_ && held >= kResetHoldMs) {
        resetFired_ = true;
        return Event::Reset;
      }
    } else if (!holdFired_ && held >= kHoldMs) {
      holdFired_ = true;
      return Event::Hold;
    }
    return Event::None;
  }

  bool pressed() const { return pressed_; }
  uint32_t heldFor(uint32_t nowMs) const { return pressed_ ? nowMs - pressStartMs_ : 0; }
  bool resetArmed(uint32_t nowMs) const { return armed_ && static_cast<int32_t>(armedUntilMs_ - nowMs) >= 0; }
  float resetProgress(uint32_t nowMs) const {
    if (!pressed_ || !armedPress_) return -1;
    const float p = static_cast<float>(nowMs - pressStartMs_) / kResetHoldMs;
    return p > 1 ? 1 : p;
  }
  uint32_t baseline() const { return static_cast<uint32_t>((base256_ + 128) >> 8); }
  uint32_t lastReading() const { return last_; }

 private:
  int32_t base256_ = 0;
  uint32_t last_ = 0;
  bool pressed_ = false;
  uint8_t streak_ = 0;
  uint32_t pressStartMs_ = 0;
  bool holdFired_ = false;
  bool armed_ = false;
  uint32_t armedUntilMs_ = 0;
  bool armedPress_ = false;
  bool resetFired_ = false;
};
