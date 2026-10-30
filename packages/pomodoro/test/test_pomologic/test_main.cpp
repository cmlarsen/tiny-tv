#include <unity.h>

#include "pomologic.h"

using pomo::App;
using pomo::Gesture;
using pomo::Gestures;
using pomo::Page;
using pomo::Phase;
using pomo::Settings;
using pomo::Timer;

static uint32_t clock_ms = 0;

static Gesture run(Gestures& g, uint32_t heldMs, uint32_t ms) {
  Gesture seen = Gesture::None;
  for (uint32_t end = clock_ms + ms; clock_ms < end; clock_ms += 10) {
    const uint32_t held = heldMs ? heldMs + (clock_ms - (end - ms)) : 0;
    const Gesture e = g.feed(false, held, clock_ms);
    if (e != Gesture::None) seen = e;
  }
  return seen;
}

static Gesture press(Gestures& g, uint32_t ms) {
  Gesture seen = run(g, 1, ms);
  const Gesture e = g.feed(ms < 3000, 0, clock_ms);
  if (e != Gesture::None) seen = e;
  clock_ms += 10;
  return seen;
}

void setUp() { clock_ms = 1000; }
void tearDown() {}

void test_single_tap_fires_after_double_window() {
  Gestures g;
  TEST_ASSERT_EQUAL(Gesture::None, press(g, 120));
  TEST_ASSERT_EQUAL(Gesture::Tap, run(g, 0, Gestures::kDoubleMs + 50));
}

void test_double_tap() {
  Gestures g;
  TEST_ASSERT_EQUAL(Gesture::None, press(g, 120));
  run(g, 0, 150);
  TEST_ASSERT_EQUAL(Gesture::DoubleTap, press(g, 120));
  TEST_ASSERT_EQUAL(Gesture::None, run(g, 0, 1000));
}

void test_slow_second_tap_is_two_taps() {
  Gestures g;
  press(g, 120);
  TEST_ASSERT_EQUAL(Gesture::Tap, run(g, 0, 600));
  press(g, 120);
  TEST_ASSERT_EQUAL(Gesture::Tap, run(g, 0, 600));
}

void test_second_press_held_past_window_still_double() {
  Gestures g;
  press(g, 120);
  run(g, 0, 300);
  TEST_ASSERT_EQUAL(Gesture::DoubleTap, press(g, 300));
}

void test_double_tap_without_held_samples() {
  Gestures g;
  TEST_ASSERT_EQUAL(Gesture::None, g.feed(true, 0, 1000));
  TEST_ASSERT_EQUAL(Gesture::None, g.feed(false, 0, 1100));
  TEST_ASSERT_EQUAL(Gesture::DoubleTap, g.feed(true, 0, 1200));
}

void test_long_press_fires_once_without_tap() {
  Gestures g;
  TEST_ASSERT_EQUAL(Gesture::LongPress, press(g, 1500));
  TEST_ASSERT_EQUAL(Gesture::None, run(g, 0, 1000));
  TEST_ASSERT_EQUAL(Gesture::None, press(g, 120));
  TEST_ASSERT_EQUAL(Gesture::Tap, run(g, 0, 600));
}

void test_very_long_press_then_tap() {
  Gestures g;
  TEST_ASSERT_EQUAL(Gesture::LongPress, press(g, 4000));
  run(g, 0, 500);
  press(g, 120);
  TEST_ASSERT_EQUAL(Gesture::Tap, run(g, 0, 600));
}

void test_step_minutes_clamps() {
  TEST_ASSERT_EQUAL(30, pomo::stepMinutes(25, 1));
  TEST_ASSERT_EQUAL(20, pomo::stepMinutes(25, -1));
  TEST_ASSERT_EQUAL(120, pomo::stepMinutes(120, 1));
  TEST_ASSERT_EQUAL(5, pomo::stepMinutes(5, -1));
}

