#include "fire.h"

#include <string.h>

namespace fire {

namespace {

constexpr int kEdge = 24;

struct Stop {
  uint8_t h, r, g, b;
};

constexpr Stop kStops[] = {
    {0, 0, 0, 0},          {24, 40, 3, 0},       {56, 150, 25, 0},      {90, 230, 80, 5},
    {125, 255, 140, 20},   {160, 255, 195, 70},  {200, 255, 235, 150},  {255, 255, 252, 225},
};

int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

void blurWrap(const uint8_t src[kH][kW], uint8_t dst[kH][kW]) {
  for (int y = 0; y < kH; y++) {
    const int ya = (y + kH - 2) % kH, yb = (y + kH - 1) % kH, yc = (y + 1) % kH, yd = (y + 2) % kH;
    for (int x = 0; x < kW; x++) {
      const int xa = (x + kW - 1) % kW, xb = (x + 1) % kW;
      const int sum = src[ya][x] + src[yb][x] * 2 + src[y][x] * 2 + src[yc][x] * 2 + src[yd][x] + src[y][xa] + src[y][xb];
      dst[y][x] = static_cast<uint8_t>(sum / 10);
    }
  }
}

}  // namespace

Params level(int n) {
  static const Params kTable[kLevels] = {{220, 30}, {255, 21}, {255, 16}};
  if (n < 0) n = 0;
  if (n >= kLevels) n = kLevels - 1;
  return kTable[n];
}

Sim::Sim(uint32_t seed) : rng_(seed) {
  for (int y = 0; y < kH; y++)
    for (int x = 0; x < kW; x++) cool_[y][x] = static_cast<uint8_t>(rng_.below(256));
  for (int i = 0; i < 3; i++) {
    blurWrap(cool_, heat_);
    blurWrap(heat_, cool_);
  }
  int lo = 255, hi = 0;
  for (int y = 0; y < kH; y++)
    for (int x = 0; x < kW; x++) {
      if (cool_[y][x] < lo) lo = cool_[y][x];
      if (cool_[y][x] > hi) hi = cool_[y][x];
    }
  const int span = hi > lo ? hi - lo : 1;
  for (int y = 0; y < kH; y++)
    for (int x = 0; x < kW; x++) {
      const int c = (cool_[y][x] - lo) * 255 / span;
      cool_[y][x] = static_cast<uint8_t>(c * c / 255);
    }
  memset(heat_, 0, sizeof heat_);
  for (int x = 0; x < kW; x++) fuel_[x] = 128;
}

void Sim::step(const Params& p) {
  t_++;
  for (int x = 0; x < kW; x++) fuel_[x] = static_cast<int16_t>(clamp255(fuel_[x] + rng_.below(97) - 48));
  for (int pass = 0; pass < 3; pass++)
    for (int x = 1; x < kW - 1; x++) fuel_[x] = static_cast<int16_t>((fuel_[x - 1] + fuel_[x] * 2 + fuel_[x + 1]) / 4);

  for (int y = kH - 2; y < kH; y++) {
    for (int x = 0; x < kW; x++) {
      const int edge = x < kW - 1 - x ? x : kW - 1 - x;
      const int bell = edge >= kEdge ? 256 : edge * 256 / kEdge;
      int v = (p.fuel * bell) >> 8;
      v = (v * (100 + fuel_[x])) >> 8;
      if (v > 255) v = 255;
      v -= rng_.below(v / 4 + 1);
      heat_[y][x] = static_cast<uint8_t>(v);
    }
  }

  for (int y = 0; y < kH - 2; y++) {
    const uint8_t* below = heat_[y + 1];
    const uint8_t* below2 = heat_[y + 2];
    const uint8_t* cool = cool_[(y + scroll_) % kH];
    const int coolY = p.cooling * (6 + 48 * (kH - y) / kH);
    for (int x = 0; x < kW; x++) {
      const int l = x > 0 ? below[x - 1] : 0;
      const int r = x < kW - 1 ? below[x + 1] : 0;
      const int sum = l + below[x] + r + below2[x];
      const int v = (sum >> 2) - ((cool[x] * coolY) >> 14);
      heat_[y][x] = static_cast<uint8_t>(v < 0 ? 0 : v);
    }
  }
  scroll_ = (scroll_ + 1) % kH;
}

void heatColor(uint8_t h, uint8_t& r, uint8_t& g, uint8_t& b) {
  int i = 0;
  while (h > kStops[i + 1].h) i++;
  const Stop& a = kStops[i];
  const Stop& z = kStops[i + 1];
  const int t = (h - a.h) * 256 / (z.h - a.h);
  r = static_cast<uint8_t>(a.r + (z.r - a.r) * t / 256);
  g = static_cast<uint8_t>(a.g + (z.g - a.g) * t / 256);
  b = static_cast<uint8_t>(a.b + (z.b - a.b) * t / 256);
}

void buildPalette(uint16_t out[256]) {
  for (int h = 0; h < 256; h++) {
    uint8_t r, g, b;
    heatColor(static_cast<uint8_t>(h), r, g, b);
    const uint16_t v = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    out[h] = static_cast<uint16_t>((v >> 8) | (v << 8));
  }
}

void renderRow(const Sim& s, int y, const uint16_t pal[256], uint16_t* out) {
  const uint8_t* a = s.row(y >> 1);
  const uint8_t* b = s.row((y >> 1) + 1);
  uint8_t h[kW + 1];
  if (y & 1) {
    for (int x = 0; x < kW; x++) h[x] = static_cast<uint8_t>((a[x] + b[x]) >> 1);
  } else {
    memcpy(h, a, kW);
  }
  h[kW] = h[kW - 1];
  for (int x = 0; x < kW; x++) {
    out[2 * x] = pal[h[x]];
    out[2 * x + 1] = pal[(h[x] + h[x + 1]) >> 1];
  }
}

}  // namespace fire
