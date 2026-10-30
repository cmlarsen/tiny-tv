#include <string.h>

#include "modes.h"

namespace ca {

namespace {

struct LifeRule {
  const char* name;
  uint32_t birth;
  uint32_t survive;
};

const LifeRule kRules[] = {
    {"Conway", 1u << 3, (1u << 2) | (1u << 3)},
    {"HighLife", (1u << 3) | (1u << 6), (1u << 2) | (1u << 3)},
    {"Day & Night", (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8),
     (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 8)},
};

const Color kHues[Life::kSpecies] = {{255, 84, 96}, {255, 196, 60}, {40, 225, 190}, {150, 110, 255}};
constexpr Color kBg = {6, 6, 14};
constexpr Color kWhite = {255, 250, 240};
constexpr int kFade = 14;

}  // namespace

Life::Life() : rng_(1) {
  for (int s = 0; s < kSpecies; s++) {
    for (int a = 0; a < 8; a++) live_[s][a] = px(lerp(kWhite, kHues[s], a * 255 / 7));
    const Color dim = lerp(kBg, kHues[s], 110);
    for (int t = 0; t < 16; t++) fade_[s][t] = px(lerp(kBg, dim, t * t * 255 / 225));
  }
  clear();
}

const char* Life::variantName() const { return kRules[variant_].name; }

void Life::start(uint32_t seed, int variant) {
  rng_ = Rng(seed);
  variant_ = variant;
  birthMask_ = kRules[variant].birth;
  surviveMask_ = kRules[variant].survive;
  clear();
  const int density = variant == 2 ? 50 : 30;
  for (int i = 0; i < kN * kN; i++)
    if (rng_.chance(density)) {
      const int sp = (i % kN) * kSpecies / kN;
      cell_[i] = static_cast<uint8_t>(sp + 1);
      hue_[i] = static_cast<uint8_t>(sp);
    }
}

void Life::clear() {
  memset(cell_, 0, sizeof cell_);
  memset(age_, 0, sizeof age_);
  memset(hue_, 0, sizeof hue_);
  births_ = deaths_ = quiet_ = 0;
  frame_ = 0;
}

void Life::set(int x, int y, int species) {
  const int i = y * kN + x;
  cell_[i] = static_cast<uint8_t>(species + 1);
  hue_[i] = static_cast<uint8_t>(species);
  age_[i] = 0;
}

int Life::population() const {
  int n = 0;
  for (int i = 0; i < kN * kN; i++) n += cell_[i] != 0;
  return n;
}

void Life::step() {
  births_ = deaths_ = 0;
  for (int y = 0; y < kN; y++) {
    const uint8_t* rows[3] = {cell_ + ((y + kN - 1) % kN) * kN, cell_ + y * kN, cell_ + ((y + 1) % kN) * kN};
    for (int x = 0; x < kN; x++) {
      const int xs[3] = {x == 0 ? kN - 1 : x - 1, x, x == kN - 1 ? 0 : x + 1};
      int n = 0;
      uint8_t counts[kSpecies + 1] = {0, 0, 0, 0, 0};
      for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
          if (r == 1 && c == 1) continue;
          const uint8_t v = rows[r][xs[c]];
          counts[v]++;
          n += v != 0;
        }
      const int i = y * kN + x;
      if (cell_[i]) {
        if (surviveMask_ >> n & 1) {
          next_[i] = cell_[i];
          if (age_[i] < 255) age_[i]++;
        } else {
          next_[i] = 0;
          age_[i] = kTrail;
          deaths_++;
        }
      } else if (birthMask_ >> n & 1) {
        int best = 1, present = 0;
        for (int s = 1; s <= kSpecies; s++) {
          present += counts[s] != 0;
          if (counts[s] > counts[best] || (counts[s] == counts[best] && rng_.chance(50))) best = s;
        }
        if (present == kSpecies - 1 && counts[best] * (kSpecies - 1) == n)
          for (int s = 1; s <= kSpecies; s++)
            if (!counts[s]) best = s;
        next_[i] = static_cast<uint8_t>(best);
        hue_[i] = static_cast<uint8_t>(best - 1);
        age_[i] = 0;
        births_++;
      } else {
        next_[i] = 0;
        age_[i] = age_[i] > kFade ? age_[i] - kFade : 0;
      }
    }
  }
  memcpy(cell_, next_, sizeof cell_);
}

void Life::splash() {
  const int cx = rng_.below(kN), cy = rng_.below(kN);
  const int r = 6 + rng_.below(6);
  const int a = rng_.below(kSpecies), b = rng_.below(kSpecies);
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy > r * r || !rng_.chance(40)) continue;
      set((cx + dx + kN) % kN, (cy + dy + kN) % kN, dx < 0 ? a : b);
    }
}

void Life::tick() {
  if (frame_++ & 1) return;
  step();
  quiet_ = activity() < kN * kN / 120 ? quiet_ + 1 : 0;
  if (quiet_ > 40 || rng_.below(500) == 0) {
    splash();
    quiet_ = 0;
  }
}

void Life::renderRow(int y, uint16_t* out) const {
  const int cy = y / 3;
  const bool edge = y % 3 == 2;
  for (int x = 0; x < kN; x++) {
    const int i = cy * kN + x;
    uint16_t c, gap;
    if (cell_[i]) {
      const int a = age_[i] < 7 ? age_[i] : 7;
      c = live_[hue_[i]][a];
      gap = fade_[hue_[i]][15];
    } else {
      c = gap = fade_[hue_[i]][age_[i] >> 4];
    }
    *out++ = edge ? gap : c;
    *out++ = edge ? gap : c;
    *out++ = gap;
  }
}

}  // namespace ca
