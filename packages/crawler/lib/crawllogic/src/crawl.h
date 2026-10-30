#pragma once

#include <stddef.h>
#include <stdint.h>

namespace crawl {

constexpr int kMapSize = 17;
constexpr int kMaxMonsters = 16;
constexpr int kViewDepth = 4;
constexpr int kFloorStepLimit = 400;

enum class Cell : uint8_t { Wall, Floor, DoorClosed, DoorOpen, StairsUp, StairsDown };
enum class Decal : uint8_t { None, Torch, Grate, Lever };
enum Dir : uint8_t { North, East, South, West };

enum class Mode : uint8_t { Explore, Combat, Descending, Wiped };

enum class Event : uint8_t {
  None,
  Turned,
  Moved,
  Opened,
  Encounter,
  HeroHit,
  HeroMiss,
  Potion,
  MonsterKilled,
  MonsterAttack,
  LevelUp,
  Descended,
  Wiped,
  NewGame,
};

struct Rng {
  uint32_t s = 1;
  void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
  int between(int lo, int hi) { return lo + below(hi - lo + 1); }
};

bool walkable(Cell c);
bool blocksView(Cell c);
Dir turnRight(Dir d);
Dir turnLeft(Dir d);
int dx(Dir d);
int dy(Dir d);

struct Map {
  Cell cell[kMapSize][kMapSize];
  Decal decal[kMapSize][kMapSize];
  uint8_t stain[kMapSize][kMapSize];
  bool seen[kMapSize][kMapSize];

  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < kMapSize && y < kMapSize; }
  Cell at(int x, int y) const { return inside(x, y) ? cell[y][x] : Cell::Wall; }
  Decal decalAt(int x, int y) const { return inside(x, y) ? decal[y][x] : Decal::None; }
  uint8_t stainAt(int x, int y) const { return inside(x, y) ? stain[y][x] : 0; }
};

struct MonsterKind {
  const char* id;
  const char* name;
  const char* verb;
  int hp;
  int dmgLo;
  int dmgHi;
  int tier;
};

constexpr int kKindCount = 10;
extern const MonsterKind kKinds[kKindCount];

struct Monster {
  uint8_t kind;
  int8_t x;
  int8_t y;
  int16_t hp;
  bool alive;
};

struct Outcome {
  Event event;
  int monster;
  int amount;
};

class Game {
 public:
  void begin(uint32_t seed);
  Outcome step();

  void viewCell(int depth, int col, int& x, int& y) const;
  int monsterAt(int x, int y) const;
  void jumpToFloor(int n);
  int stainPercent() const;
  int spawnAhead(int kind);

  int themeCount = 12;
  int stainKinds = 3;
  Map map;
  int px = 0;
  int py = 0;
  Dir facing = North;
  int floor = 1;
  int level = 1;
  int xp = 0;
  int kills = 0;
  int stepsOnFloor = 0;
  int potions = 0;
  int hp = 0;
  int maxHp = 0;
  Monster monsters[kMaxMonsters];
  int monsterCount = 0;
  Mode mode = Mode::Explore;
  int foe = -1;
  const char* killer = "";
  char message[64] = "";

 private:
  void newFloor();
  void generate();
  void placeMonsters();
  void markSeen();
  Outcome explore();
  Outcome combat();
  Outcome heroTurn();
  Outcome monsterTurn();
  Outcome navigate();
  Outcome face(Dir want);
  void moveMonsters();
  bool findPath(bool toStairs, Dir& first) const;
  bool passableForParty(int x, int y) const;

  Rng rng_;
  bool heroTurn_ = true;
  int tick_ = 0;
  bool pendingLevelUp_ = false;
};

}  // namespace crawl
