#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "air.h"
#include "aqilogic.h"
#include "board.h"
#include "canvas.h"
#include "display.h"
#include "portal.h"
#include "touch.h"

using aqilogic::Level;
using nmtv::Canvas;
using nmtv::rgb;

namespace {

constexpr uint32_t kFrameMs = 250;
constexpr uint32_t kFastFrameMs = 100;
constexpr uint32_t kSetupFrameMs = 1000;
constexpr uint32_t kInfoMs = 15000;
constexpr uint8_t kBrightness = 150;

constexpr uint16_t kBg = rgb(12, 14, 20);
constexpr uint16_t kText = rgb(240, 242, 250);
constexpr uint16_t kDim = rgb(130, 138, 160);
constexpr uint16_t kTrack = rgb(40, 44, 58);
constexpr uint16_t kAccent = rgb(80, 200, 255);
constexpr uint16_t kHot = rgb(255, 90, 70);

constexpr int kCx = 120;
constexpr int kCy = 120;
constexpr float kArcStart = 135;
constexpr float kArcSweep = 270;
constexpr int kArcMax = 300;

enum class View { Now, Average, Info, Setup, Connecting, Status };
const char* const kViewNames[] = {"now", "avg", "info", "setup", "connecting", "status"};

LGFX lcd;
LGFX_Sprite band(&lcd);
TouchPad touch;

View page = View::Now;
bool showInfo = false;
uint32_t infoUntilMs = 0;
int forcedView = -1;

uint16_t levelColor(Level l) {
  switch (l) {
    case Level::Good: return rgb(40, 210, 90);
    case Level::Moderate: return rgb(255, 214, 0);
    case Level::Sensitive: return rgb(255, 130, 0);
    case Level::Unhealthy: return rgb(240, 40, 40);
    case Level::VeryUnhealthy: return rgb(165, 80, 210);
    case Level::Hazardous: return rgb(200, 30, 80);
  }
  return kText;
}

uint16_t aqiColor(int aqi) { return levelColor(aqilogic::level(aqi)); }

float arcAngle(int aqi) {
  const int v = aqi < 0 ? 0 : (aqi > kArcMax ? kArcMax : aqi);
  return kArcStart + kArcSweep * v / kArcMax;
}

void fitText(Canvas& c, const char* str, int x, int y, int maxW, uint16_t color, textdatum_t datum) {
  char buf[64];
  snprintf(buf, sizeof buf, "%s", str);
  size_t n = strlen(buf);
  while (n > 1 && c.width(buf) > maxW) {
    buf[--n] = '\0';
    if (n > 1) {
      buf[n - 1] = '.';
    }
  }
  c.text(buf, x, y, color, datum);
}

void drawGauge(Canvas& c, int aqi) {
  static const int kEdges[] = {0, 50, 100, 150, 200, 300};
  for (int i = 0; i + 1 < 6; i++) {
    const int lo = kEdges[i], hi = kEdges[i + 1];
    const uint16_t col = aqiColor(hi);
    c.arc(kCx, kCy, 94, 106, arcAngle(lo), arcAngle(hi), nmtv::mix(kBg, col, 60));
    if (aqi > lo) c.arc(kCx, kCy, 94, 106, arcAngle(lo), arcAngle(aqi < hi ? aqi : hi), col);
  }
  const float a = arcAngle(aqi) * 3.14159265f / 180;
  const int mx = kCx + static_cast<int>(100 * cosf(a));
  const int my = kCy + static_cast<int>(100 * sinf(a));
  c.circle(mx, my, 10, kText);
  c.circle(mx, my, 6, aqiColor(aqi));
}

void drawLevel(Canvas& c, int aqi, int y) {
  const Level l = aqilogic::level(aqi);
  if (aqilogic::levelLine2(l)[0]) {
    c.font(&fonts::FreeSansBold9pt7b);
    c.text(aqilogic::levelLine1(l), kCx, y - 9, kText, middle_center);
    c.text(aqilogic::levelLine2(l), kCx, y + 9, kText, middle_center);
  } else {
    c.font(&fonts::FreeSansBold12pt7b);
    c.text(aqilogic::levelLine1(l), kCx, y, kText, middle_center);
  }
}

void drawNow(Canvas& c, const air::Snapshot& s) {
  const int aqi = s.now.aqi();
  drawGauge(c, aqi);

  char buf[48];
  c.font(&fonts::FreeSansBold9pt7b);
  snprintf(buf, sizeof buf, "AQI  %s", s.now.dominant());
  c.text(buf, kCx, 60, kDim, middle_center);

  snprintf(buf, sizeof buf, "%d", aqi);
  c.font(&fonts::DejaVu72);
  c.text(buf, kCx, 106, aqiColor(aqi), middle_center);

  drawLevel(c, aqi, 156);

  char age[16];
  aqilogic::ageLabel(time(nullptr), s.newestHour, age, sizeof age);
  snprintf(buf, sizeof buf, "%s  %s", s.now.site(), age);
  c.font(&fonts::FreeSans9pt7b);
  fitText(c, buf, kCx, 224, 220, kDim, middle_center);
}

void drawAverage(Canvas& c, const air::Snapshot& s) {
  char buf[40];
  c.font(&fonts::FreeSansBold9pt7b);
  snprintf(buf, sizeof buf, "%d-HOUR AVERAGE", s.hours);
  c.text(buf, kCx, 18, kDim, middle_center);

  if (!s.avg.have) {
    c.font(&fonts::FreeSansBold12pt7b);
    c.text("Collecting...", kCx, 90, kText, middle_center);
  } else {
    snprintf(buf, sizeof buf, "%d", s.avg.aqi);
    c.font(&fonts::DejaVu72);
    c.text(buf, kCx, 62, aqiColor(s.avg.aqi), middle_center);
    drawLevel(c, s.avg.aqi, 115);
    if (s.avg.hours < s.hours) snprintf(buf, sizeof buf, "%s  %d of %d h so far", s.avg.dominant, s.avg.hours, s.hours);
    else snprintf(buf, sizeof buf, "%s", s.avg.dominant);
    c.font(&fonts::FreeSans9pt7b);
    c.text(buf, kCx, 146, kDim, middle_center);
  }

  constexpr int kLeft = 12, kWidth = 216, kBase = 204, kHeight = 44;
  int top = 100;
  for (int i = 0; i < s.hours; i++)
    if (s.bars[i] > top) top = s.bars[i];
  if (top > 100) top = (top + 49) / 50 * 50;
  for (int guide : {50, 100}) {
    if (guide >= top) continue;
    const int gy = kBase - guide * kHeight / top;
    for (int x = kLeft; x < kLeft + kWidth; x += 6) c.hline(x, gy, 3, kTrack);
  }
  const float slot = static_cast<float>(kWidth) / s.hours;
  const int gap = slot >= 6 ? 2 : (slot >= 3 ? 1 : 0);
  for (int i = 0; i < s.hours; i++) {
    const int x0 = kLeft + static_cast<int>(i * slot);
    const int x1 = kLeft + static_cast<int>((i + 1) * slot) - gap;
    const int v = s.bars[i];
    if (v < 0) {
      c.rect(x0, kBase - 2, x1 - x0, 2, kTrack);
      continue;
    }
    int h = v * kHeight / top;
    if (h < 3) h = 3;
    c.rect(x0, kBase - h, x1 - x0, h, aqiColor(v));
  }
  c.hline(kLeft, kBase + 1, kWidth, kTrack);

  c.font(&fonts::FreeSans9pt7b);
  snprintf(buf, sizeof buf, "-%dh", s.hours);
  c.text(buf, kLeft, 222, kDim, middle_left);
  c.text("latest", kLeft + kWidth, 222, kDim, middle_right);
}

void drawDots(Canvas& c, uint32_t now, int y) {
  for (int i = 0; i < 3; i++) {
    const bool on = (now / 300) % 3 == static_cast<uint32_t>(i);
    c.circle(108 + i * 12, y, 4, on ? kAccent : kTrack);
  }
}

void drawStatus(Canvas& c, const air::Snapshot& s, uint32_t now) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Air Quality", kCx, 60, kText, middle_center);
  c.font(&fonts::FreeSansBold9pt7b);
  if (s.noMonitor) {
    c.text("No monitor within 80 km", kCx, 110, kHot, middle_center);
    fitText(c, s.place.name, kCx, 136, 220, kDim, middle_center);
    c.text("Set a ZIP code at", kCx, 180, kDim, middle_center);
    c.text("air-quality.local", kCx, 202, kAccent, middle_center);
  } else if (s.failed) {
    c.text(s.located ? "Can't reach AirNow" : "Can't find location", kCx, 110, kHot, middle_center);
    c.text("Retrying every minute", kCx, 136, kDim, middle_center);
  } else {
    c.text(s.located ? "Reading nearby monitors" : "Finding your location", kCx, 110, kDim, middle_center);
    drawDots(c, now, 140);
  }
}

