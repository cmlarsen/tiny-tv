#include "view.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "gen/art.h"
#include "rle.h"

extern const uint8_t kArtBlob[] asm("_binary_src_gen_art_bin_start");

using nmtv::Canvas;
using nmtv::rgb;

namespace view {

namespace {

constexpr int kStatusY = art::kViewH;
constexpr uint16_t kStatusBg = rgb(22, 18, 26);
constexpr uint16_t kStatusEdge = rgb(92, 74, 52);
constexpr uint16_t kText = rgb(236, 226, 206);
constexpr uint16_t kDim = rgb(150, 136, 118);
constexpr uint16_t kGold = rgb(232, 184, 72);
constexpr uint16_t kBlood = rgb(200, 30, 30);
constexpr uint16_t kHealGlow = rgb(120, 230, 140);
constexpr uint32_t kFlashMs = 220;
constexpr uint32_t kCorpseMs = 700;

struct Splat {
  int8_t set = -1;
  int8_t scale = 1;
  int16_t cx = 0;
  int16_t cy = 0;
  uint32_t start = 0;
};

struct ThemeName {
  const char* id;
  const char* title;
};

const ThemeName kTitles[] = {
    {"stone-dungeon", "Stone Dungeon"}, {"sewer", "The Sewers"},     {"crypt", "The Crypt"},
    {"overgrown-ruins", "Overgrown Ruins"}, {"ice-cave", "Ice Caves"}, {"lava-forge", "Lava Forge"},
    {"mine-shaft", "The Mine Shaft"}, {"castle-library", "Castle Library"}, {"desert-tomb", "Desert Tomb"},
    {"fungal-cavern", "Fungal Cavern"}, {"drowned-temple", "Drowned Temple"}, {"clockwork-vault", "Clockwork Vault"},
};

constexpr uint32_t kSplatMs = 650;
constexpr int kMaxSplats = 4;

struct MonsterAnim {
  int8_t anim = -1;
  uint32_t start = 0;
};

struct Corpse {
  int8_t kind = -1;
  int8_t x = 0;
  int8_t y = 0;
  uint32_t start = 0;
};

uint16_t themePal[256];
uint16_t stainPal[256];
uint16_t splatPal[256];
Splat splats[kMaxSplats];
uint16_t monsterPal[art::kMonsterCount][256];
int8_t artIndexForKind[crawl::kKindCount];
int theme = -1;
MonsterAnim anims[crawl::kMaxMonsters];
Corpse dead;
uint32_t hurtAt = 0;
uint32_t healAt = 0;
Card card = Card::None;
uint32_t cardUntil = 0;
uint32_t cardFrom = 0;
Tomb tomb = {};
int speed = 1;
int deepest = 0;
uint16_t* prevView = nullptr;
constexpr int kChunkRows = 10;
uint16_t* nextChunk[art::kViewH / kChunkRows] = {};
bool nextAlloc = false;
bool nextReady = false;
int viewX = 0;
Transition transition = Transition::None;
uint32_t transStart = 0;
uint32_t transMs = 0;
int heldPermille = -1;
bool fresh = false;

uint16_t swapped(uint16_t c) { return static_cast<uint16_t>((c >> 8) | (c << 8)); }

void loadPalette(uint32_t off, int n, uint16_t* out) {
  const uint16_t* src = reinterpret_cast<const uint16_t*>(kArtBlob + off);
  for (int i = 0; i < n; i++) out[i] = swapped(src[i]);
}

rle::Target target(Canvas& c) {
  return {static_cast<uint16_t*>(c.s.getBuffer()), nmtv::kScreen, c.oy, nmtv::kBandH};
}

void blit(Canvas& c, const art::Sprite& s, const uint16_t* pal) {
  if (s.w == 0) return;
  rle::blit(kArtBlob + s.off, s.w, s.h, s.x + viewX, s.y, 1, pal, target(c));
}

const int kScale[crawl::kViewDepth] = {0, 4, 2, 1};

void drawMonster(Canvas& c, int kind, int anim, int frame, int depth, int col) {
  const int a = artIndexForKind[kind];
  if (a < 0 || depth < 1 || depth >= crawl::kViewDepth) return;
  const int scale = kScale[depth];
  const int size = art::kMonsterSize * scale;
  const int cx = 160 + static_cast<int>(col * art::kFocal / depth) - art::kCropX + viewX;
  const int floorY = art::kViewH / 2 + art::kViewH / 4 / depth;
  const art::Sprite& s = art::kMonsterFrames[a][anim][frame];
  rle::blit(kArtBlob + s.off, s.w, s.h, cx - size / 2, floorY - size + 2 * scale, scale, monsterPal[a], target(c));
}

int frameFor(int anim, uint32_t elapsed) {
  return static_cast<int>(elapsed * art::kAnimFps[anim] / 1000);
}

void drawMonstersAt(Canvas& c, const crawl::Game& g, int depth, uint32_t now) {
  for (int col = -1; col <= 1; col++) {
    int x, y;
    g.viewCell(depth, col, x, y);
    const int m = g.monsterAt(x, y);
    if (m >= 0) {
      int anim = art::Idle;
      int frame = static_cast<int>((now * art::kAnimFps[art::Idle] / 1000 + m) % art::kAnimFrames[art::Idle]);
      if (anims[m].anim >= 0) {
        const int f = frameFor(anims[m].anim, now - anims[m].start);
        if (f < art::kAnimFrames[anims[m].anim]) {
          anim = anims[m].anim;
          frame = f;
        } else {
          anims[m].anim = -1;
        }
      }
      drawMonster(c, g.monsters[m].kind, anim, frame, depth, col);
    } else if (dead.kind >= 0 && dead.x == x && dead.y == y && now - dead.start < kCorpseMs) {
      const int f = frameFor(art::Death, now - dead.start);
      if (f < art::kAnimFrames[art::Death]) drawMonster(c, dead.kind, art::Death, f, depth, col);
    }
  }
}

void drawScene(Canvas& c, const crawl::Game& g, uint32_t now) {
  blit(c, art::kBackground[theme], themePal);
  for (int d = crawl::kViewDepth - 1; d >= 0; d--) {
    for (int col = -3; col <= 3; col++) {
      int x, y;
      g.viewCell(d, col, x, y);
      if (g.map.at(x, y) == crawl::Cell::StairsDown) blit(c, art::kHatch[theme][d][col + 3], themePal);
    }
  }

  int depth = art::kSlots[0].depth;
  for (int i = 0; i < art::kSlotCount; i++) {
    const art::Slot& slot = art::kSlots[i];
    if (slot.depth < depth) {
      drawMonstersAt(c, g, depth, now);
      depth = slot.depth;
    }
    int x, y;
    g.viewCell(slot.depth, slot.col, x, y);
    int set = -1;
    switch (g.map.at(x, y)) {
      case crawl::Cell::Wall: set = art::Wall; break;
      case crawl::Cell::DoorClosed: set = art::DoorClosed; break;
      case crawl::Cell::DoorOpen: set = art::DoorOpen; break;
      case crawl::Cell::StairsUp: set = art::StairsUp; break;
      default: break;
    }
    if (set < 0) continue;
    blit(c, art::kPieces[theme][set][i], themePal);
    if (set != art::Wall) continue;
    switch (g.map.decalAt(x, y)) {
      case crawl::Decal::Torch: blit(c, art::kPieces[theme][art::Torch][i], themePal); break;
      case crawl::Decal::Grate: blit(c, art::kPieces[theme][art::Grate][i], themePal); break;
      case crawl::Decal::Lever: blit(c, art::kPieces[theme][art::Lever][i], themePal); break;
      default: break;
    }
    const int stain = g.map.stainAt(x, y);
    if (stain > 0 && stain <= art::kStainCount) blit(c, art::kStains[stain - 1][i], stainPal);
  }
  drawMonstersAt(c, g, depth, now);
}

int viewRows(const Canvas& c, int& r0) {
  r0 = c.oy;
  const int r1 = c.oy + nmtv::kBandH < art::kViewH ? c.oy + nmtv::kBandH : art::kViewH;
  return r1 > r0 ? r1 - r0 : 0;
}

void saveView(Canvas& c) {
  if (!prevView) return;
  int r0;
  const int rows = viewRows(c, r0);
  memcpy(prevView + r0 * art::kViewW, c.s.getBuffer(), rows * art::kViewW * sizeof(uint16_t));
  if (r0 + rows == art::kViewH) fresh = true;
}

void drawZoom(Canvas& c, int permille) {
  uint16_t* dst = static_cast<uint16_t*>(c.s.getBuffer());
  const int scale1024 = 1024 + permille * 3 / 4;
  const int cx = art::kViewW / 2, cy = art::kViewH / 2;
  int r0;
  const int rows = viewRows(c, r0);
  for (int r = 0; r < rows; r++) {
    const int sy = cy + ((r0 + r - cy) * 1024) / scale1024;
    const uint16_t* src = prevView + sy * art::kViewW;
    uint16_t* out = dst + r * nmtv::kScreen;
    for (int x = 0; x < art::kViewW; x++) out[x] = src[cx + ((x - cx) * 1024) / scale1024];
  }
}

uint16_t* nextRow(int y) { return nextChunk[y / kChunkRows] + (y % kChunkRows) * art::kViewW; }

void copyWords(uint16_t* dst, const uint16_t* src, int pixels) {
  volatile uint32_t* d = reinterpret_cast<volatile uint32_t*>(dst);
  const volatile uint32_t* s = reinterpret_cast<const volatile uint32_t*>(src);
  for (int i = 0; i < pixels / 2; i++) d[i] = s[i];
}

int eased(int permille) { return permille * permille * (3000 - 2 * permille) / 1000000; }

void drawSlide(Canvas& c, const crawl::Game& g, uint32_t now, int permille, bool right) {
  const int shift = art::kViewW * eased(permille) / 1000 & ~1;
  uint16_t* dst = static_cast<uint16_t*>(c.s.getBuffer());
  int r0;
  const int rows = viewRows(c, r0);
  if (!nextReady) {
    viewX = right ? art::kViewW - shift : shift - art::kViewW;
    drawScene(c, g, now);
    viewX = 0;
  }
  const int keep = art::kViewW - shift;
  for (int r = 0; r < rows; r++) {
    const uint16_t* src = prevView + (r0 + r) * art::kViewW;
    const uint16_t* next = nextReady ? nextRow(r0 + r) : nullptr;
    uint16_t* out = dst + r * nmtv::kScreen;
    if (right) {
      memcpy(out, src + shift, keep * sizeof(uint16_t));
      if (next) copyWords(out + keep, next, shift);
    } else {
      memcpy(out + shift, src, keep * sizeof(uint16_t));
      if (next) copyWords(out, next + keep, shift);
    }
  }
}

void drawSplats(Canvas& c, uint32_t now) {
  for (Splat& sp : splats) {
    if (sp.set < 0) continue;
    const uint32_t elapsed = now - sp.start;
    if (elapsed >= kSplatMs) {
      sp.set = -1;
      continue;
    }
    const art::Sprite& s = art::kSplats[sp.set][elapsed * art::kSplatFrames / kSplatMs];
    if (s.w == 0) continue;
    rle::blit(kArtBlob + s.off, s.w, s.h, sp.cx - s.w * sp.scale / 2, sp.cy - s.h * sp.scale / 2, sp.scale, splatPal,
              target(c));
  }
}

void drawView(Canvas& c, const crawl::Game& g, uint32_t now) {
  int permille = -1;
  if (heldPermille >= 0) permille = heldPermille;
  else if (transition != Transition::None && now - transStart < transMs) permille = (now - transStart) * 1000 / transMs;
  else transition = Transition::None;

  if (permille < 0 || !prevView) {
    drawScene(c, g, now);
    saveView(c);
  } else if (transition == Transition::Forward) {
    drawZoom(c, permille);
  } else {
    drawSlide(c, g, now, permille, transition == Transition::TurnRight);
  }
  drawSplats(c, now);

  static const char kCompass[] = "NESW";
  const char dir[2] = {kCompass[g.facing], 0};
  c.rect(art::kViewW - 22, 4, 18, 18, rgb(0, 0, 0));
  c.s.drawRect(art::kViewW - 22, 4 - c.oy, 18, 18, kStatusEdge);
  c.font(&fonts::Font2);
  c.text(dir, art::kViewW - 13, 13, kGold, middle_center);

  char deep[24];
  snprintf(deep, sizeof deep, "Deepest: floor %d", deepest > g.floor ? deepest : g.floor);
  const int dw = c.s.textWidth(deep) + 12;
  c.roundRect(art::kViewW / 2 - dw / 2, 4, dw, 18, 4, rgb(0, 0, 0));
  c.text(deep, art::kViewW / 2, 13, kGold, middle_center);

  if (speed > 1) {
    char sp[4];
    snprintf(sp, sizeof sp, "%dx", speed);
    c.text(sp, 6, 13, kGold, middle_left);
  }

  if (now - hurtAt < kFlashMs) {
    for (int i = 0; i < 3; i++) c.s.drawRect(i, i - c.oy, art::kViewW - 2 * i, art::kViewH - 2 * i, kBlood);
  }
}

uint16_t hpColor(int hp, int maxHp) {
  if (hp * 3 > maxHp * 2) return rgb(84, 196, 92);
  if (hp * 3 > maxHp) return rgb(226, 190, 60);
  return rgb(214, 64, 52);
}

void drawStatus(Canvas& c, const crawl::Game& g, uint32_t now) {
  c.rect(0, kStatusY, nmtv::kScreen, nmtv::kScreen - kStatusY, kStatusBg);
  c.hline(0, kStatusY, nmtv::kScreen, kStatusEdge);

  char line[72];
  const char* msg = g.message;
  if (!msg[0]) {
    snprintf(line, sizeof line, "%s - Floor %d", themeTitle(g.floor), g.floor);
    msg = line;
  }
  c.font(&fonts::FreeSansBold9pt7b);
  if (c.width(msg) > nmtv::kScreen - 8) c.font(&fonts::Font2);
  c.text(msg, nmtv::kScreen / 2, kStatusY + 12, g.message[0] ? kText : kDim, middle_center);

  c.font(&fonts::Font2);
  const int y = kStatusY + 28;
  char lv[8];
  snprintf(lv, sizeof lv, "Lv%d", g.level);
  c.text(lv, 6, y, kGold, middle_left);
  const int bx = 38, bw = 118;
  c.rect(bx, y - 4, bw, 8, rgb(48, 40, 44));
  uint16_t col = hpColor(g.hp, g.maxHp);
  if (now - hurtAt < kFlashMs) col = rgb(255, 255, 255);
  else if (now - healAt < kFlashMs * 3) col = kHealGlow;
  if (g.hp > 0) c.rect(bx, y - 4, bw * g.hp / g.maxHp, 8, col);
  char hp[12];
  snprintf(hp, sizeof hp, "%d/%d", g.hp, g.maxHp);
  c.text(hp, bx + bw + 4, y, kText, middle_left);
  for (int i = 0; i < g.potions && i < 4; i++) {
    const int px = 231 - i * 11;
    c.rect(px - 1, y - 6, 3, 3, rgb(170, 160, 150));
    c.circle(px, y + 1, 4, rgb(196, 40, 60));
  }
}

void drawFloorCard(Canvas& c, const crawl::Game& g) {
  c.s.fillScreen(rgb(8, 6, 10));
  char line[24];
  snprintf(line, sizeof line, "FLOOR %d", g.floor);
  c.font(&fonts::FreeSerifBold18pt7b);
  c.text(line, 120, 100, kGold, middle_center);
  c.font(&fonts::FreeSerifItalic12pt7b);
  c.text(themeTitle(g.floor), 120, 140, kText, middle_center);
}

void drawTomb(Canvas& c) {
  c.s.fillScreen(rgb(10, 12, 20));
  c.rect(0, 206, 240, 34, rgb(28, 40, 26));
  const uint16_t stone = rgb(120, 118, 124);
  c.circle(120, 74, 62, stone);
  c.rect(58, 74, 125, 136, stone);
  c.rect(52, 206, 137, 8, rgb(90, 88, 94));
  c.font(&fonts::FreeSerifBold18pt7b);
  c.text("R.I.P.", 120, 52, rgb(40, 38, 44), middle_center);
  c.font(&fonts::Font2);
  char line[48];
  c.text("Slain by a", 120, 84, rgb(30, 28, 34), middle_center);
  c.text(tomb.killer, 120, 102, rgb(30, 28, 34), middle_center);
  snprintf(line, sizeof line, "on floor %d", tomb.floor);
  c.text(line, 120, 120, rgb(30, 28, 34), middle_center);
  snprintf(line, sizeof line, "Level %d, %d kills", tomb.level, tomb.kills);
  c.text(line, 120, 148, rgb(52, 50, 58), middle_center);
  snprintf(line, sizeof line, "Best: floor %d", tomb.best);
  c.text(line, 120, 176, rgb(110, 30, 30), middle_center);
}

}  // namespace

void begin() {
  prevView = static_cast<uint16_t*>(malloc(art::kViewW * art::kViewH * sizeof(uint16_t)));
  if (!prevView) Serial.println("[view] no memory for transitions");
  nextAlloc = true;
  for (uint16_t*& chunk : nextChunk) {
    const size_t bytes = art::kViewW * kChunkRows * sizeof(uint16_t);
    chunk = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_EXEC));
    if (!chunk) chunk = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_32BIT));
    nextAlloc = nextAlloc && chunk;
  }
  if (!nextAlloc) Serial.println("[view] no memory for smooth turns");
  for (int k = 0; k < crawl::kKindCount; k++) {
    artIndexForKind[k] = -1;
    for (int a = 0; a < art::kMonsterCount; a++)
      if (strcmp(crawl::kKinds[k].id, art::kMonsterIds[a]) == 0) artIndexForKind[k] = static_cast<int8_t>(a);
  }
  loadPalette(art::kStainPalette, art::kStainPaletteSize, stainPal);
  loadPalette(art::kSplatPalette, art::kSplatPaletteSize, splatPal);
  for (int a = 0; a < art::kMonsterCount; a++) loadPalette(art::kMonsterPalette[a], art::kMonsterPaletteSize[a], monsterPal[a]);
  setFloor(1);
}

