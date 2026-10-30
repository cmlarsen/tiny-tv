#include <stdlib.h>
#include <string.h>

#include "modes.h"

namespace ca {

namespace {

const GrayScott::Params kParams[] = {
    {"Coral", 0.0545f, 0.062f, 0},
    {"Mitosis", 0.0367f, 0.0649f, 1},
    {"Maze", 0.029f, 0.057f, 2},
    {"Worms", 0.078f, 0.061f, 0},
};

const Color kPalettes[][5] = {
    {{4, 6, 20}, {10, 40, 90}, {20, 150, 170}, {150, 240, 210}, {255, 255, 240}},
    {{10, 4, 8}, {80, 10, 40}, {220, 60, 80}, {255, 170, 120}, {255, 245, 230}},
    {{2, 8, 4}, {20, 60, 30}, {110, 170, 60}, {230, 230, 120}, {255, 255, 235}},
};

constexpr int kStepsPerTick = 8;

}  // namespace

GrayScott::GrayScott() : rng_(1) {
  const size_t bytes = kN * kN * sizeof(int16_t);
  u_ = static_cast<int16_t*>(malloc(bytes));
  v_ = static_cast<int16_t*>(malloc(bytes));
  u2_ = static_cast<int16_t*>(malloc(bytes));
  v2_ = static_cast<int16_t*>(malloc(bytes));
  setParams(kParams[0]);
  fill(1, 0);
}

GrayScott::~GrayScott() {
  free(u_);
  free(v_);
  free(u2_);
  free(v2_);
}

int GrayScott::variants() const { return sizeof kParams / sizeof kParams[0]; }

void GrayScott::setParams(const Params& p) {
  p_ = p;
  feed_ = static_cast<int32_t>(p.feed * kOne + 0.5f);
  kill_ = static_cast<int32_t>(p.kill * kOne + 0.5f);
  gradient(kPalettes[p.palette], 5, pal_);
}

void GrayScott::fill(float u, float v) {
  const int16_t uq = static_cast<int16_t>(u * kOne), vq = static_cast<int16_t>(v * kOne);
  for (int i = 0; i < kN * kN; i++) {
    u_[i] = uq;
    v_[i] = vq;
  }
  quiet_ = 0;
}

void GrayScott::drop(int cx, int cy, int r) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy > r * r) continue;
      const int i = ((cy + dy + kN) % kN) * kN + (cx + dx + kN) % kN;
      u_[i] = static_cast<int16_t>(kOne / 2 + rng_.below(kOne / 10));
      v_[i] = static_cast<int16_t>(kOne / 4 + rng_.below(kOne / 10));
    }
}

void GrayScott::start(uint32_t seed, int variant) {
  rng_ = Rng(seed);
  variant_ = variant;
  setParams(kParams[variant]);
  fill(1, 0);
  for (int n = 8 + rng_.below(8); n > 0; n--) drop(rng_.below(kN), rng_.below(kN), 2 + rng_.below(4));
}

int GrayScott::step() {
  const int32_t feed = feed_, fk = feed_ + kill_;
  auto rows = [&](int y0, int y1) {
    int changed = 0;
    for (int y = y0; y < y1; y++) {
      const int ya = ((y + kN - 1) % kN) * kN, yb = y * kN, yc = ((y + 1) % kN) * kN;
      for (int x = 0; x < kN; x++) {
        const int xl = x == 0 ? kN - 1 : x - 1, xr = x == kN - 1 ? 0 : x + 1;
        const int i = yb + x;
        const int32_t u = u_[i], v = v_[i];
        const int32_t lu = (4 * (u_[ya + x] + u_[yc + x] + u_[yb + xl] + u_[yb + xr]) + u_[ya + xl] + u_[ya + xr] +
                            u_[yc + xl] + u_[yc + xr] - 20 * u) * 3277 >> 16;
        const int32_t lv = (4 * (v_[ya + x] + v_[yc + x] + v_[yb + xl] + v_[yb + xr]) + v_[ya + xl] + v_[ya + xr] +
                            v_[yc + xl] + v_[yc + xr] - 20 * v) * 1638 >> 16;
        const int32_t uvv = ((u * v >> 14) * v) >> 14;
        int32_t nu = u + lu - uvv + (feed * (kOne - u) >> 14);
        int32_t nv = v + lv + uvv - (fk * v >> 14);
        nu = nu < 0 ? 0 : (nu > kOne ? kOne : nu);
        nv = nv < 0 ? 0 : (nv > kOne ? kOne : nv);
        u2_[i] = static_cast<int16_t>(nu);
        v2_[i] = static_cast<int16_t>(nv);
        const int32_t d = nv - v;
        changed += d > 2 || d < -2;
      }
    }
    return changed;
  };
  const int changed = forRows(kN, rows);
  int16_t* t = u_;
  u_ = u2_;
  u2_ = t;
  t = v_;
  v_ = v2_;
  v2_ = t;
  return changed;
}

void GrayScott::tick() {
  int changed = 0;
  for (int i = 0; i < kStepsPerTick; i++) changed = step();
  quiet_ = changed < kN * kN / 50 ? quiet_ + 1 : 0;
  if (quiet_ > 60) {
    drop(rng_.below(kN), rng_.below(kN), 3 + rng_.below(4));
    quiet_ = 0;
  }
}

void GrayScott::renderRow(int y, uint16_t* out) const {
  const int16_t* a = v_ + (y >> 1) * kN;
  const int16_t* b = v_ + ((y >> 1) + 1) % kN * kN;
  int h[kN + 1];
  for (int x = 0; x < kN; x++) h[x] = (y & 1) ? (a[x] + b[x]) >> 1 : a[x];
  h[kN] = h[0];
  for (int x = 0; x < kN; x++) {
    const int c0 = h[x] >> 5, c1 = (h[x] + h[x + 1]) >> 6;
    out[2 * x] = pal_[c0 > 255 ? 255 : c0];
    out[2 * x + 1] = pal_[c1 > 255 ? 255 : c1];
  }
}

}  // namespace ca
