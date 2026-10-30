#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "crawl.h"
#include "rle.h"

using namespace crawl;

void setUp() {}
void tearDown() {}

static int reachable(const Game& g, bool& stairs) {
  bool seen[kMapSize][kMapSize] = {};
  int qx[kMapSize * kMapSize], qy[kMapSize * kMapSize];
  int head = 0, tail = 0, count = 0;
  qx[tail] = g.px;
  qy[tail++] = g.py;
  seen[g.py][g.px] = true;
  stairs = false;
  while (head < tail) {
    const int x = qx[head], y = qy[head++];
    count++;
    stairs |= g.map.at(x, y) == Cell::StairsDown;
    for (int d = 0; d < 4; d++) {
      const int nx = x + dx(static_cast<Dir>(d)), ny = y + dy(static_cast<Dir>(d));
      const Cell c = g.map.at(nx, ny);
      if (!g.map.inside(nx, ny) || seen[ny][nx] || !(walkable(c) || c == Cell::DoorClosed)) continue;
      seen[ny][nx] = true;
      qx[tail] = nx;
      qy[tail++] = ny;
    }
  }
  return count;
}

void test_every_open_cell_is_reachable_and_stairs_exist() {
  for (uint32_t seed = 1; seed <= 200; seed++) {
    Game g;
    g.begin(seed);
    int open = 0;
    for (int y = 0; y < kMapSize; y++)
      for (int x = 0; x < kMapSize; x++) {
        const Cell c = g.map.cell[y][x];
        open += walkable(c) || c == Cell::DoorClosed;
      }
    bool stairs;
    TEST_ASSERT_EQUAL(open, reachable(g, stairs));
    TEST_ASSERT_TRUE(stairs);
    TEST_ASSERT_TRUE(walkable(g.map.at(g.px + dx(g.facing), g.py + dy(g.facing))) ||
                     g.map.at(g.px + dx(g.facing), g.py + dy(g.facing)) == Cell::DoorClosed);
    for (int x = 0; x < kMapSize; x++) {
      TEST_ASSERT_TRUE(blocksView(g.map.cell[0][x]));
      TEST_ASSERT_TRUE(blocksView(g.map.cell[kMapSize - 1][x]));
    }
  }
}

void test_view_cell_rotates_with_facing() {
  Game g;
  g.begin(7);
  g.px = 5;
  g.py = 5;
  int x, y;
  g.facing = North;
  g.viewCell(2, 1, x, y);
  TEST_ASSERT_EQUAL(6, x);
  TEST_ASSERT_EQUAL(3, y);
  g.facing = East;
  g.viewCell(1, 1, x, y);
  TEST_ASSERT_EQUAL(6, x);
  TEST_ASSERT_EQUAL(6, y);
  g.facing = West;
  g.viewCell(3, -1, x, y);
  TEST_ASSERT_EQUAL(2, x);
  TEST_ASSERT_EQUAL(6, y);
}

void test_monster_ahead_starts_combat_that_ends() {
  Game g;
  g.begin(3);
  g.monsterCount = 0;
  TEST_ASSERT_TRUE(g.spawnAhead(0) >= 0);
  Outcome o = g.step();
  TEST_ASSERT_EQUAL(Event::Encounter, o.event);
  TEST_ASSERT_EQUAL(Mode::Combat, g.mode);
  bool killed = false;
  for (int i = 0; i < 60 && !killed; i++) killed = g.step().event == Event::MonsterKilled;
  TEST_ASSERT_TRUE(killed);
  TEST_ASSERT_EQUAL(Mode::Explore, g.mode);
  TEST_ASSERT_EQUAL(1, g.kills);
}

void test_autopilot_descends_and_eventually_dies() {
  int deepest = 0, wipes = 0, totalFloor = 0;
  for (uint32_t seed = 1; seed <= 100; seed++) {
    Game g;
    g.begin(seed * 7919);
    int steps = 0;
    while (g.mode != Mode::Wiped && steps < 200000) {
      g.step();
      steps++;
    }
    if (g.mode == Mode::Wiped) wipes++;
    totalFloor += g.floor;
    if (g.floor > deepest) deepest = g.floor;
  }
  char msg[96];
  snprintf(msg, sizeof msg, "wipes %d/100, mean floor %.1f, deepest %d", wipes, totalFloor / 100.0, deepest);
  TEST_MESSAGE(msg);
  TEST_ASSERT_EQUAL(100, wipes);
  TEST_ASSERT_TRUE(totalFloor / 100.0 >= 2.0);
}

void test_rle_blit_runs_literals_skips_and_clipping() {
  const uint8_t src[] = {
      (rle::Skip << 6) | 0, (rle::Run << 6) | 1, 2, (rle::Literal << 6) | 0, 3,
      (rle::Literal << 6) | 3, 1, 2, 3, 1,
  };
  const uint16_t pal[] = {0, 10, 20, 30};
  uint16_t buf[6 * 4];
  for (auto& v : buf) v = 99;
  rle::blit(src, 4, 2, 1, 0, 1, pal, {buf, 6, 0, 4});
  const uint16_t row0[] = {99, 99, 20, 20, 30, 99};
  const uint16_t row1[] = {99, 10, 20, 30, 10, 99};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(row0, buf, 6);
  TEST_ASSERT_EQUAL_UINT16_ARRAY(row1, buf + 6, 6);

  for (auto& v : buf) v = 99;
  rle::blit(src, 4, 2, -2, 1, 2, pal, {buf, 6, 2, 4});
  const uint16_t scaled0[] = {20, 20, 20, 20, 30, 30};
  const uint16_t scaled1[] = {20, 20, 30, 30, 10, 10};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(scaled0, buf, 6);
  TEST_ASSERT_EQUAL_UINT16_ARRAY(scaled1, buf + 6, 6);
  TEST_ASSERT_EQUAL_UINT16_ARRAY(scaled1, buf + 12, 6);
  const uint16_t untouched[] = {99, 99, 99, 99, 99, 99};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, buf + 18, 6);
}

static int stainedWalls(Game& g, int floor) {
  g.jumpToFloor(floor);
  int n = 0;
  for (int y = 0; y < kMapSize; y++)
    for (int x = 0; x < kMapSize; x++) n += g.map.stain[y][x] != 0;
  return n;
}

void test_walls_get_bloodier_on_each_pass_through_the_themes() {
  Game g;
  g.begin(11);
  g.themeCount = 12;
  TEST_ASSERT_EQUAL(0, stainedWalls(g, 1));
  TEST_ASSERT_EQUAL(0, stainedWalls(g, 12));
  const int second = stainedWalls(g, 13);
  const int fourth = stainedWalls(g, 37);
  TEST_ASSERT_TRUE(second > 0);
  TEST_ASSERT_TRUE(fourth > second);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_open_cell_is_reachable_and_stairs_exist);
  RUN_TEST(test_view_cell_rotates_with_facing);
  RUN_TEST(test_monster_ahead_starts_combat_that_ends);
  RUN_TEST(test_autopilot_descends_and_eventually_dies);
  RUN_TEST(test_walls_get_bloodier_on_each_pass_through_the_themes);
  RUN_TEST(test_rle_blit_runs_literals_skips_and_clipping);
  return UNITY_END();
}
