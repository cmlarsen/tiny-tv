#include <string.h>

#include "modes.h"

namespace ca {

namespace {

const char* const kVariants[] = {"Soup", "Mandala"};
constexpr uint8_t kGlowOn = 255;
constexpr uint8_t kGlowDying = 200;
constexpr int kDecay = 10;

}  // namespace

Brain::Brain() : rng_(1) {
  const Color stops[] = {{4, 4, 12}, {20, 10, 70}, {40, 40, 200}, {60, 140, 255}};
  gradient(stops, 4, pal_);
  on_ = px({235, 250, 255});
  dying_ = px({90, 150, 255});
  clear();
}

const char* Brain::variantName() const { return kVariants[variant_]; }

void Brain::clear() {
  memset(cell_, 0, sizeof cell_);
  memset(glow_, 0, sizeof glow_);
  frame_ = 0;
}

void Brain::start(uint32_t seed, int variant) {
  rng_ = Rng(seed);
  variant_ = variant;
  clear();
  if (variant == 0) {
    for (int i = 0; i < kN * kN; i++)
      if (rng_.chance(18)) cell_[i] = On;
    return;
  }
  const int r = 6 + rng_.below(6);
  for (int y = kN / 2 - r; y < kN / 2; y++)
    for (int x = kN / 2 - r; x < kN / 2; x++) {
      if (!rng_.chance(40)) continue;
      const int mx = kN - 1 - x, my = kN - 1 - y;
      cell_[y * kN + x] = cell_[y * kN + mx] = cell_[my * kN + x] = cell_[my * kN + mx] = On;
    }
}

int Brain::population() const {
  int n = 0;
  for (int i = 0; i < kN * kN; i++) n += cell_[i] != Off;
  return n;
}

void Brain::step() {
  for (int y = 0; y < kN; y++) {
    const uint8_t* rows[3] = {cell_ + ((y + kN - 1) % kN) * kN, cell_ + y * kN, cell_ + ((y + 1) % kN) * kN};
    for (int x = 0; x < kN; x++) {
      const int i = y * kN + x;
      const uint8_t s = cell_[i];
      if (s == On) {
        next_[i] = Dying;
        glow_[i] = kGlowDying;
        continue;
      }
      if (s == Dying) {
        next_[i] = Off;
        continue;
      }
      const int xl = x == 0 ? kN - 1 : x - 1, xr = x == kN - 1 ? 0 : x + 1;
      int n = 0;
      for (int r = 0; r < 3; r++) n += (rows[r][xl] == On) + (rows[r][x] == On) + (rows[r][xr] == On);
      if (n == 2) {
        next_[i] = On;
        glow_[i] = kGlowOn;
      } else {
        next_[i] = Off;
        glow_[i] = glow_[i] > kDecay ? glow_[i] - kDecay : 0;
      }
    }
  }
  memcpy(cell_, next_, sizeof cell_);
}

void Brain::tick() {
  if (frame_++ & 1) return;
  step();
  if (population() < kN * kN / 200) start(rng_.next(), variant_);
}

void Brain::renderRow(int y, uint16_t* out) const {
  const uint8_t* row = cell_ + (y / 2) * kN;
  const uint8_t* glow = glow_ + (y / 2) * kN;
  for (int x = 0; x < kN; x++) {
    const uint16_t c = row[x] == On ? on_ : row[x] == Dying ? dying_ : pal_[glow[x]];
    *out++ = c;
    *out++ = c;
  }
}

}  // namespace ca
