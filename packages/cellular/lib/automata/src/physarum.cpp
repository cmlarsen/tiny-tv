#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "modes.h"

namespace ca {

namespace {

const Physarum::Params kParams[] = {
    {"Veins", 32, 9, 32, 40, 235, 0, 0},
    {"Web", 16, 18, 16, 30, 245, 0, 1},
    {"Bloom", 48, 6, 24, 50, 220, 1, 2},
};

const Color kPalettes[][5] = {
    {{0, 0, 0}, {50, 10, 80}, {190, 30, 120}, {255, 140, 40}, {255, 245, 200}},
    {{0, 0, 0}, {5, 20, 70}, {10, 110, 160}, {80, 230, 220}, {240, 255, 255}},
    {{0, 0, 0}, {10, 50, 20}, {40, 170, 40}, {190, 255, 60}, {250, 255, 220}},
};

constexpr float kTau = 6.2831853f;

}  // namespace

Physarum::Physarum() : rng_(1) {
  trail_ = static_cast<uint8_t*>(calloc(kN * kN, 1));
  tmp_ = static_cast<uint8_t*>(calloc(kN * kN, 1));
  x_ = static_cast<float*>(calloc(kAgents, sizeof(float)));
  y_ = static_cast<float*>(calloc(kAgents, sizeof(float)));
  heading_ = static_cast<uint8_t*>(calloc(kAgents, 1));
  for (int i = 0; i < 256; i++) {
    cos_[i] = cosf(i * kTau / 256);
    sin_[i] = sinf(i * kTau / 256);
  }
}

Physarum::~Physarum() {
  free(trail_);
  free(tmp_);
  free(x_);
  free(y_);
  free(heading_);
}

int Physarum::variants() const { return sizeof kParams / sizeof kParams[0]; }

void Physarum::start(uint32_t seed, int variant) {
  rng_ = Rng(seed);
  variant_ = variant;
  p_ = kParams[variant];
  gradient(kPalettes[p_.palette], 5, pal_);
  memset(trail_, 0, kN * kN);
  const float c = kN / 2.0f;
  for (int i = 0; i < kAgents; i++) {
    const uint8_t a = static_cast<uint8_t>(rng_.below(256));
    if (p_.spawn == 0) {
      x_[i] = static_cast<float>(rng_.below(kN * 16)) / 16;
      y_[i] = static_cast<float>(rng_.below(kN * 16)) / 16;
      heading_[i] = a;
    } else {
      const float r = sqrtf(static_cast<float>(rng_.below(1024)) / 1024) * kN * 0.2f;
      x_[i] = c + cos_[a] * r;
      y_[i] = c + sin_[a] * r;
      heading_[i] = a;
    }
  }
}

uint8_t Physarum::sense(float x, float y, uint8_t heading) const {
  int sx = static_cast<int>(x + cos_[heading] * p_.sensorDist + kN) % kN;
  int sy = static_cast<int>(y + sin_[heading] * p_.sensorDist + kN) % kN;
  return trail_[sy * kN + sx];
}

void Physarum::step() {
  for (int i = 0; i < kAgents; i++) {
    uint8_t h = heading_[i];
    const uint8_t f = sense(x_[i], y_[i], h);
    const uint8_t l = sense(x_[i], y_[i], static_cast<uint8_t>(h - p_.sensorAngle));
    const uint8_t r = sense(x_[i], y_[i], static_cast<uint8_t>(h + p_.sensorAngle));
    if (f < l && f < r) h = static_cast<uint8_t>(rng_.chance(50) ? h + p_.turn : h - p_.turn);
    else if (l > f) h = static_cast<uint8_t>(h - p_.turn);
    else if (r > f) h = static_cast<uint8_t>(h + p_.turn);
    if (rng_.below(16) == 0) h = static_cast<uint8_t>(h + rng_.below(9) - 4);
    heading_[i] = h;
    float x = x_[i] + cos_[h];
    float y = y_[i] + sin_[h];
    if (x < 0) x += kN;
    if (x >= kN) x -= kN;
    if (y < 0) y += kN;
    if (y >= kN) y -= kN;
    x_[i] = x;
    y_[i] = y;
    uint8_t& t = trail_[static_cast<int>(y) * kN + static_cast<int>(x)];
    t = t + p_.deposit > 255 ? 255 : static_cast<uint8_t>(t + p_.deposit);
  }
}

void Physarum::diffuse() {
  const uint32_t decay = p_.decay;
  auto rows = [&](int y0, int y1) {
    for (int y = y0; y < y1; y++) {
      const uint8_t* a = trail_ + ((y + kN - 1) % kN) * kN;
      const uint8_t* b = trail_ + y * kN;
      const uint8_t* c = trail_ + ((y + 1) % kN) * kN;
      uint8_t* out = tmp_ + y * kN;
      uint32_t left = a[kN - 1] + b[kN - 1] + c[kN - 1];
      uint32_t mid = a[0] + b[0] + c[0];
      for (int x = 0; x < kN; x++) {
        const int xr = x == kN - 1 ? 0 : x + 1;
        const uint32_t right = a[xr] + b[xr] + c[xr];
        out[x] = static_cast<uint8_t>(((left + mid + right) * 7282 >> 16) * decay >> 8);
        left = mid;
        mid = right;
      }
    }
    return 0;
  };
  forRows(kN, rows);
  uint8_t* t = trail_;
  trail_ = tmp_;
  tmp_ = t;
}

void Physarum::tick() {
  step();
  diffuse();
}

void Physarum::renderRow(int y, uint16_t* out) const {
  const uint8_t* row = trail_ + y * kN;
  for (int x = 0; x < kN; x++) out[x] = pal_[row[x]];
}

}  // namespace ca
