#pragma once

#include "display.h"

namespace nmtv {

constexpr int kScreen = 240;
constexpr int kBandH = 120;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

inline uint16_t mix(uint16_t a, uint16_t b, int t256) {
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (br - ar) * t256 / 256;
  const int g = ag + (bg - ag) * t256 / 256;
  const int bl = ab + (bb - ab) * t256 / 256;
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

struct Canvas {
  LGFX_Sprite& s;
  int oy;

  void hline(int x, int y, int w, uint16_t c) { s.drawFastHLine(x, y - oy, w, c); }
  void rect(int x, int y, int w, int h, uint16_t c) { s.fillRect(x, y - oy, w, h, c); }
  void roundRect(int x, int y, int w, int h, int r, uint16_t c) { s.fillSmoothRoundRect(x, y - oy, w, h, r, c); }
  void circle(int x, int y, int r, uint16_t c) { s.fillSmoothCircle(x, y - oy, r, c); }
  void ring(int x, int y, int r, uint16_t c) { s.drawCircle(x, y - oy, r, c); }
  void ellipse(int x, int y, int rx, int ry, uint16_t c) { s.fillEllipse(x, y - oy, rx, ry, c); }
  void ellipseArc(int x, int y, int rx, int ry, float a0, float a1, uint16_t c) {
    s.fillEllipseArc(x, y - oy, 0, rx, 0, ry, a0, a1, c);
  }
  void arc(int x, int y, int r0, int r1, float a0, float a1, uint16_t c) { s.fillArc(x, y - oy, r0, r1, a0, a1, c); }
  void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    s.fillTriangle(x0, y0 - oy, x1, y1 - oy, x2, y2 - oy, c);
  }
  void line(float x0, float y0, float x1, float y1, float r, uint16_t c) {
    s.drawWideLine(x0, y0 - oy, x1, y1 - oy, r, c);
  }
  void font(const lgfx::IFont* f) { s.setFont(f); }
  int width(const char* str) { return s.textWidth(str); }
  void text(const char* str, int x, int y, uint16_t c, textdatum_t datum) {
    s.setTextColor(c);
    s.setTextDatum(datum);
    s.drawString(str, x, y - oy);
  }
  void qr(const char* str, int x, int y, int w) { s.qrcode(str, x, y - oy, w); }
};

}  // namespace nmtv
