#include <Arduino.h>
#include <Preferences.h>

#include "board.h"
#include "canvas.h"
#include "crawl.h"
#include "display.h"
#include "gen/art.h"
#include "touch.h"
#include "view.h"

namespace {

constexpr uint32_t kFrameMs = 80;
constexpr uint32_t kFloorCardMs = 2400;
constexpr uint32_t kTombMs = 8000;
constexpr uint32_t kMoveAnimMs = 345;
constexpr uint32_t kTurnAnimMs = 320;
constexpr uint8_t kBrightness = 150;
constexpr int kSpeeds[] = {1, 2, 4};

LGFX lcd;
LGFX_Sprite band(&lcd);
TouchPad touch;
Preferences prefs;
crawl::Game game;

int speedIdx = 0;
bool paused = false;
uint32_t nextTickMs = 0;
int best = 0;

uint32_t delayFor(crawl::Event e) {
  switch (e) {
    case crawl::Event::Turned: return 375;
    case crawl::Event::Moved: return 535;
    case crawl::Event::Opened: return 870;
    case crawl::Event::Encounter: return 1100;
    case crawl::Event::HeroHit:
    case crawl::Event::HeroMiss:
    case crawl::Event::Potion:
    case crawl::Event::MonsterAttack: return 850;
    case crawl::Event::MonsterKilled: return 1100;
    case crawl::Event::LevelUp: return 1600;
    default: return 400;
  }
}

void saveBest() {
  if (game.floor <= best) return;
  best = game.floor;
  prefs.putInt("best", best);
  view::setDeepest(best);
}

uint32_t apply(const crawl::Outcome& o, uint32_t now) {
  const int speed = kSpeeds[speedIdx];
  switch (o.event) {
    case crawl::Event::HeroHit:
      view::playMonster(o.monster, art::Hurt, now);
      view::splat(view::SplatKind::MonsterHit, now);
      break;
    case crawl::Event::Encounter:
      view::playMonster(o.monster, art::Move, now);
      break;
    case crawl::Event::MonsterKilled:
      view::corpse(game.monsters[o.monster], now);
      view::splat(view::SplatKind::MonsterDeath, now);
      break;
    case crawl::Event::MonsterAttack:
      view::playMonster(o.monster, art::Attack, now);
      view::hurt(now);
      view::splat(view::SplatKind::HeroHurt, now);
      break;
    case crawl::Event::Potion:
      view::heal(now);
      break;
    case crawl::Event::Descended:
      saveBest();
      view::setFloor(game.floor);
      view::showCard(view::Card::Floor, now + kFloorCardMs / speed);
      return kFloorCardMs / speed;
    case crawl::Event::Wiped: {
      view::playMonster(o.monster, art::Attack, now);
      view::hurt(now);
      view::splat(view::SplatKind::HeroHurt, now);
      saveBest();
      view::setTomb({game.killer, game.floor, game.level, game.kills, best});
      const uint32_t beat = 1500 / speed;
      view::showCard(view::Card::Tomb, now + beat + kTombMs, now + beat);
      return beat + kTombMs;
    }
    case crawl::Event::NewGame:
      view::setFloor(game.floor);
      view::showCard(view::Card::Floor, now + kFloorCardMs);
      return kFloorCardMs;
    default:
      break;
  }
  return delayFor(o.event) / speed;
}

uint32_t tick(uint32_t now) {
  const crawl::Dir before = game.facing;
  const crawl::Outcome o = game.step();
  const uint32_t wait = apply(o, now);
  if (o.event == crawl::Event::Moved) {
    view::startTransition(view::Transition::Forward, now, std::min<uint32_t>(kMoveAnimMs, wait * 2 / 3));
  } else if (o.event == crawl::Event::Turned) {
    const bool right = game.facing == crawl::turnRight(before);
    view::startTransition(right ? view::Transition::TurnRight : view::Transition::TurnLeft, now,
                          std::min<uint32_t>(kTurnAnimMs, wait * 7 / 8));
    view::captureNext(band, game, now);
  }
  return wait;
}

void newGame(uint32_t seed, uint32_t now) {
  game.themeCount = art::kThemeCount;
  game.stainKinds = art::kStainCount;
  game.begin(seed);
  view::setFloor(game.floor);
  view::showCard(view::Card::Floor, now + kFloorCardMs);
  nextTickMs = now + kFloorCardMs;
}

void render(Print* dump = nullptr) {
  const uint32_t now = millis();
  nmtv::renderBands(band, [now](nmtv::Canvas& c) { view::draw(c, game, now); }, dump);
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  const int sp = line.indexOf(' ');
  const String cmd = sp < 0 ? line : line.substring(0, sp);
  const long arg = sp < 0 ? 0 : line.substring(sp + 1).toInt();
  const uint32_t now = millis();
  if (cmd == "shot") {
    Serial.print("\nSHOT\n");
    render(&Serial);
    Serial.print("\nENDSHOT\n");
  } else if (cmd == "perf") {
    const uint32_t t0 = micros();
    render();
    Serial.printf("render %lu us, heap %u, largest %u\n", (unsigned long)(micros() - t0), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  } else if (cmd == "pause") {
    paused = true;
  } else if (cmd == "resume") {
    paused = false;
  } else if (cmd == "seed") {
    newGame(static_cast<uint32_t>(arg), now);
    view::showCard(view::Card::None, now);
  } else if (cmd == "floor") {
    game.jumpToFloor(static_cast<int>(arg));
    view::setFloor(game.floor);
    view::showCard(view::Card::None, now);
    int stained = 0;
    for (int y = 0; y < crawl::kMapSize; y++)
      for (int x = 0; x < crawl::kMapSize; x++) stained += game.map.stain[y][x] != 0;
    Serial.printf("floor %d: %d%% stain chance, %d stained walls\n", game.floor, game.stainPercent(), stained);
  } else if (cmd == "spawn") {
    Serial.printf("spawned %d\n", game.spawnAhead(static_cast<int>(arg)));
  } else if (cmd == "step") {
    view::showCard(view::Card::None, now);
    for (long i = 0; i < (arg > 0 ? arg : 1); i++) {
      render();
      tick(now);
    }
  } else if (cmd == "card") {
    view::showCard(view::Card::Floor, now + 60000);
  } else if (cmd == "tomb") {
    view::setTomb({"Stone Golem", game.floor, game.level, game.kills, best});
    view::showCard(view::Card::Tomb, now + 60000);
  } else if (cmd == "anim") {
    const int kind = static_cast<int>(arg / 1000);
    view::holdTransition(static_cast<view::Transition>(kind), static_cast<int>(arg % 1000));
    if (kind >= 2) view::captureNext(band, game, now);
  } else if (cmd == "splat") {
    view::splat(static_cast<view::SplatKind>(arg % 3), now);
  } else if (cmd == "best") {
    best = static_cast<int>(arg);
    prefs.putInt("best", best);
    view::setDeepest(best);
  } else if (cmd == "speed") {
    speedIdx = arg >= 4 ? 2 : arg >= 2 ? 1 : 0;
    view::setSpeed(kSpeeds[speedIdx]);
  }
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  nmtv::beginDisplay(lcd);
  lcd.setBrightness(kBrightness);
  band.setColorDepth(16);
  if (!band.createSprite(nmtv::kScreen, nmtv::kBandH)) Serial.println("[main] sprite alloc failed");
  touch.begin(board::kTouch);
  prefs.begin("crawler");
  best = prefs.getInt("best", 0);
  view::setDeepest(best);
  view::begin();
  newGame(esp_random(), millis());
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();

  handleSerial();

  switch (touch.poll(now)) {
    case TouchPad::Event::Tap:
      speedIdx = (speedIdx + 1) % 3;
      view::setSpeed(kSpeeds[speedIdx]);
      break;
    case TouchPad::Event::Hold:
      newGame(esp_random(), now);
      break;
    default:
      break;
  }

  if (!paused && static_cast<int32_t>(now - nextTickMs) >= 0) {
    if (!view::freshView()) render();
    nextTickMs = now + tick(now);
  }

  if (now - lastFrameMs >= kFrameMs || view::animating(now)) {
    lastFrameMs = now;
    render();
  }
  delay(2);
}
