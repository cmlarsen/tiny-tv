#pragma once

#include <stdint.h>

namespace ca {

constexpr int kScreen = 240;

class Rng {
 public:
  explicit Rng(uint32_t seed = 1) : s_(seed ? seed : 1) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
  bool chance(int pct) { return below(100) < pct; }

 private:
  uint32_t s_;
};

struct Color {
  uint8_t r, g, b;
};

Color lerp(Color a, Color b, int t256);
Color hsv(int hue256, uint8_t s, uint8_t v);
uint16_t px(Color c);
void gradient(const Color* stops, int count, uint16_t out[256]);

using RowFn = int (*)(void* ctx, int y0, int y1);
using RowRunner = int (*)(RowFn fn, void* ctx, int rows);
void setRowRunner(RowRunner runner);
int runRows(RowFn fn, void* ctx, int rows);

template <typename F>
int forRows(int rows, F& f) {
  return runRows([](void* c, int y0, int y1) { return (*static_cast<F*>(c))(y0, y1); }, &f, rows);
}

class Mode {
 public:
  virtual ~Mode() = default;
  virtual const char* name() const = 0;
  virtual int variants() const = 0;
  virtual const char* variantName() const = 0;
  virtual int variant() const = 0;
  virtual void start(uint32_t seed, int variant) = 0;
  virtual void tick() = 0;
  virtual void renderRow(int y, uint16_t* out) const = 0;
};

}  // namespace ca