void drawInfo(Canvas& c, const air::Snapshot& s) {
  char lines[5][44];
  snprintf(lines[0], sizeof lines[0], "IP: %s", nmtv::portal::ip().c_str());
  snprintf(lines[1], sizeof lines[1], "Settings: air-quality.local");
  snprintf(lines[2], sizeof lines[2], "Near: %s", s.located ? s.place.name : "?");
  if (s.have) snprintf(lines[3], sizeof lines[3], "Monitor: %s %.0fkm", s.now.site(),
                       s.now.pm25.have ? s.now.pm25.km : s.now.o3.km);
  else snprintf(lines[3], sizeof lines[3], "Monitor: none yet");
  snprintf(lines[4], sizeof lines[4], "Data: AirNow, hourly");

  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Air Quality", kCx, 24, kAccent, middle_center);
  c.font(&fonts::FreeSansBold9pt7b);
  for (int i = 0; i < 5; i++) fitText(c, lines[i], 12, 60 + i * 24, 220, kText, middle_left);

  const float progress = touch.resetProgress(millis());
  if (progress >= 0) {
    c.text("Keep holding to reset WiFi", kCx, 196, kHot, middle_center);
    c.roundRect(20, 210, 200, 14, 7, kTrack);
    const int w = static_cast<int>(200 * (progress < 1 ? progress : 1));
    if (w > 14) c.roundRect(20, 210, w, 14, 7, kHot);
  } else if (touch.resetArmed(millis())) {
    c.text("Hold 5 s to reset WiFi", kCx, 196, kDim, middle_center);
    c.text("Tap to go back", kCx, 220, kDim, middle_center);
  } else {
    c.text("Tap to go back", kCx, 214, kDim, middle_center);
  }
}

