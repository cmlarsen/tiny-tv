#include <unity.h>

#include "modes.h"

using namespace ca;

void setUp() {}
void tearDown() {}

void test_px_is_byte_swapped_rgb565() {
  TEST_ASSERT_EQUAL_HEX16(0x0000, px({0, 0, 0}));
  TEST_ASSERT_EQUAL_HEX16(0x00F8, px({255, 0, 0}));
  TEST_ASSERT_EQUAL_HEX16(0x1F00, px({0, 0, 255}));
}

void test_life_blinker_oscillates() {
  static Life life;
  life.start(1, 0);
  life.clear();
  for (int x = 10; x <= 12; x++) life.set(x, 20, 0);
  life.step();
  TEST_ASSERT_FALSE(life.alive(10, 20));
  TEST_ASSERT_TRUE(life.alive(11, 19));
  TEST_ASSERT_TRUE(life.alive(11, 20));
  TEST_ASSERT_TRUE(life.alive(11, 21));
  TEST_ASSERT_EQUAL(3, life.population());
  life.step();
  TEST_ASSERT_TRUE(life.alive(10, 20));
  TEST_ASSERT_FALSE(life.alive(11, 19));
}

void test_life_glider_wraps_around_the_torus() {
  static Life life;
  life.start(1, 0);
  life.clear();
  const int g[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
  for (auto& c : g) life.set(Life::kN - 3 + c[0], Life::kN - 3 + c[1], 2);
  for (int i = 0; i < 4 * 4; i++) life.step();
  TEST_ASSERT_EQUAL(5, life.population());
  for (auto& c : g) TEST_ASSERT_TRUE(life.alive((Life::kN + 1 + c[0]) % Life::kN, (Life::kN + 1 + c[1]) % Life::kN));
}

void test_life_newborn_takes_the_majority_species() {
  static Life life;
  life.start(1, 0);
  life.clear();
  life.set(10, 9, 1);
  life.set(11, 9, 1);
  life.set(9, 11, 3);
  life.step();
  TEST_ASSERT_TRUE(life.alive(10, 10));
  TEST_ASSERT_EQUAL(1, life.species(10, 10));
}

void test_life_three_different_parents_give_the_missing_species() {
  static Life life;
  life.start(1, 0);
  life.clear();
  life.set(10, 9, 0);
  life.set(11, 9, 1);
  life.set(9, 11, 3);
  life.step();
  TEST_ASSERT_TRUE(life.alive(10, 10));
  TEST_ASSERT_EQUAL(2, life.species(10, 10));
}

void test_life_dead_cells_leave_a_fading_trail() {
  static Life life;
  life.start(1, 0);
  life.clear();
  life.set(30, 30, 0);
  life.step();
  const uint8_t first = life.trail(30, 30);
  TEST_ASSERT_TRUE(first > 100);
  life.step();
  TEST_ASSERT_TRUE(life.trail(30, 30) < first);
  for (int i = 0; i < 100; i++) life.step();
  TEST_ASSERT_EQUAL_UINT8(0, life.trail(30, 30));
}

void test_life_keeps_going_by_splashing_new_soup() {
  static Life life;
  life.start(5, 0);
  for (int i = 0; i < 6000; i++) life.tick();
  TEST_ASSERT_TRUE(life.population() > 0);
  int active = 0;
  for (int i = 0; i < 400; i++) {
    life.tick();
    active += life.activity();
  }
  TEST_ASSERT_TRUE(active > 200);
}

void test_brain_cycles_on_dying_off() {
  static Brain b;
  b.start(1, 0);
  b.clear();
  b.set(5, 5, Brain::On);
  b.step();
  TEST_ASSERT_EQUAL(Brain::Dying, b.at(5, 5));
  b.step();
  TEST_ASSERT_EQUAL(Brain::Off, b.at(5, 5));
}

void test_brain_birth_needs_exactly_two_on_neighbors() {
  static Brain b;
  b.start(1, 0);
  b.clear();
  b.set(10, 10, Brain::On);
  b.set(11, 10, Brain::On);
  b.set(30, 30, Brain::On);
  b.set(31, 30, Brain::On);
  b.set(32, 30, Brain::On);
  b.step();
  TEST_ASSERT_EQUAL(Brain::On, b.at(10, 9));
  TEST_ASSERT_EQUAL(Brain::On, b.at(11, 11));
  TEST_ASSERT_EQUAL(Brain::Off, b.at(31, 29));
  TEST_ASSERT_EQUAL(Brain::On, b.at(30, 29));
}

void test_brain_mandala_seed_is_symmetric() {
  static Brain b;
  b.start(9, 1);
  for (int i = 0; i < 40; i++) b.step();
  for (int y = 0; y < Brain::kN; y++)
    for (int x = 0; x < Brain::kN; x++) TEST_ASSERT_EQUAL(b.at(x, y), b.at(Brain::kN - 1 - x, y));
}

void test_cyclic_cell_advances_when_enough_neighbors_lead() {
  static Cyclic c;
  c.setRule({"t", 1, 2, 4, true});
  c.fill(0);
  c.set(10, 10, 1);
  c.step();
  TEST_ASSERT_EQUAL_UINT8(0, c.at(11, 10));
  c.set(12, 10, 1);
  c.step();
  TEST_ASSERT_EQUAL_UINT8(1, c.at(11, 10));
  TEST_ASSERT_EQUAL_UINT8(1, c.at(11, 11));
  TEST_ASSERT_EQUAL_UINT8(0, c.at(13, 10));
}

void test_cyclic_last_state_wraps_to_zero() {
  static Cyclic c;
  c.setRule({"t", 1, 1, 3, false});
  c.fill(2);
  c.set(5, 5, 0);
  c.step();
  TEST_ASSERT_EQUAL_UINT8(0, c.at(5, 4));
  TEST_ASSERT_EQUAL_UINT8(2, c.at(6, 6));
}

void test_cyclic_every_preset_stays_alive() {
  static Cyclic c;
  for (int v = 0; v < c.variants(); v++) {
    c.start(3, v);
    int changes = 0;
    for (int i = 0; i < 600; i++) changes = c.step();
    TEST_ASSERT_TRUE_MESSAGE(changes > 0, c.variantName());
  }
}

void test_physarum_agents_stay_on_screen_and_leave_trails() {
  static Physarum p;
  for (int v = 0; v < p.variants(); v++) {
    p.start(4, v);
    for (int i = 0; i < 50; i++) p.tick();
    for (int i = 0; i < Physarum::kAgents; i++) {
      TEST_ASSERT_TRUE(p.agentX(i) >= 0 && p.agentX(i) < Physarum::kN);
      TEST_ASSERT_TRUE(p.agentY(i) >= 0 && p.agentY(i) < Physarum::kN);
    }
    long sum = 0;
    for (int y = 0; y < Physarum::kN; y++)
      for (int x = 0; x < Physarum::kN; x++) sum += p.trail(x, y);
    TEST_ASSERT_TRUE_MESSAGE(sum > 100000, p.variantName());
  }
}

void test_physarum_trail_diffuses_and_decays() {
  static Physarum p;
  p.start(1, 0);
  uint8_t* t = p.trail();
  for (int i = 0; i < Physarum::kN * Physarum::kN; i++) t[i] = 0;
  t[100 * Physarum::kN + 100] = 180;
  p.diffuse();
  TEST_ASSERT_TRUE(p.trail(100, 100) < 180);
  TEST_ASSERT_TRUE(p.trail(101, 100) > 0);
  TEST_ASSERT_TRUE(p.trail(99, 101) > 0);
  TEST_ASSERT_EQUAL_UINT8(0, p.trail(103, 100));
  for (int i = 0; i < 200; i++) p.diffuse();
  TEST_ASSERT_EQUAL_UINT8(0, p.trail(100, 100));
}

void test_grayscott_untouched_field_is_steady() {
  static GrayScott g;
  g.start(1, 0);
  g.fill(1, 0);
  for (int i = 0; i < 50; i++) TEST_ASSERT_EQUAL(0, g.step());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, g.u(7, 7));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, g.v(7, 7));
}

