#pragma once

#include <stdint.h>

namespace fire {

constexpr int kW = 120;
constexpr int kVisH = 120;
constexpr int kH = kVisH + 4;
constexpr int kLevels = 3;

struct Params {
  uint8_t fuel;
  uint8_t cooling;
};

Params level(int n);

class Rng {
 public:
  explicit Rng(uint32_t seed) : s_(seed ? seed : 1) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }

 private:
  uint32_t s_;
};

class Sim {
 public:
  explicit Sim(uint32_t seed = 1);
  void step(const Params& p);
  uint8_t at(int x, int y) const { return heat_[y][x]; }
  const uint8_t* row(int y) const { return heat_[y]; }
  uint8_t* row(int y) { return heat_[y]; }

 private:
  uint8_t heat_[kH][kW];
  uint8_t cool_[kH][kW];
  int16_t fuel_[kW];
  int scroll_ = 0;
  uint32_t t_ = 0;
  Rng rng_;
};

void heatColor(uint8_t h, uint8_t& r, uint8_t& g, uint8_t& b);
void buildPalette(uint16_t out[256]);
void renderRow(const Sim& s, int y, const uint16_t pal[256], uint16_t* out);

}  // namespace fire