void setFloor(int floor) {
  const int t = (floor - 1) % art::kThemeCount;
  for (auto& a : anims) a.anim = -1;
  for (auto& sp : splats) sp.set = -1;
  dead.kind = -1;
  if (t == theme) return;
  theme = t;
  loadPalette(art::kPalette[t], art::kPaletteSize[t], themePal);
}

const char* themeTitle(int floor) {
  const char* id = art::kThemeIds[(floor - 1) % art::kThemeCount];
  for (const ThemeName& n : kTitles)
    if (strcmp(n.id, id) == 0) return n.title;
  return id;
}

void playMonster(int monster, int anim, uint32_t now) {
  if (monster < 0 || monster >= crawl::kMaxMonsters) return;
  anims[monster].anim = static_cast<int8_t>(anim);
  anims[monster].start = now;
}

void corpse(const crawl::Monster& m, uint32_t now) {
  dead.kind = static_cast<int8_t>(m.kind);
  dead.x = m.x;
  dead.y = m.y;
  dead.start = now;
}

void hurt(uint32_t now) { hurtAt = now; }

void splat(SplatKind kind, uint32_t now) {
  Splat* slot = &splats[0];
  for (Splat& sp : splats) {
    if (sp.set < 0) {
      slot = &sp;
      break;
    }
    if (sp.start < slot->start) slot = &sp;
  }
  slot->set = static_cast<int8_t>(esp_random() % art::kSplatSets);
  slot->start = now;
  const int jitter = static_cast<int>(esp_random() % 41) - 20;
  const int monsterY = art::kViewH / 2 + art::kViewH / 4 - art::kMonsterSize * 2;
  switch (kind) {
    case SplatKind::MonsterHit:
      slot->scale = 3;
      slot->cx = static_cast<int16_t>(art::kViewW / 2 + jitter);
      slot->cy = static_cast<int16_t>(monsterY + jitter / 2);
      break;
    case SplatKind::MonsterDeath:
      slot->scale = 4;
      slot->cx = art::kViewW / 2;
      slot->cy = static_cast<int16_t>(monsterY + 10);
      break;
    case SplatKind::HeroHurt:
      slot->scale = 6;
      slot->cx = static_cast<int16_t>(art::kViewW / 2 + jitter * 2);
      slot->cy = static_cast<int16_t>(art::kViewH / 2 + jitter);
      break;
  }
}

