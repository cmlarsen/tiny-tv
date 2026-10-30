#include <unity.h>

#include <string.h>

#include "zoo.h"

using namespace ca;

void setUp() {}
void tearDown() {}

void test_begins_on_the_first_mode_with_its_label() {
  Zoo zoo(1);
  zoo.begin(1000);
  TEST_ASSERT_EQUAL(0, zoo.index());
  TEST_ASSERT_TRUE(zoo.labelVisible(1000 + Zoo::kLabelMs - 1));
  TEST_ASSERT_FALSE(zoo.labelVisible(1000 + Zoo::kLabelMs));
}

void test_next_cycles_through_every_mode() {
  Zoo zoo(1);
  zoo.begin(0);
  const char* seen[kModes];
  for (int i = 0; i < kModes; i++) {
    seen[i] = zoo.mode().name();
    for (int j = 0; j < i; j++) TEST_ASSERT_TRUE(strcmp(seen[i], seen[j]) != 0);
    zoo.next(10);
  }
  TEST_ASSERT_EQUAL(0, zoo.index());
}

void test_stays_on_a_mode_until_told_to_change() {
  Zoo zoo(1);
  zoo.begin(0);
  for (int i = 0; i < 100; i++) zoo.mode().tick();
  TEST_ASSERT_EQUAL(0, zoo.index());
  zoo.next(60 * 60 * 1000);
  TEST_ASSERT_EQUAL(1, zoo.index());
}

void test_next_variant_stays_on_the_mode() {
  Zoo zoo(1);
  zoo.select(2, 0, 0);
  TEST_ASSERT_EQUAL(0, zoo.mode().variant());
  zoo.nextVariant(5000);
  TEST_ASSERT_EQUAL(2, zoo.index());
  TEST_ASSERT_EQUAL(1, zoo.mode().variant());
  TEST_ASSERT_TRUE(zoo.labelVisible(5000));
  for (int i = 1; i < zoo.mode().variants(); i++) zoo.nextVariant(5000);
  TEST_ASSERT_EQUAL(0, zoo.mode().variant());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_begins_on_the_first_mode_with_its_label);
  RUN_TEST(test_next_cycles_through_every_mode);
  RUN_TEST(test_stays_on_a_mode_until_told_to_change);
  RUN_TEST(test_next_variant_stays_on_the_mode);
  return UNITY_END();
}
