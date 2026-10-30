#include "pomologic.h"

#include <stdio.h>

namespace pomo {

Gesture Gestures::feed(bool tapped, uint32_t heldMs, uint32_t nowMs) {
  if (heldMs > 0) {
    pressStartMs_ = nowMs - heldMs;
    sawPress_ = true;
    if (heldMs >= kLongMs && !longFired_) {
      longFired_ = true;
      pending_ = false;
      return Gesture::LongPress;
    }
    if (pending_ && pressStartMs_ - pendingAtMs_ > kDoubleMs) {
      pending_ = false;
      return Gesture::Tap;
    }
    return Gesture::None;
  }

  if (tapped) {
    if (!sawPress_) pressStartMs_ = nowMs;
    sawPress_ = false;
    if (longFired_) {
      longFired_ = false;
      return Gesture::None;
    }
    if (pending_ && pressStartMs_ - pendingAtMs_ <= kDoubleMs) {
      pending_ = false;
      return Gesture::DoubleTap;
    }
    const bool hadPending = pending_;
    pending_ = true;
    pendingAtMs_ = nowMs;
    return hadPending ? Gesture::Tap : Gesture::None;
  }

  longFired_ = false;
  if (pending_ && nowMs - pendingAtMs_ > kDoubleMs) {
    pending_ = false;
    return Gesture::Tap;
  }
  return Gesture::None;
}

int stepMinutes(int minutes, int dir) {
  const int m = minutes + dir * kStepMinutes;
  return m < kMinMinutes ? kMinMinutes : m > kMaxMinutes ? kMaxMinutes : m;
}

static uint32_t minutesMs(int m) { return static_cast<uint32_t>(m) * 60000; }

void Timer::enter(Phase phase, uint32_t durationMs, uint32_t nowMs) {
  phase_ = phase;
  durationMs_ = durationMs;
  phaseStartMs_ = nowMs;
  paused_ = false;
}

void Timer::start(const Settings& s, uint32_t nowMs) { enter(Phase::Focus, minutesMs(s.focusMin), nowMs); }

void Timer::reset() {
  enter(Phase::Ready, 0, 0);
  completed_ = 0;
}

void Timer::togglePause(uint32_t nowMs) {
  if (phase_ != Phase::Focus && phase_ != Phase::Break) return;
  if (paused_) phaseStartMs_ += nowMs - pausedAtMs_;
  else pausedAtMs_ = nowMs;
  paused_ = !paused_;
}

void Timer::advance(const Settings& s, uint32_t nowMs) {
  switch (phase_) {
    case Phase::Ready: start(s, nowMs); break;
    case Phase::Focus:
      completed_++;
      enter(Phase::FocusDone, kFlashMs, nowMs);
      break;
    case Phase::FocusDone: enter(Phase::Break, minutesMs(s.breakMin), nowMs); break;
    case Phase::Break: enter(Phase::BreakDone, kFlashMs, nowMs); break;
    case Phase::BreakDone: start(s, nowMs); break;
  }
}

void Timer::update(const Settings& s, uint32_t nowMs) {
  if (phase_ == Phase::Ready || paused_) return;
  if (elapsedMs(nowMs) >= durationMs_) advance(s, phaseStartMs_ + durationMs_);
}

uint32_t Timer::elapsedMs(uint32_t nowMs) const {
  if (phase_ == Phase::Ready) return 0;
  const uint32_t e = (paused_ ? pausedAtMs_ : nowMs) - phaseStartMs_;
  return e > durationMs_ ? durationMs_ : e;
}

uint32_t Timer::remainingMs(uint32_t nowMs) const { return durationMs_ - elapsedMs(nowMs); }

float Timer::progress(uint32_t nowMs) const {
  return durationMs_ ? static_cast<float>(elapsedMs(nowMs)) / durationMs_ : 0;
}

void Timer::force(Phase phase, uint32_t durationMs, float progress, uint32_t nowMs) {
  enter(phase, durationMs, nowMs - static_cast<uint32_t>(durationMs * progress));
}

bool App::handle(Gesture g, uint32_t nowMs) {
  if (g == Gesture::None) return false;
  if (inSettings) {
    const int dir = g == Gesture::Tap ? 1 : -1;
    switch (g) {
      case Gesture::LongPress:
        if (page == Page::Theme) {
          inSettings = false;
          return true;
        }
        page = page == Page::Focus ? Page::Break : Page::Theme;
        break;
      default:
        if (page == Page::Focus) settings.focusMin = stepMinutes(settings.focusMin, dir);
        else if (page == Page::Break) settings.breakMin = stepMinutes(settings.breakMin, dir);
        else settings.theme = (settings.theme + dir + kThemeCount) % kThemeCount;
        break;
    }
    return false;
  }

  switch (g) {
    case Gesture::LongPress:
      inSettings = true;
      page = Page::Focus;
      break;
    case Gesture::DoubleTap: timer.reset(); break;
    case Gesture::Tap:
      if (timer.phase() == Phase::Focus || timer.phase() == Phase::Break) timer.togglePause(nowMs);
      else timer.advance(settings, nowMs);
      break;
    default: break;
  }
  return false;
}

void formatClock(uint32_t ms, char* out, size_t len) {
  const uint32_t s = (ms + 999) / 1000;
  snprintf(out, len, "%02lu:%02lu", static_cast<unsigned long>(s / 60), static_cast<unsigned long>(s % 60));
}

}  // namespace pomo
