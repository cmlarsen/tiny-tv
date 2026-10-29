#include <unity.h>

#include "touchlogic.h"

using Event = TouchLogic::Event;

static uint32_t clock_ms = 0;

static Event feedFor(TouchLogic& t, uint32_t value, uint32_t ms) {
  Event last = Event::None;
  for (uint32_t end = clock_ms + ms; clock_ms < end; clock_ms += 10) {
    const Event e = t.feed(value, clock_ms);
    if (e != Event::None) last = e;
  }
  return last;
}

static Event tap(TouchLogic& t, uint32_t touched) {
  const uint32_t steps[][2] = {{96, 40}, {94, 40}, {touched, 200}, {95, 40}, {101, 300}};
  Event seen = Event::None;
  for (const auto& st : steps) {
    const Event e = feedFor(t, st[0], st[1]);
    if (e != Event::None) seen = e;
  }
  return seen;
}

void setUp() { clock_ms = 0; }
void tearDown() {}

void test_idle_noise_never_fires() {
  TouchLogic t;
  t.begin(101);
  for (int i = 0; i < 2000; i++) {
    TEST_ASSERT_EQUAL(Event::None, t.feed(97 + (i * 7) % 5, clock_ms));
    clock_ms += 10;
  }
  TEST_ASSERT_FALSE(t.pressed());
}

void test_tap_with_small_swing() {
  TouchLogic t;
  t.begin(101);
  TEST_ASSERT_EQUAL(Event::Tap, tap(t, 88));
}

void test_many_taps_with_approach_do_not_drift_baseline() {
  TouchLogic t;
  t.begin(101);
  for (int i = 0; i < 50; i++) {
    TEST_ASSERT_EQUAL_MESSAGE(Event::Tap, tap(t, 86 + i % 4), "tap stopped registering");
  }
  TEST_ASSERT_UINT32_WITHIN(2, 101, t.baseline());
}

void test_baseline_can_rise_again() {
  TouchLogic t;
  t.begin(95);
  feedFor(t, 101, 20000);
  TEST_ASSERT_UINT32_WITHIN(1, 101, t.baseline());
}

void test_baseline_follows_slow_downward_drift() {
  TouchLogic t;
  t.begin(101);
  for (uint32_t v = 101; v >= 90; v--) feedFor(t, v, 3000);
  TEST_ASSERT_UINT32_WITHIN(2, 90, t.baseline());
  TEST_ASSERT_FALSE(t.pressed());
  TEST_ASSERT_EQUAL(Event::Tap, tap(t, 78));
}

void test_hold_then_long_hold_without_tap() {
  TouchLogic t;
  t.begin(101);
  feedFor(t, 86, 100);
  TEST_ASSERT_TRUE(t.pressed());
  TEST_ASSERT_EQUAL(Event::Hold, feedFor(t, 86, TouchLogic::kHoldMs));
  TEST_ASSERT_EQUAL(Event::LongHold, feedFor(t, 86, TouchLogic::kLongHoldMs - TouchLogic::kHoldMs));
  TEST_ASSERT_EQUAL(Event::None, feedFor(t, 101, 200));
  TEST_ASSERT_FALSE(t.pressed());
}

void test_single_glitch_sample_is_ignored() {
  TouchLogic t;
  t.begin(101);
  feedFor(t, 101, 100);
  t.feed(70, clock_ms);
  clock_ms += 10;
  TEST_ASSERT_EQUAL(Event::None, feedFor(t, 101, 500));
  TEST_ASSERT_FALSE(t.pressed());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_idle_noise_never_fires);
  RUN_TEST(test_tap_with_small_swing);
  RUN_TEST(test_many_taps_with_approach_do_not_drift_baseline);
  RUN_TEST(test_baseline_can_rise_again);
  RUN_TEST(test_baseline_follows_slow_downward_drift);
  RUN_TEST(test_hold_then_long_hold_without_tap);
  RUN_TEST(test_single_glitch_sample_is_ignored);
  return UNITY_END();
}
