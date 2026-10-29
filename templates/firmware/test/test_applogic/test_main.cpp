#include <unity.h>

#include "applogic.h"

void setUp() {}
void tearDown() {}

void test_tap_label() {
  char buf[24];
  applogic::tapLabel(0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Tap the pad", buf);
  applogic::tapLabel(1, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("1 tap", buf);
  applogic::tapLabel(12, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("12 taps", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tap_label);
  return UNITY_END();
}
