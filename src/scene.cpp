#include "scene.h"

#include <math.h>

using piglogic::Mood;

namespace {

constexpr int kW = 240;
constexpr int kBandH = 120;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t mix(uint16_t a, uint16_t b, int t256) {
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (br - ar) * t256 / 256;
  const int g = ag + (bg - ag) * t256 / 256;
  const int bl = ab + (bb - ab) * t256 / 256;
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

constexpr uint16_t kWhite = rgb(255, 255, 255);
constexpr uint16_t kInk = rgb(45, 32, 55);
constexpr uint16_t kPink = rgb(255, 178, 198);
constexpr uint16_t kPinkDark = rgb(232, 120, 156);
constexpr uint16_t kSnout = rgb(250, 148, 178);
constexpr uint16_t kNostril = rgb(150, 58, 92);
constexpr uint16_t kBlush = rgb(255, 118, 150);
constexpr uint16_t kHood = rgb(255, 214, 40);
constexpr uint16_t kHoodDark = rgb(222, 168, 20);
constexpr uint16_t kBeanie = rgb(72, 128, 222);
constexpr uint16_t kScarf = rgb(222, 58, 72);
constexpr uint16_t kSun = rgb(255, 205, 40);
constexpr uint16_t kSunHot = rgb(255, 130, 30);
constexpr uint16_t kCloud = rgb(245, 248, 252);
constexpr uint16_t kCloudDark = rgb(150, 158, 175);
constexpr uint16_t kDrop = rgb(90, 160, 255);

struct Palette {
  uint16_t top;
  uint16_t bottom;
  uint16_t bar;
  uint16_t text;
};

Palette moodPalette(Mood m) {
  switch (m) {
    case Mood::Sunny: return {rgb(110, 196, 250), rgb(196, 236, 255), rgb(40, 120, 200), kInk};
    case Mood::Cloudy: return {rgb(150, 172, 196), rgb(208, 218, 230), rgb(90, 108, 135), kInk};
    case Mood::Hot: return {rgb(255, 150, 70), rgb(255, 222, 120), rgb(214, 90, 40), kInk};
    case Mood::Chilly: return {rgb(130, 190, 215), rgb(205, 232, 242), rgb(60, 120, 150), kInk};
    case Mood::Cold: return {rgb(150, 200, 255), rgb(232, 245, 255), rgb(70, 110, 190), kInk};
    case Mood::Rain: return {rgb(80, 100, 135), rgb(140, 160, 188), rgb(48, 62, 90), kWhite};
    case Mood::Storm: return {rgb(40, 44, 70), rgb(88, 92, 122), rgb(26, 28, 48), kWhite};
    case Mood::Snow: return {rgb(180, 208, 240), rgb(244, 249, 255), rgb(90, 120, 175), kInk};
    case Mood::Windy: return {rgb(130, 205, 228), rgb(205, 242, 240), rgb(50, 135, 150), kInk};
    case Mood::Fog: return {rgb(170, 176, 184), rgb(216, 219, 224), rgb(105, 110, 120), kInk};
  }
  return {kWhite, kWhite, kInk, kInk};
}

Palette paletteFor(const SceneState& s) {
  switch (s.view) {
    case View::Night: return {rgb(12, 18, 48), rgb(38, 42, 96), rgb(8, 10, 30), kWhite};
    case View::Setup: return {rgb(255, 196, 214), rgb(255, 232, 238), rgb(214, 96, 140), kInk};
    case View::Loading: return moodPalette(Mood::Sunny);
    case View::Offline: return moodPalette(Mood::Fog);
    case View::Info: return {rgb(30, 30, 46), rgb(30, 30, 46), rgb(20, 20, 30), kWhite};
    default: return moodPalette(s.mood);
  }
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

void background(Canvas& c, const Palette& p, uint16_t flashTint, int flash256) {
  for (int row = 0; row < kBandH; row++) {
    const int y = c.oy + row;
    uint16_t col = mix(p.top, p.bottom, y * 256 / kW);
    if (flash256) col = mix(col, flashTint, flash256);
    c.hline(0, y, kW, col);
  }
}

uint16_t backgroundAt(const Palette& p, int y) { return mix(p.top, p.bottom, y * 256 / kW); }

enum class Eyes { Open, Happy, Sleepy, Worried };

struct PigLook {
  int cx = 100;
  int cy = 158;
  Eyes eyes = Eyes::Open;
  bool blink = false;
  bool hood = false;
  bool beanie = false;
  bool scarf = false;
  bool shades = false;
  bool sweat = false;
  bool oMouth = false;
  int earWiggle = 0;
};

void drawEar(Canvas& c, const PigLook& p, int side) {
  const int w = p.earWiggle * side;
  const int ax = p.cx + side * 46, ay = p.cy - 22;
  const int bx = p.cx + side * 12, by = p.cy - 42;
  const int tx = p.cx + side * 52 + w, ty = p.cy - 64 + abs(w) / 2;
  c.tri(ax, ay, bx, by, tx, ty, kPinkDark);
  c.tri(ax - side * 3, ay - 1, bx + side * 2, by + 2, tx - side * 1, ty + 3, kPink);
  const int mx = (ax + bx + tx) / 3, my = (ay + by + ty) / 3;
  c.tri((ax + mx) / 2, (ay + my) / 2, (bx + mx) / 2, (by + my) / 2, (tx * 3 + mx) / 4, (ty * 3 + my) / 4, kPinkDark);
}

void drawEye(Canvas& c, const PigLook& p, int ex, int ey, int side) {
  if (p.blink && p.eyes != Eyes::Sleepy) {
    c.roundRect(ex - 7, ey - 1, 14, 4, 2, kInk);
    return;
  }
  switch (p.eyes) {
    case Eyes::Happy:
      c.arc(ex, ey + 4, 5, 8, 200, 340, kInk);
      break;
    case Eyes::Sleepy:
      c.arc(ex, ey - 4, 5, 8, 20, 160, kInk);
      break;
    case Eyes::Worried:
      c.line(ex + side * 8, ey - 13, ex - side * 6, ey - 18, 1.5f, kInk);
      [[fallthrough]];
    case Eyes::Open:
      c.ellipse(ex, ey, 6, 8, kInk);
      c.circle(ex + 2, ey - 3, 2, kWhite);
      break;
  }
}

void drawPig(Canvas& c, const PigLook& p) {
  const int cx = p.cx, cy = p.cy;

  if (p.hood) {
    c.ellipse(cx, cy + 66, 62, 36, kHoodDark);
    c.ellipse(cx, cy + 66, 59, 33, kHood);
    c.circle(cx, cy + 50, 4, kHoodDark);
    c.circle(cx, cy + 62, 4, kHoodDark);
    for (int side = -1; side <= 1; side += 2) {
      c.circle(cx + side * 38, cy - 50, 15, kHoodDark);
      c.circle(cx + side * 38, cy - 50, 12, kHood);
    }
    c.ellipse(cx, cy - 4, 66, 58, kHoodDark);
    c.ellipse(cx, cy - 4, 63, 55, kHood);
    c.line(cx - 20, cy + 46, cx - 24, cy + 62, 1.5f, kHoodDark);
    c.line(cx + 20, cy + 46, cx + 24, cy + 62, 1.5f, kHoodDark);
  } else {
    c.ellipse(cx, cy + 66, 50, 32, kPinkDark);
    c.ellipse(cx, cy + 66, 48, 30, kPink);
    drawEar(c, p, -1);
    drawEar(c, p, 1);
  }

  c.ellipse(cx, cy, 56, 48, kPinkDark);
  c.ellipse(cx, cy, 54, 46, kPink);

  c.ellipse(cx - 34, cy + 12, 10, 6, kBlush);
  c.ellipse(cx + 34, cy + 12, 10, 6, kBlush);

  if (p.shades) {
    c.roundRect(cx - 35, cy - 18, 29, 19, 7, kInk);
    c.roundRect(cx + 6, cy - 18, 29, 19, 7, kInk);
    c.line(cx - 7, cy - 12, cx + 7, cy - 12, 2, kInk);
    c.line(cx - 29, cy - 13, cx - 24, cy - 8, 1, kWhite);
    c.line(cx + 12, cy - 13, cx + 17, cy - 8, 1, kWhite);
  } else {
    drawEye(c, p, cx - 20, cy - 8, -1);
    drawEye(c, p, cx + 20, cy - 8, 1);
  }

  c.ellipse(cx, cy + 14, 21, 15, kPinkDark);
  c.ellipse(cx, cy + 14, 19, 13, kSnout);
  c.ellipse(cx - 7, cy + 14, 3, 5, kNostril);
  c.ellipse(cx + 7, cy + 14, 3, 5, kNostril);

  if (p.oMouth) {
    c.ellipse(cx, cy + 35, 4, 5, kInk);
  } else {
    c.arc(cx, cy + 29, 5, 7, 30, 150, kInk);
  }

  if (p.sweat) {
    const int x = cx + 48, y = cy - 24;
    c.circle(x, y, 5, kDrop);
    c.tri(x - 5, y - 1, x + 5, y - 1, x, y - 12, kDrop);
    c.circle(x - 2, y - 1, 1, kWhite);
  }

  if (p.beanie) {
    c.ellipseArc(cx, cy - 24, 52, 34, 180, 360, kBeanie);
    for (int i = -3; i <= 3; i++) c.line(cx + i * 14, cy - 28, cx + i * 11, cy - 50 + abs(i) * 3, 1, rgb(110, 160, 240));
    c.roundRect(cx - 55, cy - 31, 110, 14, 7, kWhite);
    for (int x = cx - 48; x <= cx + 48; x += 8) c.line(x, cy - 28, x, cy - 21, 1, rgb(210, 215, 228));
    c.circle(cx, cy - 60, 10, kWhite);
  }

  if (p.scarf) {
    c.roundRect(cx - 48, cy + 40, 96, 15, 7, kScarf);
    for (int x = cx - 36; x <= cx + 36; x += 14) c.rect(x, cy + 41, 4, 13, kWhite);
    c.roundRect(cx + 18, cy + 46, 16, 24, 5, kScarf);
    c.rect(cx + 20, cy + 56, 12, 4, kWhite);
  }
}

void drawCloud(Canvas& c, int x, int y, uint16_t col) {
  c.circle(x - 11, y + 4, 10, col);
  c.circle(x + 2, y - 4, 13, col);
  c.circle(x + 14, y + 4, 9, col);
  c.roundRect(x - 21, y + 2, 44, 13, 6, col);
}

void drawSun(Canvas& c, int x, int y, int r, uint16_t col, uint32_t t) {
  const float spin = t / 1500.0f;
  for (int i = 0; i < 8; i++) {
    const float a = spin + i * (float)M_PI / 4;
    c.line(x + cosf(a) * (r + 4), y + sinf(a) * (r + 4), x + cosf(a) * (r + 10), y + sinf(a) * (r + 10), 2, col);
  }
  c.circle(x, y, r, col);
}

void drawIcon(Canvas& c, const SceneState& s, const Palette& pal, int x, int y, uint32_t t) {
  if (s.view == View::Night) {
    c.circle(x, y, 15, rgb(255, 236, 160));
    c.circle(x + 8, y - 6, 13, backgroundAt(pal, y - 6));
    return;
  }
  switch (s.mood) {
    case Mood::Sunny: drawSun(c, x, y, 13, kSun, t); break;
    case Mood::Hot: drawSun(c, x, y, 14, kSunHot, t); break;
    case Mood::Cloudy: drawCloud(c, x, y, kCloud); break;
    case Mood::Chilly:
      drawSun(c, x + 9, y - 8, 9, kSun, t);
      drawCloud(c, x - 3, y + 4, kCloud);
      break;
    case Mood::Cold:
      for (int i = 0; i < 3; i++) {
        const float a = i * (float)M_PI / 3 + t / 3000.0f;
        c.line(x - cosf(a) * 16, y - sinf(a) * 16, x + cosf(a) * 16, y + sinf(a) * 16, 2, kWhite);
      }
      c.circle(x, y, 4, kWhite);
      break;
    case Mood::Rain:
      drawCloud(c, x, y - 4, kCloud);
      for (int i = -1; i <= 1; i++) {
        const int dy = (t / 60 + (i + 1) * 5) % 12;
        c.line(x + i * 11, y + 14 + dy, x + i * 11 - 2, y + 19 + dy, 1.5f, kDrop);
      }
      break;
    case Mood::Storm:
      drawCloud(c, x, y - 4, kCloudDark);
      c.tri(x + 2, y + 8, x - 8, y + 22, x + 1, y + 21, kSun);
      c.tri(x + 1, y + 19, x + 8, y + 19, x - 4, y + 34, kSun);
      break;
    case Mood::Snow:
      drawCloud(c, x, y - 4, kCloud);
      for (int i = -1; i <= 1; i++) c.circle(x + i * 11, y + 18 + ((t / 200 + i) % 3) * 3, 2, kWhite);
      break;
    case Mood::Windy:
      for (int i = 0; i < 3; i++) {
        const int len = 26 - i * 6;
        const int yy = y - 10 + i * 10;
        c.line(x - 16, yy, x - 16 + len, yy, 2, kWhite);
        c.arc(x - 16 + len, yy - 4, 3, 5, 270, 450, kWhite);
      }
      break;
    case Mood::Fog:
      for (int i = 0; i < 4; i++) c.line(x - 16 + (i % 2) * 4, y - 12 + i * 8, x + 14 + (i % 2) * 4, y - 12 + i * 8, 2, kWhite);
      break;
  }
}

int drawTemp(Canvas& c, int x, int y, int value, const lgfx::IFont* f, uint16_t col, bool centered) {
  char buf[8];
  snprintf(buf, sizeof buf, "%d", value);
  c.font(f);
  const int w = c.width(buf);
  const int fh = c.s.fontHeight();
  const int r = fh >= 30 ? 4 : 2;
  const int total = w + r * 2 + 3;
  const int left = centered ? x - total / 2 : x;
  c.text(buf, left, y, col, middle_left);
  const int ry = y - fh / 4;
  c.ring(left + w + r + 2, ry, r, col);
  if (r > 2) c.ring(left + w + r + 2, ry, r - 1, col);
  return total;
}

void drawFog(Canvas& c, const SceneState& s, uint32_t t) {
  if (s.view != View::Today && s.view != View::Tomorrow) return;
  if (s.mood != Mood::Fog) return;
  for (int i = 0; i < 4; i++) {
    const int bx = (int)((t / 30 + i * 90) % 360) - 120;
    const int by = 104 + i * 26;
    for (int row = 0; row < 8; row += 2) c.hline(bx, by + row, 130, rgb(236, 238, 242));
  }
}

void drawParticles(Canvas& c, const SceneState& s, uint32_t t) {
  if (s.view != View::Today && s.view != View::Tomorrow) return;
  switch (s.mood) {
    case Mood::Rain:
    case Mood::Storm:
      for (int i = 0; i < 28; i++) {
        const int y = 84 + (i * 41 + t / 7) % 130;
        const int x = (i * 67 + y / 4) % kW;
        c.line(x, y, x - 3, y + 9, 1, rgb(200, 222, 255));
      }
      break;
    case Mood::Snow:
      for (int i = 0; i < 24; i++) {
        const int y = 84 + (i * 37 + t / 28) % 130;
        const int x = (i * 53 + (int)(sinf(t / 700.0f + i) * 7) + kW) % kW;
        c.circle(x, y, i % 3 == 0 ? 3 : 2, kWhite);
      }
      break;
    case Mood::Windy:
      for (int i = 0; i < 4; i++) {
        const int x = kW - (int)((t / 4 + i * 97) % 340);
        const int y = 104 + i * 27;
        c.line(x, y, x + 34, y, 1.5f, kWhite);
      }
      break;
    default:
      break;
  }
}

void drawStars(Canvas& c, uint32_t t) {
  for (int i = 0; i < 16; i++) {
    const int x = (i * 83 + 17) % kW;
    const int y = 88 + (i * 47) % 120;
    const bool twinkle = ((t / 450) + i * 3) % 7 == 0;
    c.circle(x, y, twinkle ? 2 : 1, rgb(255, 250, 210));
  }
}

void drawZzz(Canvas& c, int cx, int cy, uint32_t t) {
  c.font(&fonts::FreeSansBold9pt7b);
  for (int k = 0; k < 3; k++) {
    const float ph = fmodf(t / 2400.0f + k / 3.0f, 1.0f);
    c.text("z", cx + 44 + (int)(ph * 22), cy - 44 - (int)(ph * 34), kWhite, middle_center);
  }
}

void drawBubble(Canvas& c, const char* msg, int tailX) {
  c.roundRect(6, 4, 228, 78, 16, kWhite);
  c.tri(tailX - 14, 78, tailX + 10, 78, tailX - 2, 96, kWhite);

  static char lines[4][64];
  const lgfx::IFont* fontsToTry[] = {&fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b};
  const int lineHeights[] = {22, 17};
  const int maxLines[] = {3, 4};
  for (int fi = 0; fi < 2; fi++) {
    c.font(fontsToTry[fi]);
    int n = 0;
    char cur[64] = "";
    bool overflow = false;
    const char* p = msg;
    while (*p && !overflow) {
      while (*p == ' ') p++;
      const char* end = p;
      while (*end && *end != ' ') end++;
      char word[48];
      const int wl = min((int)(end - p), (int)sizeof word - 1);
      memcpy(word, p, wl);
      word[wl] = '\0';
      p = end;
      if (!wl) break;
      char trial[64];
      snprintf(trial, sizeof trial, cur[0] ? "%s %s" : "%s%s", cur, word);
      if (cur[0] && c.width(trial) > 208) {
        if (n == maxLines[fi]) { overflow = true; break; }
        strcpy(lines[n++], cur);
        strcpy(cur, word);
      } else {
        strcpy(cur, trial);
      }
    }
    if (cur[0]) {
      if (n == maxLines[fi]) overflow = true;
      else strcpy(lines[n++], cur);
    }
    if (overflow && fi == 0) continue;
    const int lh = lineHeights[fi];
    const int y0 = 43 - (n - 1) * lh / 2;
    for (int i = 0; i < n; i++) c.text(lines[i], 120, y0 + i * lh, kInk, middle_center);
    return;
  }
}

PigLook lookFor(const SceneState& s, uint32_t t) {
  PigLook p;
  const bool bouncing = s.bounceAtMs && t - s.bounceAtMs < 700;
  int dy = (int)lroundf(sinf(t / 700.0f) * 2);
  if (bouncing) dy -= (int)(sinf((float)M_PI * (t - s.bounceAtMs) / 700.0f) * 14);
  p.cy += dy;
  p.blink = (t % 3700) < 130;
  if ((t % 5200) < 320) p.earWiggle = (int)(sinf(t / 40.0f) * 3);

  if (s.view == View::Night) {
    p.eyes = Eyes::Sleepy;
    return p;
  }
  if (s.view == View::Offline) {
    p.eyes = Eyes::Worried;
    p.oMouth = true;
    return p;
  }
  if (s.view == View::Setup) {
    p.cx = 64;
    return p;
  }
  if (s.view == View::Loading) return p;

  switch (s.mood) {
    case Mood::Sunny: break;
    case Mood::Cloudy: break;
    case Mood::Hot: p.shades = true; p.sweat = true; break;
    case Mood::Chilly: p.scarf = true; break;
    case Mood::Cold:
      p.beanie = true;
      p.scarf = true;
      if ((t % 2200) < 450) p.cx += ((t / 60) % 2) ? 2 : -2;
      break;
    case Mood::Rain: p.hood = true; break;
    case Mood::Storm: p.hood = true; p.eyes = Eyes::Worried; p.oMouth = true; break;
    case Mood::Snow: p.beanie = true; p.scarf = true; break;
    case Mood::Windy: p.earWiggle = (int)(sinf(t / 110.0f) * 7); break;
    case Mood::Fog: break;
  }
  if (bouncing) p.eyes = Eyes::Happy;
  return p;
}

void drawBar(Canvas& c, const SceneState& s, const Palette& pal) {
  c.rect(0, 214, kW, 26, pal.bar);
  c.font(&fonts::FreeSansBold9pt7b);
  const int y = 227;
  if (s.view == View::Setup) {
    c.text("WiFi: Piggy-Weather", 120, y, kWhite, middle_center);
    return;
  }
  if (!s.haveWeather || s.view == View::Loading || s.view == View::Offline) {
    c.text("Piggy Weather", 120, y, kWhite, middle_center);
    return;
  }
  int x = 8;
  c.text("H", x, y, kWhite, middle_left);
  x += c.width("H") + 3;
  x += drawTemp(c, x, y, s.hi, &fonts::FreeSansBold9pt7b, kWhite, false) + 8;
  c.text("L", x, y, kWhite, middle_left);
  x += c.width("L") + 3;
  drawTemp(c, x, y, s.lo, &fonts::FreeSansBold9pt7b, kWhite, false);

  char buf[8];
  snprintf(buf, sizeof buf, "%d%%", s.precip);
  const int px = 136;
  c.circle(px, y + 2, 4, kDrop);
  c.tri(px - 4, y + 1, px + 4, y + 1, px, y - 7, kDrop);
  c.text(buf, px + 8, y, kWhite, middle_left);

  c.text(s.clock, 232, y, kWhite, middle_right);
}

void drawWeatherView(Canvas& c, const SceneState& s, const Palette& pal, uint32_t t) {
  if (s.view == View::Night) drawStars(c, t);
  drawFog(c, s, t);
  const PigLook look = lookFor(s, t);
  drawPig(c, look);
  if (s.view == View::Night) drawZzz(c, look.cx, look.cy, t);
  drawParticles(c, s, t);

  const bool hasColumn = s.haveWeather && (s.view == View::Today || s.view == View::Tomorrow || s.view == View::Night);
  if (hasColumn) {
    drawIcon(c, s, pal, 200, 118, t);
    drawTemp(c, 200, 166, s.bigTemp, &fonts::FreeSansBold18pt7b, pal.text, true);
    c.font(&fonts::FreeSansBold9pt7b);
    c.text(s.bigLabel, 200, 194, pal.text, middle_center);
  }
  if (s.view == View::Setup && s.qr) {
    c.roundRect(126, 98, 108, 108, 8, kWhite);
    c.qr(s.qr, 130, 102, 100);
  }

  drawBar(c, s, pal);
  drawBubble(c, s.message, look.cx);
}

void drawInfoView(Canvas& c, const SceneState& s) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Piggy Weather", 120, 24, kPink, middle_center);
  c.font(&fonts::FreeSansBold9pt7b);
  for (int i = 0; i < 5; i++) {
    if (s.info[i][0]) c.text(s.info[i], 12, 60 + i * 24, kWhite, middle_left);
  }
  if (s.resetProgress >= 0) {
    c.text("Keep holding to reset WiFi", 120, 196, rgb(255, 200, 120), middle_center);
    c.roundRect(20, 210, 200, 14, 7, rgb(70, 70, 90));
    const int w = (int)(200 * min(1.0f, s.resetProgress));
    if (w > 14) c.roundRect(20, 210, w, 14, 7, rgb(255, 120, 150));
  } else {
    c.text("Tap to go back", 120, 214, rgb(160, 160, 180), middle_center);
  }
}

}  // namespace

bool Scene::begin() {
  band_.setColorDepth(16);
  return band_.createSprite(kW, kBandH) != nullptr;
}

void Scene::render(const SceneState& s, uint32_t t, Print* dump) {
  const Palette pal = paletteFor(s);
  int flash = 0;
  if (s.view == View::Today || s.view == View::Tomorrow) {
    if (s.mood == Mood::Storm && (t % 4300) < 140) flash = 150;
  }
  for (int oy = 0; oy < kW; oy += kBandH) {
    Canvas c{band_, oy};
    background(c, pal, rgb(230, 230, 255), flash);
    if (s.view == View::Info) drawInfoView(c, s);
    else drawWeatherView(c, s, pal, t);
    band_.pushSprite(0, oy);
    if (dump) dump->write(static_cast<const uint8_t*>(band_.getBuffer()), kW * kBandH * 2);
  }
}
