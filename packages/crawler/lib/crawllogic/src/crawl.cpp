#include "crawl.h"

#include <stdio.h>
#include <initializer_list>
#include <string.h>

namespace crawl {

const MonsterKind kKinds[kKindCount] = {
    {"slime", "Slime", "oozes on", 6, 1, 3, 1},
    {"cave_bat", "Cave Bat", "bites", 5, 1, 4, 1},
    {"mushroom_creep", "Mushroom Creep", "spores", 9, 1, 4, 2},
    {"goblin", "Goblin", "stabs", 11, 2, 5, 2},
    {"fire_slime", "Fire Slime", "burns", 13, 2, 6, 3},
    {"skeleton", "Skeleton", "slashes", 15, 2, 7, 3},
    {"ghost", "Ghost", "chills", 16, 3, 7, 4},
    {"floating_eye", "Floating Eye", "zaps", 16, 3, 8, 4},
    {"mimic", "Mimic", "chomps", 22, 3, 9, 5},
    {"stone_golem", "Stone Golem", "pounds", 30, 4, 10, 6},
};

namespace {

constexpr int kStartHp = 36;
constexpr int kHpPerLevel = 8;

struct Point {
  int8_t x;
  int8_t y;
};

}  // namespace

bool walkable(Cell c) { return c == Cell::Floor || c == Cell::DoorOpen || c == Cell::StairsDown; }

bool blocksView(Cell c) { return c == Cell::Wall || c == Cell::DoorClosed || c == Cell::StairsUp; }

Dir turnRight(Dir d) { return static_cast<Dir>((d + 1) % 4); }
Dir turnLeft(Dir d) { return static_cast<Dir>((d + 3) % 4); }
int dx(Dir d) { return d == East ? 1 : d == West ? -1 : 0; }
int dy(Dir d) { return d == South ? 1 : d == North ? -1 : 0; }

void Game::begin(uint32_t seed) {
  rng_.seed(seed);
  maxHp = kStartHp;
  hp = maxHp;
  floor = 1;
  level = 1;
  xp = 0;
  kills = 0;
  killer = "";
  pendingLevelUp_ = false;
  newFloor();
}

void Game::jumpToFloor(int n) {
  floor = n < 1 ? 1 : n;
  newFloor();
}

int Game::stainPercent() const {
  const int pass = themeCount > 0 ? (floor - 1) / themeCount : 0;
  return pass * 15 > 45 ? 45 : pass * 15;
}

void Game::newFloor() {
  generate();
  placeMonsters();
  mode = Mode::Explore;
  foe = -1;
  stepsOnFloor = 0;
  potions = 3 + level / 2;
  tick_ = 0;
  markSeen();
  snprintf(message, sizeof message, "Floor %d", floor);
}

void Game::generate() {
  for (int y = 0; y < kMapSize; y++) {
    for (int x = 0; x < kMapSize; x++) {
      map.cell[y][x] = Cell::Wall;
      map.decal[y][x] = Decal::None;
      map.stain[y][x] = 0;
      map.seen[y][x] = false;
    }
  }

  constexpr int kNodes = (kMapSize - 1) / 2;
  bool visited[kNodes][kNodes] = {};
  Point stack[kNodes * kNodes];
  int top = 0;
  stack[top++] = {static_cast<int8_t>(rng_.below(kNodes)), static_cast<int8_t>(rng_.below(kNodes))};
  visited[stack[0].y][stack[0].x] = true;
  map.cell[stack[0].y * 2 + 1][stack[0].x * 2 + 1] = Cell::Floor;
  while (top > 0) {
    const Point p = stack[top - 1];
    Dir options[4];
    int n = 0;
    for (int d = 0; d < 4; d++) {
      const int nx = p.x + dx(static_cast<Dir>(d)), ny = p.y + dy(static_cast<Dir>(d));
      if (nx >= 0 && ny >= 0 && nx < kNodes && ny < kNodes && !visited[ny][nx]) options[n++] = static_cast<Dir>(d);
    }
    if (n == 0) {
      top--;
      continue;
    }
    const Dir d = options[rng_.below(n)];
    const int nx = p.x + dx(d), ny = p.y + dy(d);
    visited[ny][nx] = true;
    map.cell[p.y * 2 + 1 + dy(d)][p.x * 2 + 1 + dx(d)] = Cell::Floor;
    map.cell[ny * 2 + 1][nx * 2 + 1] = Cell::Floor;
    stack[top++] = {static_cast<int8_t>(nx), static_cast<int8_t>(ny)};
  }

  const int rooms = rng_.between(2, 4);
  for (int r = 0; r < rooms; r++) {
    const int w = rng_.below(2) ? 3 : 5, h = rng_.below(2) ? 3 : 5;
    const int x0 = 1 + 2 * rng_.below((kMapSize - 1 - w) / 2 + 1);
    const int y0 = 1 + 2 * rng_.below((kMapSize - 1 - h) / 2 + 1);
    for (int y = y0; y < y0 + h && y < kMapSize - 1; y++)
      for (int x = x0; x < x0 + w && x < kMapSize - 1; x++) map.cell[y][x] = Cell::Floor;
  }

  for (int i = 0; i < 14; i++) {
    const int x = 1 + rng_.below(kMapSize - 2), y = 1 + rng_.below(kMapSize - 2);
    if (map.cell[y][x] != Cell::Wall) continue;
    const bool ew = map.at(x - 1, y) == Cell::Floor && map.at(x + 1, y) == Cell::Floor &&
                    map.at(x, y - 1) == Cell::Wall && map.at(x, y + 1) == Cell::Wall;
    const bool ns = map.at(x, y - 1) == Cell::Floor && map.at(x, y + 1) == Cell::Floor &&
                    map.at(x - 1, y) == Cell::Wall && map.at(x + 1, y) == Cell::Wall;
    if (ew || ns) map.cell[y][x] = Cell::Floor;
  }

  for (int y = 1; y < kMapSize - 1; y++) {
    for (int x = 1; x < kMapSize - 1; x++) {
      if (map.cell[y][x] != Cell::Floor) continue;
      const bool ew = map.at(x - 1, y) == Cell::Floor && map.at(x + 1, y) == Cell::Floor &&
                      map.at(x, y - 1) == Cell::Wall && map.at(x, y + 1) == Cell::Wall;
      const bool ns = map.at(x, y - 1) == Cell::Floor && map.at(x, y + 1) == Cell::Floor &&
                      map.at(x - 1, y) == Cell::Wall && map.at(x + 1, y) == Cell::Wall;
      if ((ew || ns) && rng_.below(100) < 9) map.cell[y][x] = Cell::DoorClosed;
    }
  }

  for (int y = 0; y < kMapSize; y++) {
    for (int x = 0; x < kMapSize; x++) {
      if (map.cell[y][x] != Cell::Wall) continue;
      bool faces = false;
      for (int d = 0; d < 4; d++) faces |= map.at(x + dx(static_cast<Dir>(d)), y + dy(static_cast<Dir>(d))) == Cell::Floor;
      if (!faces) continue;
      const int roll = rng_.below(100);
      if (roll < 7) map.decal[y][x] = Decal::Torch;
      else if (roll < 9) map.decal[y][x] = Decal::Grate;
      else if (roll < 10) map.decal[y][x] = Decal::Lever;
      else if (rng_.below(100) < stainPercent()) map.stain[y][x] = static_cast<uint8_t>(1 + rng_.below(stainKinds));
    }
  }

  Point ends[kMapSize * kMapSize];
  int nEnds = 0;
  for (int y = 1; y < kMapSize - 1; y++) {
    for (int x = 1; x < kMapSize - 1; x++) {
      if (map.cell[y][x] != Cell::Floor) continue;
      int open = 0;
      for (int d = 0; d < 4; d++) open += map.at(x + dx(static_cast<Dir>(d)), y + dy(static_cast<Dir>(d))) != Cell::Wall;
      if (open == 1) ends[nEnds++] = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
    }
  }
  if (nEnds == 0) ends[nEnds++] = {static_cast<int8_t>(stack[0].x * 2 + 1), static_cast<int8_t>(stack[0].y * 2 + 1)};
  const Point start = ends[rng_.below(nEnds)];
  px = start.x;
  py = start.y;
  for (int d = 0; d < 4; d++) {
    const Dir dir = static_cast<Dir>(d);
    if (map.at(px + dx(dir), py + dy(dir)) != Cell::Wall) facing = dir;
  }
  const Dir back = turnRight(turnRight(facing));
  for (Dir d : {back, turnLeft(facing), turnRight(facing)}) {
    const int x = px + dx(d), y = py + dy(d);
    if (map.inside(x, y) && map.cell[y][x] == Cell::Wall) {
      map.cell[y][x] = Cell::StairsUp;
      map.decal[y][x] = Decal::None;
      break;
    }
  }

  int dist[kMapSize][kMapSize];
  memset(dist, -1, sizeof dist);
  Point queue[kMapSize * kMapSize];
  int head = 0, tail = 0;
  queue[tail++] = start;
  dist[py][px] = 0;
  Point far = start;
  while (head < tail) {
    const Point p = queue[head++];
    if (map.cell[p.y][p.x] == Cell::Floor && dist[p.y][p.x] > dist[far.y][far.x]) far = p;
    for (int d = 0; d < 4; d++) {
      const int nx = p.x + dx(static_cast<Dir>(d)), ny = p.y + dy(static_cast<Dir>(d));
      const Cell c = map.at(nx, ny);
      if ((walkable(c) || c == Cell::DoorClosed) && dist[ny][nx] < 0) {
        dist[ny][nx] = dist[p.y][p.x] + 1;
        queue[tail++] = {static_cast<int8_t>(nx), static_cast<int8_t>(ny)};
      }
    }
  }
  map.cell[far.y][far.x] = Cell::StairsDown;
}

void Game::placeMonsters() {
  monsterCount = 0;
  const int want = 5 + floor / 2 > kMaxMonsters ? kMaxMonsters : 5 + floor / 2;
  const int maxTier = floor + 1;
  for (int tries = 0; tries < 400 && monsterCount < want; tries++) {
    const int x = 1 + rng_.below(kMapSize - 2), y = 1 + rng_.below(kMapSize - 2);
    if (map.cell[y][x] != Cell::Floor || monsterAt(x, y) >= 0) continue;
    const int manhattan = (x > px ? x - px : px - x) + (y > py ? y - py : py - y);
    if (manhattan < 5) continue;
    int kind;
    do kind = rng_.below(kKindCount);
    while (kKinds[kind].tier > maxTier || (kKinds[kind].tier < maxTier - 3 && rng_.below(3)));
    Monster& m = monsters[monsterCount++];
    m.kind = static_cast<uint8_t>(kind);
    m.x = static_cast<int8_t>(x);
    m.y = static_cast<int8_t>(y);
    m.hp = static_cast<int16_t>(kKinds[kind].hp * (20 + 3 * (floor - 1)) / 20);
    m.alive = true;
  }
}

int Game::spawnAhead(int kind) {
  const int x = px + dx(facing), y = py + dy(facing);
  if (!walkable(map.at(x, y)) || monsterCount >= kMaxMonsters || monsterAt(x, y) >= 0) return -1;
  Monster& m = monsters[monsterCount];
  m.kind = static_cast<uint8_t>(kind % kKindCount);
  m.x = static_cast<int8_t>(x);
  m.y = static_cast<int8_t>(y);
  m.hp = static_cast<int16_t>(kKinds[m.kind].hp);
  m.alive = true;
  return monsterCount++;
}

void Game::viewCell(int depth, int col, int& x, int& y) const {
  const Dir right = turnRight(facing);
  x = px + dx(facing) * depth + dx(right) * col;
  y = py + dy(facing) * depth + dy(right) * col;
}

int Game::monsterAt(int x, int y) const {
  for (int i = 0; i < monsterCount; i++)
    if (monsters[i].alive && monsters[i].x == x && monsters[i].y == y) return i;
  return -1;
}

void Game::markSeen() {
  for (int d = 0; d < kViewDepth; d++) {
    int x, y;
    for (int col = -1; col <= 1; col++) {
      viewCell(d, col, x, y);
      if (map.inside(x, y)) map.seen[y][x] = true;
    }
    viewCell(d, 0, x, y);
    if (blocksView(map.at(x, y))) break;
  }
}

Outcome Game::step() {
  switch (mode) {
    case Mode::Wiped:
      begin(rng_.next());
      return {Event::NewGame, -1, 0};
    case Mode::Descending: {
      floor++;
      hp = hp + maxHp / 2 > maxHp ? maxHp : hp + maxHp / 2;
      newFloor();
      return {Event::Descended, -1, 0};
    }
    case Mode::Combat:
      return combat();
    case Mode::Explore:
    default:
      return explore();
  }
}

Outcome Game::explore() {
  if (pendingLevelUp_) {
    pendingLevelUp_ = false;
    snprintf(message, sizeof message, "You reach level %d!", level);
    return {Event::LevelUp, -1, 0};
  }

  tick_++;
  if (tick_ % 2 == 0) moveMonsters();

  const Dir order[4] = {facing, turnLeft(facing), turnRight(facing), turnRight(turnRight(facing))};
  for (Dir d : order) {
    const int m = monsterAt(px + dx(d), py + dy(d));
    if (m < 0) continue;
    if (d != facing) return face(d);
    mode = Mode::Combat;
    foe = m;
    heroTurn_ = true;
    snprintf(message, sizeof message, "A %s blocks the way!", kKinds[monsters[m].kind].name);
    return {Event::Encounter, m, 0};
  }

  return navigate();
}

Outcome Game::face(Dir want) {
  facing = want == turnLeft(facing) ? turnLeft(facing) : turnRight(facing);
  markSeen();
  return {Event::Turned, -1, 0};
}

bool Game::passableForParty(int x, int y) const {
  const Cell c = map.at(x, y);
  return walkable(c) || c == Cell::DoorClosed;
}

bool Game::findPath(bool toStairs, Dir& first) const {
  int8_t from[kMapSize][kMapSize];
  memset(from, -1, sizeof from);
  Point queue[kMapSize * kMapSize];
  int head = 0, tail = 0;
  queue[tail++] = {static_cast<int8_t>(px), static_cast<int8_t>(py)};
  from[py][px] = 4;
  while (head < tail) {
    const Point p = queue[head++];
    const bool goal = (p.x != px || p.y != py) &&
                      (toStairs ? map.cell[p.y][p.x] == Cell::StairsDown : !map.seen[p.y][p.x]);
    if (goal) {
      Point c = p;
      while (true) {
        const Dir d = static_cast<Dir>(from[c.y][c.x]);
        const Point prev = {static_cast<int8_t>(c.x - dx(d)), static_cast<int8_t>(c.y - dy(d))};
        if (prev.x == px && prev.y == py) {
          first = d;
          return true;
        }
        c = prev;
      }
    }
    for (int d = 0; d < 4; d++) {
      const int nx = p.x + dx(static_cast<Dir>(d)), ny = p.y + dy(static_cast<Dir>(d));
      if (!map.inside(nx, ny) || from[ny][nx] >= 0 || !passableForParty(nx, ny)) continue;
      from[ny][nx] = static_cast<int8_t>(d);
      queue[tail++] = {static_cast<int8_t>(nx), static_cast<int8_t>(ny)};
    }
  }
  return false;
}

Outcome Game::navigate() {
  Dir want;
  const bool hurry = stepsOnFloor > kFloorStepLimit;
  if (!(!hurry && findPath(false, want)) && !findPath(true, want)) return face(turnRight(facing));
  if (want != facing) return face(want);

  const int nx = px + dx(facing), ny = py + dy(facing);
  if (map.cell[ny][nx] == Cell::DoorClosed) {
    map.cell[ny][nx] = Cell::DoorOpen;
    markSeen();
    snprintf(message, sizeof message, "You shoulder a door open");
    return {Event::Opened, -1, 0};
  }
  px = nx;
  py = ny;
  stepsOnFloor++;
  markSeen();
  if (map.cell[py][px] == Cell::StairsDown) {
    mode = Mode::Descending;
    snprintf(message, sizeof message, "Stairs lead down...");
  } else if (message[0] && stepsOnFloor % 6 == 0) {
    message[0] = 0;
  }
  return {Event::Moved, -1, 0};
}

void Game::moveMonsters() {
  for (int i = 0; i < monsterCount; i++) {
    Monster& m = monsters[i];
    if (!m.alive || !map.seen[m.y][m.x]) continue;
    const int ddx = px - m.x, ddy = py - m.y;
    const int adx = ddx < 0 ? -ddx : ddx, ady = ddy < 0 ? -ddy : ddy;
    if (adx + ady > 6 || adx + ady <= 1) continue;
    const int sx = ddx > 0 ? 1 : ddx < 0 ? -1 : 0, sy = ddy > 0 ? 1 : ddy < 0 ? -1 : 0;
    const int tries[2][2] = {{adx >= ady ? sx : 0, adx >= ady ? 0 : sy}, {adx >= ady ? 0 : sx, adx >= ady ? sy : 0}};
    for (const auto& t : tries) {
      const int nx = m.x + t[0], ny = m.y + t[1];
      if ((t[0] || t[1]) && map.at(nx, ny) == Cell::Floor && monsterAt(nx, ny) < 0 && (nx != px || ny != py)) {
        m.x = static_cast<int8_t>(nx);
        m.y = static_cast<int8_t>(ny);
        break;
      }
    }
  }
}

Outcome Game::combat() {
  heroTurn_ = !heroTurn_;
  return heroTurn_ ? monsterTurn() : heroTurn();
}

Outcome Game::heroTurn() {
  Monster& m = monsters[foe];
  const MonsterKind& k = kKinds[m.kind];

  if (potions > 0 && hp * 100 < maxHp * 35) {
    potions--;
    const int amount = rng_.between(8, 12) + 2 * level;
    hp = hp + amount > maxHp ? maxHp : hp + amount;
    snprintf(message, sizeof message, "You drink a potion (+%d)", amount);
    return {Event::Potion, foe, amount};
  }

  if (rng_.below(100) < 12) {
    snprintf(message, sizeof message, "You miss the %s", k.name);
    return {Event::HeroMiss, foe, 0};
  }

  const int dmg = rng_.between(3, 8) + level;
  m.hp = static_cast<int16_t>(m.hp - dmg);
  if (m.hp > 0) {
    snprintf(message, sizeof message, "You hit the %s for %d", k.name, dmg);
    return {Event::HeroHit, foe, dmg};
  }

  m.alive = false;
  kills++;
  xp += k.hp + k.tier * 3;
  if (xp >= 25 * level * level) {
    level++;
    pendingLevelUp_ = true;
    maxHp += kHpPerLevel;
    hp += kHpPerLevel;
  }
  mode = Mode::Explore;
  const int slain = foe;
  foe = -1;
  snprintf(message, sizeof message, "You slay the %s!", k.name);
  return {Event::MonsterKilled, slain, dmg};
}

Outcome Game::monsterTurn() {
  const MonsterKind& k = kKinds[monsters[foe].kind];
  const int dmg = rng_.between(k.dmgLo, k.dmgHi) + (floor - 1) / 4;
  hp -= dmg;
  if (hp > 0) {
    snprintf(message, sizeof message, "The %s %s you for %d", k.name, k.verb, dmg);
    return {Event::MonsterAttack, foe, dmg};
  }
  hp = 0;
  mode = Mode::Wiped;
  killer = k.name;
  snprintf(message, sizeof message, "The %s %s you. You die.", k.name, k.verb);
  return {Event::Wiped, foe, dmg};
}

}  // namespace crawl
