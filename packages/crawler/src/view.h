#pragma once

#include <Arduino.h>

#include "canvas.h"
#include "crawl.h"

namespace view {

enum class Card : uint8_t { None, Floor, Tomb };
enum class SplatKind : uint8_t { MonsterHit, MonsterDeath, HeroHurt };
enum class Transition : uint8_t { None, Forward, TurnLeft, TurnRight };

struct Tomb {
  const char* killer;
  int floor;
  int level;
  int kills;
  int best;
};

void begin();
void setFloor(int floor);
const char* themeTitle(int floor);

void playMonster(int monster, int anim, uint32_t now);
void corpse(const crawl::Monster& m, uint32_t now);
void hurt(uint32_t now);
void splat(SplatKind kind, uint32_t now);
void heal(uint32_t now);

void showCard(Card card, uint32_t until, uint32_t from = 0);
void setTomb(const Tomb& t);
void setSpeed(int speed);
void setDeepest(int floor);
void startTransition(Transition t, uint32_t now, uint32_t ms);
void holdTransition(Transition t, int permille);
void captureNext(LGFX_Sprite& band, const crawl::Game& g, uint32_t now);
bool freshView();
bool animating(uint32_t now);

void draw(nmtv::Canvas& c, const crawl::Game& g, uint32_t now);

}  // namespace view
