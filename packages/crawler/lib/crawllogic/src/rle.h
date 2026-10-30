#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rle {

enum Op : uint8_t { Skip = 0, Run = 1, Literal = 2 };
constexpr int kMaxCount = 64;

struct Target {
  uint16_t* px;
  int w;
  int y0;
  int h;
};

void blit(const uint8_t* src, int w, int h, int x, int y, int scale, const uint16_t* palette, const Target& t);

}  // namespace rle
