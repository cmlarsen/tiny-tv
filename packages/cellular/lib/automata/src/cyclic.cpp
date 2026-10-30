#include <string.h>

#include "modes.h"

namespace ca {

namespace {

const Cyclic::Rule kRules[] = {
    {"Demons", 1, 1, 14, false},
    {"Perfect Spirals", 1, 3, 4, true},
    {"313", 1, 3, 3, true},
    {"Lava Lamp", 2, 10, 3, true},
};

}  // namespace

Cyclic::Cyclic() : rng_(1) {
  setRule(kRules[0]);
  fill(0);
}

int Cyclic::variants() const { return sizeof kRules / sizeof kRules[0]; }

void Cyclic::setRule(const Rule& r) {
  rule_ = r;
  offsets_ = 0;
  for (int dy = -r.range; dy <= r.range; dy++)
    for (int dx = -r.range; dx <= r.range; dx++) {
      if (!dx && !dy) continue;
      if (!r.moore && (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > r.range) continue;
      dx_[offsets_] = static_cast<int8_t>(dx);
      dy_[offsets_] = static_cast<int8_t>(dy);
      offsets_++;
    }
  const int base = rng_.below(256);
  for (int s = 0; s < r.states; s++) pal_[s] = px(hsv(base + s * 256 / r.states, 210, 255));
}

void Cyclic::fill(uint8_t s) { memset(cell_, s, sizeof cell_); }

void Cyclic::start(uint32_t seed, int variant) {
  rng_ = Rng(seed);
  variant_ = variant;
  setRule(kRules[variant]);
  for (int i = 0; i < kN * kN; i++) cell_[i] = static_cast<uint8_t>(rng_.below(rule_.states));
}

int Cyclic::step() {
  int xs[kN + 6];
  for (int i = 0; i < kN + 6; i++) xs[i] = (i - 3 + kN) % kN;
  auto rows = [&](int y0, int y1) {
    int changes = 0;
    for (int y = y0; y < y1; y++) {
      const uint8_t* src[kMaxOffsets];
      for (int k = 0; k < offsets_; k++) src[k] = cell_ + ((y + dy_[k] + kN) % kN) * kN;
      for (int x = 0; x < kN; x++) {
        const int i = y * kN + x;
        const uint8_t s = cell_[i];
        const uint8_t want = s + 1 == rule_.states ? 0 : s + 1;
        int n = 0;
        for (int k = 0; k < offsets_ && n < rule_.threshold; k++) n += src[k][xs[x + 3 + dx_[k]]] == want;
        if (n >= rule_.threshold) {
          next_[i] = want;
          changes++;
        } else {
          next_[i] = s;
        }
      }
    }
    return changes;
  };
  const int changes = forRows(kN, rows);
  memcpy(cell_, next_, sizeof cell_);
  return changes;
}

void Cyclic::tick() {
  if (step() < kN * kN / 1000) start(rng_.next(), variant_);
}

void Cyclic::renderRow(int y, uint16_t* out) const {
  const uint8_t* row = cell_ + (y / 2) * kN;
  for (int x = 0; x < kN; x++) {
    *out++ = pal_[row[x]];
    *out++ = pal_[row[x]];
  }
}

}  // namespace ca
