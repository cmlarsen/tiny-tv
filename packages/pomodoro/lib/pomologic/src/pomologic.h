#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pomo {

enum class Gesture { None, Tap, DoubleTap, LongPress };

class Gestures {
 public:
  static constexpr uint32_t kDoubleMs = 350;
  static constexpr uint32_t kLongMs = 700;

  Gesture feed(bool tapped, uint32_t heldMs, uint32_t nowMs);

 private:
  bool pending_ = false;
  uint32_t pendingAtMs_ = 0;
  uint32_t pressStartMs_ = 0;
  bool sawPress_ = false;
  bool longFired_ = false;
};

constexpr int kMinMinutes = 5;
constexpr int kMaxMinutes = 120;
constexpr int kStepMinutes = 5;
constexpr int kThemeCount = 6;

int stepMinutes(int minutes, int dir);

struct Settings {
  int focusMin = 25;
  int breakMin = 5;
  int theme = 0;
};

enum class Phase { Ready, Focus, FocusDone, Break, BreakDone };

class Timer {
 public:
  static constexpr uint32_t kFlashMs = 5000;

  void start(const Settings& s, uint32_t nowMs);
  void reset();
  void togglePause(uint32_t nowMs);
  void advance(const Settings& s, uint32_t nowMs);
  void update(const Settings& s, uint32_t nowMs);

  Phase phase() const { return phase_; }
  bool paused() const { return paused_; }
  int completed() const { return completed_; }
  uint32_t elapsedMs(uint32_t nowMs) const;
  uint32_t remainingMs(uint32_t nowMs) const;
  float progress(uint32_t nowMs) const;
  uint32_t sinceDoneMs(uint32_t nowMs) const { return nowMs - phaseStartMs_; }

  void force(Phase phase, uint32_t durationMs, float progress, uint32_t nowMs);

 private:
  void enter(Phase phase, uint32_t durationMs, uint32_t nowMs);

  Phase phase_ = Phase::Ready;
  uint32_t durationMs_ = 0;
  uint32_t phaseStartMs_ = 0;
  bool paused_ = false;
  uint32_t pausedAtMs_ = 0;
  int completed_ = 0;
};

enum class Page { Focus, Break, Theme };

struct App {
  Settings settings;
  Timer timer;
  bool inSettings = false;
  Page page = Page::Focus;

  bool handle(Gesture g, uint32_t nowMs);
};

void formatClock(uint32_t ms, char* out, size_t len);

}  // namespace pomo
