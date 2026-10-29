#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "board.h"
#include "display.h"
#include "fleet.h"
#include "net.h"
#include "portal.h"
#include "scene.h"
#include "touch.h"

namespace {

constexpr uint32_t kFrameMs = 80;
constexpr uint32_t kSetupFrameMs = 1000;
constexpr uint32_t kDetailMs = 60000;
constexpr uint32_t kInfoMs = 30000;
constexpr uint8_t kBrightness = 150;

enum class Mode { Overview, Detail, Info };

LGFX lcd;
Scene scene(lcd);
TouchPad touch;
SceneState st;
Machine snapshot[kMaxMachines];

Mode mode = Mode::Overview;
int detailIndex = 0;
uint32_t modeUntilMs = 0;
bool clockStarted = false;
int forcedView = -1;
int forcedDetail = 0;

const char* const kViewNames[] = {"overview", "detail", "info", "setup", "connecting"};

void setMode(Mode m, uint32_t now, uint32_t durationMs) {
  static const char* const names[] = {"overview", "detail", "info"};
  Serial.printf("[mode] %s %d (count %d)\n", names[static_cast<int>(m)], detailIndex, st.count);
  mode = m;
  modeUntilMs = m == Mode::Overview ? 0 : now + durationMs;
}

void fillInfo() {
  snprintf(st.info[0], sizeof st.info[0], "IP: %s", nmtv::portal::ip().c_str());
  snprintf(st.info[1], sizeof st.info[1], "Settings: stats.local");
  snprintf(st.info[2], sizeof st.info[2], "Watching %d machines", st.count);
  snprintf(st.info[3], sizeof st.info[3], "Polling every %lus", (unsigned long)(kPollMs / 1000));
  snprintf(st.info[4], sizeof st.info[4], "Free heap: %luK", (unsigned long)(ESP.getFreeHeap() / 1024));
}

void buildState(uint32_t now) {
  st.count = fleet::snapshot(snapshot, kMaxMachines);
  st.machines = snapshot;

  struct tm tmv;
  st.clock[0] = '\0';
  if (clockStarted && getLocalTime(&tmv, 0)) {
    const int h12 = tmv.tm_hour % 12 == 0 ? 12 : tmv.tm_hour % 12;
    snprintf(st.clock, sizeof st.clock, "%d:%02d", h12, tmv.tm_min);
  }

  View v;
  if (forcedView >= 0) v = static_cast<View>(forcedView);
  else if (!nmtv::portal::online()) v = WiFi.getMode() & WIFI_AP ? View::Setup : View::Connecting;
  else if (mode == Mode::Info) v = View::Info;
  else if (mode == Mode::Detail) v = View::Detail;
  else v = View::Overview;

  st.view = v;
  st.detail = forcedView >= 0 ? forcedDetail : detailIndex;
  st.qr = v == View::Setup ? nmtv::portal::apQr() : nullptr;
  if (v == View::Info) fillInfo();

  const uint32_t held = touch.heldFor(now);
  st.resetProgress = (v == View::Info && held > TouchPad::kHoldMs)
                         ? (float)(held - TouchPad::kHoldMs) / (TouchPad::kLongHoldMs - TouchPad::kHoldMs)
                         : -1;
}

void handleTouch(uint32_t now) {
  static uint32_t lastLogMs = 0;
  const TouchPad::Event ev = touch.poll(now);
  if (ev != TouchPad::Event::None || now - lastLogMs >= 1000) {
    static const char* const names[] = {"-", "tap", "hold", "longhold"};
    Serial.printf("[touch] %s raw=%lu base=%lu pressed=%d\n", names[static_cast<int>(ev)],
                  (unsigned long)touch.lastReading(), (unsigned long)touch.baseline(), touch.pressed());
    lastLogMs = now;
  }
  switch (ev) {
    case TouchPad::Event::Tap:
      if (mode == Mode::Overview) {
        detailIndex = 0;
        if (st.count > 0) setMode(Mode::Detail, now, kDetailMs);
      } else if (mode == Mode::Detail && detailIndex + 1 < st.count) {
        detailIndex++;
        setMode(Mode::Detail, now, kDetailMs);
      } else {
        setMode(Mode::Overview, now, 0);
      }
      break;
    case TouchPad::Event::Hold:
      setMode(Mode::Info, now, kInfoMs);
      break;
    case TouchPad::Event::LongHold:
      nmtv::portal::resetAll();
      break;
    case TouchPad::Event::None:
      break;
  }
  if (modeUntilMs && now > modeUntilMs && !touch.pressed()) setMode(Mode::Overview, now, 0);
}

void handleSerial(uint32_t now) {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line == "shot") {
    buildState(now);
    Serial.print("\nSHOT\n");
    scene.render(st, now, &Serial);
    Serial.print("\nENDSHOT\n");
  } else if (line.startsWith("view ")) {
    String arg = line.substring(5);
    forcedDetail = 0;
    if (arg.startsWith("detail ")) {
      forcedDetail = arg.substring(7).toInt();
      arg = "detail";
    }
    forcedView = -1;
    for (int i = 0; i < 5; i++)
      if (arg == kViewNames[i]) forcedView = i;
    Serial.printf("forced view %d\n", forcedView);
  } else if (line == "perf") {
    buildState(now);
    const uint32_t t0 = micros();
    scene.render(st, now);
    Serial.printf("render %lu us (view %d)\n", (unsigned long)(micros() - t0), (int)st.view);
  } else if (line.startsWith("machines ")) {
    fleet::reconfigure(line.substring(9).c_str());
    Serial.println("machines updated until reboot");
  }
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  nmtv::beginDisplay(lcd);
  lcd.setBrightness(kBrightness);
  if (!scene.begin()) Serial.println("[main] sprite alloc failed");
  touch.begin(board::kTouch);
  scene.render(st, millis());

  net::begin();
  fleet::begin(net::settings().machines);
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();

  nmtv::portal::loop();
  if (nmtv::portal::online() && !clockStarted) {
    configTzTime(net::settings().tz, "pool.ntp.org", "time.nist.gov");
    clockStarted = true;
  }
  if (nmtv::portal::takeSettingsChanged()) {
    fleet::reconfigure(net::settings().machines);
    configTzTime(net::settings().tz, "pool.ntp.org", "time.nist.gov");
  }

  handleSerial(now);
  handleTouch(now);

  if (now - lastFrameMs >= (st.view == View::Setup ? kSetupFrameMs : kFrameMs)) {
    lastFrameMs = now;
    buildState(now);
    scene.render(st, now);
  }
  delay(5);
}