void drawSetup(Canvas& c) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Set up WiFi", kCx, 18, kText, middle_center);
  c.roundRect(70, 36, 100, 100, 8, rgb(255, 255, 255));
  c.qr(nmtv::portal::apQr(), 74, 40, 92);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text("1. Scan, or join WiFi", kCx, 156, kDim, middle_center);
  c.text(nmtv::portal::apName(), kCx, 176, kText, middle_center);
  c.text("2. Open in a browser", kCx, 202, kDim, middle_center);
  c.text("192.168.4.1", kCx, 222, kAccent, middle_center);
}

void drawConnecting(Canvas& c, uint32_t now) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Connecting...", kCx, 110, kText, middle_center);
  drawDots(c, now, 140);
}

View currentView(const air::Snapshot& s) {
  if (forcedView >= 0) return static_cast<View>(forcedView);
  if (!air::online()) return WiFi.getMode() & WIFI_AP ? View::Setup : View::Connecting;
  if (showInfo) return View::Info;
  if (!s.have) return View::Status;
  return page;
}

void render(uint32_t now, Print* dump = nullptr) {
  const air::Snapshot s = air::snapshot();
  const View v = currentView(s);
  nmtv::renderBands(
      band,
      [&](Canvas& c) {
        c.s.fillScreen(kBg);
        switch (v) {
          case View::Now: drawNow(c, s); break;
          case View::Average: drawAverage(c, s); break;
          case View::Info: drawInfo(c, s); break;
          case View::Setup: drawSetup(c); break;
          case View::Connecting: drawConnecting(c, now); break;
          case View::Status: drawStatus(c, s, now); break;
        }
      },
      dump);
}

uint32_t frameMs() {
  if (!air::online()) return WiFi.getMode() & WIFI_AP ? kSetupFrameMs : kFastFrameMs;
  return showInfo ? kFastFrameMs : kFrameMs;
}

void handleTouch(uint32_t now) {
  switch (touch.poll(now)) {
    case TouchPad::Event::Tap:
      if (showInfo) showInfo = false;
      else page = page == View::Now ? View::Average : View::Now;
      break;
    case TouchPad::Event::Hold:
      showInfo = true;
      infoUntilMs = now + kInfoMs;
      break;
    case TouchPad::Event::Reset: nmtv::portal::resetAll(); break;
    case TouchPad::Event::None: break;
  }
  if (showInfo && now > infoUntilMs && !touch.pressed()) showInfo = false;
}

void handleSerial(uint32_t now) {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line == "shot") {
    air::lockSerial();
    Serial.print("\nSHOT\n");
    render(now, &Serial);
    Serial.print("\nENDSHOT\n");
    air::unlockSerial();
  } else if (line == "perf") {
    const uint32_t t0 = micros();
    render(now);
    Serial.printf("render %lu us\n", static_cast<unsigned long>(micros() - t0));
  } else if (line.startsWith("view ")) {
    const String arg = line.substring(5);
    forcedView = -1;
    for (int i = 0; i < 6; i++)
      if (arg == kViewNames[i]) forcedView = i;
    Serial.printf("forced view %d\n", forcedView);
  } else if (line.startsWith("demo ")) {
    air::demo(line.substring(5).toInt());
    Serial.println("demo set (demo -1 to clear)");
  } else if (line == "refresh") {
    air::refreshNow();
  }
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  nmtv::beginDisplay(lcd);
  lcd.setBrightness(kBrightness);
  band.setColorDepth(16);
  if (!band.createSprite(nmtv::kScreen, nmtv::kBandH)) Serial.println("[main] sprite alloc failed");
  touch.begin(board::kTouch);
  air::begin();
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();

  air::loop();
  handleSerial(now);
  handleTouch(now);

  if (now - lastFrameMs >= frameMs()) {
    lastFrameMs = now;
    render(now);
  }
  delay(5);
}