void heal(uint32_t now) { healAt = now; }

void showCard(Card c, uint32_t until, uint32_t from) {
  card = c;
  cardUntil = until;
  cardFrom = from;
}

void setTomb(const Tomb& t) { tomb = t; }

void setSpeed(int s) { speed = s; }

void setDeepest(int floor) { deepest = floor; }

void startTransition(Transition t, uint32_t now, uint32_t ms) {
  transition = ms >= 120 ? t : Transition::None;
  transStart = now;
  transMs = ms;
  fresh = false;
  nextReady = false;
}

void captureNext(LGFX_Sprite& band, const crawl::Game& g, uint32_t now) {
  if (!nextAlloc) return;
  for (int oy = 0; oy < art::kViewH; oy += nmtv::kBandH) {
    Canvas c{band, oy};
    drawScene(c, g, now);
    int r0;
    const int rows = viewRows(c, r0);
    for (int r = 0; r < rows; r++)
      copyWords(nextRow(r0 + r), static_cast<uint16_t*>(band.getBuffer()) + r * nmtv::kScreen, art::kViewW);
  }
  nextReady = true;
}

bool freshView() { return fresh; }

bool animating(uint32_t now) { return transition != Transition::None && now - transStart < transMs; }

void holdTransition(Transition t, int permille) {
  transition = t;
  heldPermille = permille;
  nextReady = false;
  if (t == Transition::None) heldPermille = -1;
}

void draw(Canvas& c, const crawl::Game& g, uint32_t now) {
  if (card != Card::None && static_cast<int32_t>(cardUntil - now) > 0) {
    if (cardFrom == 0 || static_cast<int32_t>(now - cardFrom) >= 0) {
      if (card == Card::Floor) drawFloorCard(c, g);
      else drawTomb(c);
      return;
    }
  } else {
    card = Card::None;
  }
  drawView(c, g, now);
  drawStatus(c, g, now);
}

}  // namespace view
