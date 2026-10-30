#include <Arduino.h>
#include <Preferences.h>

#include "board.h"
#include "canvas.h"
#include "display.h"
#include "pomologic.h"
#include "touch.h"

using nmtv::Canvas;
using nmtv::rgb;
using pomo::Gesture;
using pomo::Page;
using pomo::Phase;

namespace {

constexpr uint32_t kFrameMs = 100;
constexpr uint32_t kFlashToggleMs = 300;
constexpr uint8_t kBrightness = 150;

struct Theme {
  const char* name;
  uint16_t bg, focus, rest, text, dim;
};

constexpr Theme kThemes[pomo::kThemeCount] = {
    {"Tomato", rgb(24, 14, 14), rgb(200, 52, 40), rgb(56, 150, 86), rgb(250, 242, 236), rgb(150, 120, 116)},
    {"Ocean", rgb(8, 16, 30), rgb(28, 100, 196), rgb(24, 160, 150), rgb(236, 246, 255), rgb(110, 136, 170)},
    {"Forest", rgb(12, 20, 14), rgb(52, 128, 64), rgb(176, 136, 40), rgb(240, 248, 236), rgb(120, 146, 120)},
    {"Sunset", rgb(26, 12, 32), rgb(222, 100, 44), rgb(136, 72, 196), rgb(255, 242, 230), rgb(160, 120, 160)},
    {"Mono", rgb(0, 0, 0), rgb(90, 90, 96), rgb(170, 170, 176), rgb(255, 255, 255), rgb(130, 130, 136)},
    {"Light", rgb(255, 255, 255), rgb(250, 168, 156), rgb(160, 220, 178), rgb(28, 28, 34), rgb(130, 130, 140)},
};

LGFX lcd;
LGFX_Sprite band(&lcd);
TouchPad touch;
pomo::Gestures gestures;
pomo::App app;
Preferences prefs;

uint32_t clockMs = 0;
uint32_t lastRealMs = 0;
uint32_t speed = 1;

const Theme& theme() { return kThemes[app.settings.theme]; }

void loadSettings() {
  prefs.begin("pomodoro", true);
  app.settings.focusMin = prefs.getInt("focus", 25);
  app.settings.breakMin = prefs.getInt("break", 5);
  app.settings.theme = prefs.getInt("theme", 0);
  prefs.end();
  app.settings.focusMin = pomo::stepMinutes(app.settings.focusMin, 0);
  app.settings.breakMin = pomo::stepMinutes(app.settings.breakMin, 0);
  if (app.settings.theme < 0 || app.settings.theme >= pomo::kThemeCount) app.settings.theme = 0;
}

void saveSettings() {
  prefs.begin("pomodoro", false);
  prefs.putInt("focus", app.settings.focusMin);
  prefs.putInt("break", app.settings.breakMin);
  prefs.putInt("theme", app.settings.theme);
  prefs.end();
}

void bigText(Canvas& c, const char* str, int y, uint16_t color) {
  c.font(&fonts::Font7);
  c.text(str, 120, y, color, middle_center);
}

void drawTimer(Canvas& c) {
  const Theme& t = theme();
  const uint32_t now = clockMs;
  const Phase phase = app.timer.phase();
  char clock[12];
  c.s.fillScreen(t.bg);

  if (phase == Phase::FocusDone || phase == Phase::BreakDone) {
    const bool on = (app.timer.sinceDoneMs(now) / kFlashToggleMs) % 2 == 0;
    const bool focusDone = phase == Phase::FocusDone;
    if (on) c.s.fillScreen(focusDone ? t.focus : t.rest);
    c.font(&fonts::FreeSansBold18pt7b);
    c.text(focusDone ? "Focus" : "Break's", 120, 92, t.text, middle_center);
    c.text(focusDone ? "complete!" : "over!", 120, 130, t.text, middle_center);
    c.font(&fonts::FreeSansBold9pt7b);
    c.text(focusDone ? "Time for a break" : "Back to focus", 120, 180, t.text, middle_center);
    return;
  }

  const float p = app.timer.progress(now);
  if (phase == Phase::Focus) {
    const int h = static_cast<int>(p * nmtv::kScreen + 0.5f);
    c.rect(0, nmtv::kScreen - h, nmtv::kScreen, h, t.focus);
  } else if (phase == Phase::Break) {
    const int h = static_cast<int>((1 - p) * nmtv::kScreen + 0.5f);
    c.rect(0, nmtv::kScreen - h, nmtv::kScreen, h, t.rest);
  }

  const char* label = "";
  const char* hint = "";
  switch (phase) {
    case Phase::Ready:
      pomo::formatClock(app.settings.focusMin * 60000u, clock, sizeof clock);
      hint = "Tap to start";
      break;
    case Phase::Focus:
      pomo::formatClock(app.timer.remainingMs(now), clock, sizeof clock);
      break;
    default:
      label = "Break";
      pomo::formatClock(app.timer.remainingMs(now), clock, sizeof clock);
      break;
  }
  if (app.timer.paused()) hint = "Paused";

  c.font(&fonts::FreeSansBold12pt7b);
  c.text(label, 120, 52, t.text, middle_center);
  bigText(c, clock, 120, t.text);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text(hint, 120, 180, t.text, middle_center);
}

void drawSettings(Canvas& c) {
  const Theme& t = theme();
  c.s.fillScreen(t.bg);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text("Settings", 120, 18, t.dim, middle_center);

  const char* title = app.page == Page::Focus ? "Focus length" : app.page == Page::Break ? "Break length" : "Color theme";
  c.font(&fonts::FreeSansBold12pt7b);
  c.text(title, 120, 52, t.text, middle_center);

  if (app.page == Page::Theme) {
    c.font(&fonts::FreeSansBold18pt7b);
    c.text(t.name, 120, 98, t.text, middle_center);
    c.roundRect(52, 124, 40, 24, 6, t.focus);
    c.roundRect(100, 124, 40, 24, 6, t.rest);
    c.roundRect(148, 124, 40, 24, 6, t.text);
  } else {
    char num[8];
    snprintf(num, sizeof num, "%d", app.page == Page::Focus ? app.settings.focusMin : app.settings.breakMin);
    bigText(c, num, 110, app.page == Page::Focus ? t.focus : t.rest);
    c.font(&fonts::FreeSansBold9pt7b);
    c.text("minutes", 120, 146, t.dim, middle_center);
  }

  c.font(&fonts::FreeSansBold9pt7b);
  c.text(app.page == Page::Theme ? "Tap: next   2x tap: back" : "Tap: +5   2x tap: -5", 120, 176, t.text, middle_center);
  c.text(app.page == Page::Theme ? "Hold: done" : "Hold: next", 120, 198, t.dim, middle_center);
  for (int i = 0; i < 3; i++) {
    const int x = 120 + (i - 1) * 16;
    if (i == static_cast<int>(app.page)) c.circle(x, 224, 4, t.text);
    else c.ring(x, 224, 4, t.dim);
  }
}

void render(Print* dump = nullptr) {
  nmtv::renderBands(
      band,
      [](Canvas& c) {
        if (app.inSettings) drawSettings(c);
        else drawTimer(c);
      },
      dump);
}

void apply(Gesture g) {
  if (app.handle(g, clockMs)) saveSettings();
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
  } else if (line == "tap") {
    apply(Gesture::Tap);
  } else if (line == "double") {
    apply(Gesture::DoubleTap);
  } else if (line == "long") {
    apply(Gesture::LongPress);
  } else if (line.startsWith("theme ")) {
    app.settings.theme = constrain(line.substring(6).toInt(), 0, pomo::kThemeCount - 1);
    saveSettings();
  } else if (line.startsWith("speed ")) {
    speed = max(1L, line.substring(6).toInt());
  } else if (line.startsWith("view ")) {
    char name[12] = {};
    float p = 0;
    sscanf(line.c_str() + 5, "%11s %f", name, &p);
    const String n(name);
    const Phase phase = n == "focus"       ? Phase::Focus
                        : n == "break"     ? Phase::Break
                        : n == "focusdone" ? Phase::FocusDone
                        : n == "breakdone" ? Phase::BreakDone
                                           : Phase::Ready;
    const int minutes = phase == Phase::Break ? app.settings.breakMin : app.settings.focusMin;
    const uint32_t dur = phase == Phase::FocusDone || phase == Phase::BreakDone ? pomo::Timer::kFlashMs : minutes * 60000u;
    app.inSettings = false;
    app.timer.force(phase, dur, p, clockMs);
  } else if (line == "state") {
    Serial.printf("phase %d paused %d remaining %lu completed %d settings %d focus %d break %d theme %d\n",
                  (int)app.timer.phase(), app.timer.paused(), (unsigned long)app.timer.remainingMs(clockMs),
                  app.timer.completed(), app.inSettings, app.settings.focusMin, app.settings.breakMin,
                  app.settings.theme);
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
  loadSettings();
  lastRealMs = millis();
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();
  clockMs += (now - lastRealMs) * speed;
  lastRealMs = now;

  handleSerial();

  const auto event = touch.poll(now);
  if (event == TouchPad::Event::Reset) {
    app = pomo::App();
    saveSettings();
  }
  apply(gestures.feed(event == TouchPad::Event::Tap, touch.heldFor(now), now));
  app.timer.update(app.settings, clockMs);

  if (now - lastFrameMs >= kFrameMs) {
    lastFrameMs = now;
    render();
  }
  delay(5);
}