void test_grayscott_every_preset_grows_a_bounded_pattern() {
  static GrayScott g;
  for (int v = 0; v < g.variants(); v++) {
    g.start(2, v);
    for (int i = 0; i < 500; i++) g.tick();
    int spots = 0;
    for (int y = 0; y < GrayScott::kN; y++)
      for (int x = 0; x < GrayScott::kN; x++) {
        TEST_ASSERT_TRUE(g.u(x, y) >= 0 && g.u(x, y) <= 1.0f);
        TEST_ASSERT_TRUE(g.v(x, y) >= 0 && g.v(x, y) <= 1.0f);
        if (g.v(x, y) > 0.15f) spots++;
      }
    TEST_ASSERT_TRUE_MESSAGE(spots > 400, g.variantName());
  }
}

void test_every_mode_renders_full_rows() {
  static Life life;
  static Brain brain;
  static Cyclic cyclic;
  static Physarum physarum;
  static GrayScott gs;
  Mode* modes[] = {&life, &brain, &cyclic, &physarum, &gs};
  uint16_t row[kScreen + 1];
  for (Mode* m : modes) {
    m->start(6, 0);
    for (int i = 0; i < 30; i++) m->tick();
    for (int y = 0; y < kScreen; y++) {
      row[kScreen] = 0xBEEF;
      m->renderRow(y, row);
      TEST_ASSERT_EQUAL_HEX16(0xBEEF, row[kScreen]);
    }
  }
}