void test_format_clock() {
  char buf[12];
  pomo::formatClock(25 * 60000, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("25:00", buf);
  pomo::formatClock(59001, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("01:00", buf);
  pomo::formatClock(0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("00:00", buf);
  pomo::formatClock(120 * 60000, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("120:00", buf);
}

void test_full_cycle_repeats() {
  Settings s;
  s.focusMin = 25;
  s.breakMin = 5;
  Timer t;
  t.start(s, 0);
  TEST_ASSERT_EQUAL(Phase::Focus, t.phase());
  t.update(s, 12 * 60000 + 30000);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, t.progress(12 * 60000 + 30000));
  uint32_t now = 25 * 60000;
  t.update(s, now);
  TEST_ASSERT_EQUAL(Phase::FocusDone, t.phase());
  TEST_ASSERT_EQUAL(1, t.completed());
  now += Timer::kFlashMs;
  t.update(s, now);
  TEST_ASSERT_EQUAL(Phase::Break, t.phase());
  TEST_ASSERT_EQUAL_UINT32(5 * 60000, t.remainingMs(now));
  now += 5 * 60000;
  t.update(s, now);
  TEST_ASSERT_EQUAL(Phase::BreakDone, t.phase());
  now += Timer::kFlashMs;
  t.update(s, now);
  TEST_ASSERT_EQUAL(Phase::Focus, t.phase());
  TEST_ASSERT_EQUAL_UINT32(25 * 60000, t.remainingMs(now));
}

void test_pause_freezes_time() {
  Settings s;
  Timer t;
  t.start(s, 0);
  t.togglePause(60000);
  t.update(s, 30 * 60000);
  TEST_ASSERT_EQUAL(Phase::Focus, t.phase());
  TEST_ASSERT_EQUAL_UINT32(24 * 60000, t.remainingMs(30 * 60000));
  t.togglePause(30 * 60000);
  TEST_ASSERT_EQUAL_UINT32(24 * 60000, t.remainingMs(30 * 60000));
  t.update(s, 54 * 60000);
  TEST_ASSERT_EQUAL(Phase::FocusDone, t.phase());
}

void test_app_tap_starts_and_double_tap_resets() {
  App a;
  a.handle(Gesture::Tap, 0);
  TEST_ASSERT_EQUAL(Phase::Focus, a.timer.phase());
  a.handle(Gesture::DoubleTap, 1000);
  TEST_ASSERT_EQUAL(Phase::Ready, a.timer.phase());
  TEST_ASSERT_EQUAL(0, a.timer.completed());
}

void test_app_settings_flow() {
  App a;
  TEST_ASSERT_FALSE(a.handle(Gesture::LongPress, 0));
  TEST_ASSERT_TRUE(a.inSettings);
  TEST_ASSERT_EQUAL(Page::Focus, a.page);
  a.handle(Gesture::Tap, 0);
  a.handle(Gesture::Tap, 0);
  TEST_ASSERT_EQUAL(35, a.settings.focusMin);
  a.handle(Gesture::DoubleTap, 0);
  TEST_ASSERT_EQUAL(30, a.settings.focusMin);
  TEST_ASSERT_EQUAL(Phase::Ready, a.timer.phase());

  a.handle(Gesture::LongPress, 0);
  TEST_ASSERT_EQUAL(Page::Break, a.page);
  a.handle(Gesture::DoubleTap, 0);
  TEST_ASSERT_EQUAL(5, a.settings.breakMin);
  a.handle(Gesture::Tap, 0);
  TEST_ASSERT_EQUAL(10, a.settings.breakMin);

  a.handle(Gesture::LongPress, 0);
  TEST_ASSERT_EQUAL(Page::Theme, a.page);
  a.handle(Gesture::DoubleTap, 0);
  TEST_ASSERT_EQUAL(pomo::kThemeCount - 1, a.settings.theme);
  a.handle(Gesture::Tap, 0);
  a.handle(Gesture::Tap, 0);
  TEST_ASSERT_EQUAL(1, a.settings.theme);

  TEST_ASSERT_TRUE(a.handle(Gesture::LongPress, 0));
  TEST_ASSERT_FALSE(a.inSettings);
}

void test_app_tap_pauses_and_skips_flash() {
  App a;
  a.handle(Gesture::Tap, 0);
  a.handle(Gesture::Tap, 1000);
  TEST_ASSERT_TRUE(a.timer.paused());
  a.handle(Gesture::Tap, 2000);
  TEST_ASSERT_FALSE(a.timer.paused());
  a.timer.update(a.settings, 26 * 60000);
  TEST_ASSERT_EQUAL(Phase::FocusDone, a.timer.phase());
  a.handle(Gesture::Tap, 26 * 60000);
  TEST_ASSERT_EQUAL(Phase::Break, a.timer.phase());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_single_tap_fires_after_double_window);
  RUN_TEST(test_double_tap);
  RUN_TEST(test_slow_second_tap_is_two_taps);
  RUN_TEST(test_second_press_held_past_window_still_double);
  RUN_TEST(test_double_tap_without_held_samples);
  RUN_TEST(test_long_press_fires_once_without_tap);
  RUN_TEST(test_very_long_press_then_tap);
  RUN_TEST(test_step_minutes_clamps);
  RUN_TEST(test_format_clock);
  RUN_TEST(test_full_cycle_repeats);
  RUN_TEST(test_pause_freezes_time);
  RUN_TEST(test_app_tap_starts_and_double_tap_resets);
  RUN_TEST(test_app_settings_flow);
  RUN_TEST(test_app_tap_pauses_and_skips_flash);
  return UNITY_END();
}
