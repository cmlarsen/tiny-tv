#include "rle.h"

namespace rle {

namespace {

const uint8_t* skipRow(const uint8_t* p, int w) {
  for (int x = 0; x < w;) {
    const uint8_t b = *p++;
    const int n = (b & 63) + 1;
    if ((b >> 6) == Run) p++;
    else if ((b >> 6) == Literal) p += n;
    x += n;
  }
  return p;
}

}  // namespace

void blit(const uint8_t* src, int w, int h, int x, int y, int scale, const uint16_t* palette, const Target& t) {
  const uint8_t* p = src;
  for (int sy = 0; sy < h; sy++) {
    int r0 = y + sy * scale - t.y0;
    int r1 = r0 + scale;
    if (r1 <= 0 || r0 >= t.h) {
      if (r0 >= t.h) return;
      p = skipRow(p, w);
      continue;
    }
    if (r0 < 0) r0 = 0;
    if (r1 > t.h) r1 = t.h;
    int dx = x;
    for (int sx = 0; sx < w;) {
      const uint8_t b = *p++;
      const int n = (b & 63) + 1;
      const int op = b >> 6;
      for (int i = 0; i < n; i++) {
        const int idx = op == Run ? p[0] : op == Literal ? p[i] : -1;
        if (idx >= 0) {
          const uint16_t c = palette[idx];
          for (int px = dx; px < dx + scale; px++) {
            if (px < 0 || px >= t.w) continue;
            for (int r = r0; r < r1; r++) t.px[r * t.w + px] = c;
          }
        }
        dx += scale;
      }
      if (op == Run) p++;
      else if (op == Literal) p += n;
      sx += n;
    }
  }
}

}  // namespace rle