static int splitRows(RowFn fn, void* ctx, int rows) { return fn(ctx, rows / 3, rows) + fn(ctx, 0, rows / 3); }

template <typename M>
static void renderAll(M& m, uint16_t* out) {
  for (int y = 0; y < kScreen; y++) m.renderRow(y, out + y * kScreen);
}

template <typename M>
static void checkSplitMatchesSerial(int variant, int ticks) {
  static uint16_t serial[kScreen * kScreen], split[kScreen * kScreen];
  static M m;
  m.start(42, variant);
  for (int i = 0; i < ticks; i++) m.tick();
  renderAll(m, serial);
  setRowRunner(splitRows);
  m.start(42, variant);
  for (int i = 0; i < ticks; i++) m.tick();
  renderAll(m, split);
  setRowRunner(nullptr);
  TEST_ASSERT_EQUAL_UINT16_ARRAY(serial, split, kScreen * kScreen);
}

void test_split_row_runner_matches_serial() {
  checkSplitMatchesSerial<Cyclic>(3, 40);
  checkSplitMatchesSerial<GrayScott>(1, 40);
  checkSplitMatchesSerial<Physarum>(0, 40);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_split_row_runner_matches_serial);
  RUN_TEST(test_px_is_byte_swapped_rgb565);
  RUN_TEST(test_life_blinker_oscillates);
  RUN_TEST(test_life_glider_wraps_around_the_torus);
  RUN_TEST(test_life_newborn_takes_the_majority_species);
  RUN_TEST(test_life_three_different_parents_give_the_missing_species);
  RUN_TEST(test_life_dead_cells_leave_a_fading_trail);
  RUN_TEST(test_life_keeps_going_by_splashing_new_soup);
  RUN_TEST(test_brain_cycles_on_dying_off);
  RUN_TEST(test_brain_birth_needs_exactly_two_on_neighbors);
  RUN_TEST(test_brain_mandala_seed_is_symmetric);
  RUN_TEST(test_cyclic_cell_advances_when_enough_neighbors_lead);
  RUN_TEST(test_cyclic_last_state_wraps_to_zero);
  RUN_TEST(test_cyclic_every_preset_stays_alive);
  RUN_TEST(test_physarum_agents_stay_on_screen_and_leave_trails);
  RUN_TEST(test_physarum_trail_diffuses_and_decays);
  RUN_TEST(test_grayscott_untouched_field_is_steady);
  RUN_TEST(test_grayscott_every_preset_grows_a_bounded_pattern);
  RUN_TEST(test_every_mode_renders_full_rows);
  return UNITY_END();
}
