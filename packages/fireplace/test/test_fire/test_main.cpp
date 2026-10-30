#include <unity.h>

#include <string.h>

#include "fire.h"

void setUp() {}
void tearDown() {}

static long rowSum(const fire::Sim& s, int y) {
  long sum = 0;
  for (int x = 0; x < fire::kW; x++) sum += s.at(x, y);
  return sum;
}

void test_palette_runs_black_to_bright() {
  uint8_t r, g, b;
  fire::heatColor(0, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(0, r);
  TEST_ASSERT_EQUAL_UINT8(0, g);
  TEST_ASSERT_EQUAL_UINT8(0, b);
  fire::heatColor(255, r, g, b);
  TEST_ASSERT_TRUE(r > 240 && g > 200 && b > 100);
  int prev = -1;
  for (int h = 0; h < 256; h++) {
    fire::heatColor(h, r, g, b);
    const int lum = r * 3 + g * 6 + b;
    TEST_ASSERT_TRUE(lum >= prev);
    prev = lum;
  }
}

void test_palette_is_byte_swapped_rgb565() {
  uint16_t pal[256];
  fire::buildPalette(pal);
  TEST_ASSERT_EQUAL_HEX16(0x0000, pal[0]);
  uint8_t r, g, b;
  fire::heatColor(200, r, g, b);
  const uint16_t v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  TEST_ASSERT_EQUAL_HEX16(static_cast<uint16_t>((v >> 8) | (v << 8)), pal[200]);
}

void test_same_seed_same_fire() {
  static fire::Sim a(7), b(7);
  for (int i = 0; i < 50; i++) {
    a.step(fire::level(1));
    b.step(fire::level(1));
  }
  for (int y = 0; y < fire::kH; y++)
    for (int x = 0; x < fire::kW; x++) TEST_ASSERT_EQUAL_UINT8(a.at(x, y), b.at(x, y));
}

void test_burning_fire_is_hotter_low_than_high() {
  static fire::Sim s(3);
  for (int i = 0; i < 200; i++) s.step(fire::level(1));
  TEST_ASSERT_TRUE(rowSum(s, fire::kVisH - 1) > 40L * fire::kW);
  TEST_ASSERT_TRUE(rowSum(s, fire::kVisH - 1) > rowSum(s, 0) * 4);
  TEST_ASSERT_TRUE(rowSum(s, fire::kVisH / 2) > 0);
}

void test_flames_are_brighter_in_the_middle_than_at_the_edges() {
  static fire::Sim s(5);
  long mid = 0, edge = 0;
  for (int i = 0; i < 300; i++) {
    s.step(fire::level(1));
    for (int y = fire::kVisH / 2; y < fire::kVisH; y++) {
      mid += s.at(fire::kW / 2, y);
      edge += s.at(1, y);
    }
  }
  TEST_ASSERT_TRUE(mid > edge * 3);
}

void test_no_fuel_burns_out() {
  static fire::Sim s(9);
  for (int i = 0; i < 100; i++) s.step(fire::level(2));
  for (int i = 0; i < 400; i++) s.step(fire::Params{0, 8});
  for (int y = 0; y < fire::kVisH; y++) TEST_ASSERT_EQUAL(0, rowSum(s, y));
}

void test_bigger_levels_burn_taller() {
  static fire::Sim lo(11), hi(11);
  long loSum = 0, hiSum = 0;
  for (int i = 0; i < 300; i++) {
    lo.step(fire::level(0));
    hi.step(fire::level(2));
    if (i >= 100) {
      loSum += rowSum(lo, fire::kVisH / 3);
      hiSum += rowSum(hi, fire::kVisH / 3);
    }
  }
  TEST_ASSERT_TRUE(hiSum > loSum * 2);
}

void test_render_row_upscales_and_interpolates() {
  static fire::Sim s(1);
  uint16_t pal[256];
  for (int i = 0; i < 256; i++) pal[i] = static_cast<uint16_t>(i);
  memset(s.row(10), 100, fire::kW);
  memset(s.row(11), 200, fire::kW);
  s.row(10)[5] = 0;
  uint16_t out[fire::kW * 2];

  fire::renderRow(s, 20, pal, out);
  TEST_ASSERT_EQUAL_UINT16(100, out[0]);
  TEST_ASSERT_EQUAL_UINT16(0, out[10]);
  TEST_ASSERT_EQUAL_UINT16(50, out[9]);
  TEST_ASSERT_EQUAL_UINT16(50, out[11]);
  TEST_ASSERT_EQUAL_UINT16(100, out[fire::kW * 2 - 1]);

  fire::renderRow(s, 21, pal, out);
  TEST_ASSERT_EQUAL_UINT16(150, out[0]);
  TEST_ASSERT_EQUAL_UINT16(100, out[10]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_palette_runs_black_to_bright);
  RUN_TEST(test_palette_is_byte_swapped_rgb565);
  RUN_TEST(test_same_seed_same_fire);
  RUN_TEST(test_burning_fire_is_hotter_low_than_high);
  RUN_TEST(test_flames_are_brighter_in_the_middle_than_at_the_edges);
  RUN_TEST(test_no_fuel_burns_out);
  RUN_TEST(test_bigger_levels_burn_taller);
  RUN_TEST(test_render_row_upscales_and_interpolates);
  return UNITY_END();
}
