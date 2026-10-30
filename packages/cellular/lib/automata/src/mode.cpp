#include "mode.h"

namespace ca {

Color lerp(Color a, Color b, int t256) {
  return {static_cast<uint8_t>(a.r + (b.r - a.r) * t256 / 256), static_cast<uint8_t>(a.g + (b.g - a.g) * t256 / 256),
          static_cast<uint8_t>(a.b + (b.b - a.b) * t256 / 256)};
}

Color hsv(int hue256, uint8_t s, uint8_t v) {
  const int h = ((hue256 % 256) + 256) % 256 * 6;
  const int sector = h / 256, f = h % 256;
  const uint8_t p = static_cast<uint8_t>(v * (255 - s) / 255);
  const uint8_t q = static_cast<uint8_t>(v * (255 - s * f / 256) / 255);
  const uint8_t t = static_cast<uint8_t>(v * (255 - s * (255 - f) / 256) / 255);
  switch (sector) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
  }
}

uint16_t px(Color c) {
  const uint16_t v = static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
  return static_cast<uint16_t>((v >> 8) | (v << 8));
}

void gradient(const Color* stops, int count, uint16_t out[256]) {
  for (int i = 0; i < 256; i++) {
    const int pos = i * (count - 1);
    const int k = pos / 255;
    const int t = (pos % 255) * 256 / 255;
    out[i] = px(k >= count - 1 ? stops[count - 1] : lerp(stops[k], stops[k + 1], t));
  }
}

namespace {

int serialRows(RowFn fn, void* ctx, int rows) { return fn(ctx, 0, rows); }

RowRunner runner = serialRows;

}  // namespace

void setRowRunner(RowRunner r) { runner = r ? r : serialRows; }

int runRows(RowFn fn, void* ctx, int rows) { return runner(fn, ctx, rows); }

}  // namespace ca
