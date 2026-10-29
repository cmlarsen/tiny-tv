#include <Arduino.h>

#include "applogic.h"
#include "board.h"
#include "canvas.h"
#include "display.h"
#include "portal.h"
#include "touch.h"

using nmtv::Canvas;
using nmtv::rgb;

namespace {

constexpr uint32_t kFrameMs = 100;
constexpr uint32_t kSetupFrameMs = 1000;
constexpr uint8_t kBrightness = 150;

constexpr uint16_t kBg = rgb(14, 16, 24);
constexpr uint16_t kText = rgb(240, 242, 250);
constexpr uint16_t kDim = rgb(130, 138, 160);
constexpr uint16_t kAccent = rgb(80, 200, 255);

LGFX lcd;
LGFX_Sprite band(&lcd);
TouchPad touch;
int taps = 0;

char greeting[32];
nmtv::PortalField fields[] = {
    {"greeting", "Greeting", "Hello!", greeting, sizeof greeting},
};

void drawSetup(Canvas& c) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Set up WiFi", 120, 18, kText, middle_center);
  c.roundRect(70, 36, 100, 100, 8, rgb(255, 255, 255));
  c.qr(nmtv::portal::apQr(), 74, 40, 92);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text("1. Scan, or join WiFi", 120, 156, kDim, middle_center);
  c.text(nmtv::portal::apName(), 120, 176, kText, middle_center);
  c.text("2. Open in a browser", 120, 202, kDim, middle_center);
  c.text("192.168.4.1", 120, 222, kAccent, middle_center);
}

void drawMain(Canvas& c) {
  c.font(&fonts::FreeSansBold18pt7b);
  c.text(greeting, 120, 90, kText, middle_center);
  char label[24];
  applogic::tapLabel(taps, label, sizeof label);
  c.font(&fonts::FreeSansBold12pt7b);
  c.text(label, 120, 140, kAccent, middle_center);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text(nmtv::portal::ip().c_str(), 120, 222, kDim, middle_center);
}

void render(Print* dump = nullptr) {
  nmtv::renderBands(
      band,
      [](Canvas& c) {
        c.s.fillScreen(kBg);
        if (nmtv::portal::online()) drawMain(c);
        else drawSetup(c);
      },
      dump);
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line == "shot") {
    Serial.print("\nSHOT\n");
    render(&Serial);
    Serial.print("\nENDSHOT\n");
  } else if (line == "perf") {
    const uint32_t t0 = micros();
    render();
    Serial.printf("render %lu us\n", (unsigned long)(micros() - t0));
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
  nmtv::portal::begin({"__TITLE__", "__AP__", "__HOST__", fields, sizeof fields / sizeof fields[0]});
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();

  nmtv::portal::loop();
  handleSerial();

  switch (touch.poll(now)) {
    case TouchPad::Event::Tap: taps++; break;
    case TouchPad::Event::Reset: nmtv::portal::resetAll(); break;
    default: break;
  }

  if (now - lastFrameMs >= (nmtv::portal::online() ? kFrameMs : kSetupFrameMs)) {
    lastFrameMs = now;
    render();
  }
  delay(5);
}
