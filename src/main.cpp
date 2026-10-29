#include <Arduino.h>
#include <time.h>

#include "board.h"
#include "display.h"
#include "net.h"
#include "piglogic.h"
#include "scene.h"
#include "touch.h"

using piglogic::Mood;

namespace {

constexpr uint32_t kFrameMs = 50;
constexpr uint32_t kRefreshMs = 20UL * 60 * 1000;
constexpr uint32_t kRetryMs = 60UL * 1000;
constexpr uint32_t kStaleMs = 3UL * 60 * 60 * 1000;
constexpr uint32_t kTomorrowMs = 15000;
constexpr uint32_t kNightPeekMs = 30000;
constexpr uint32_t kInfoMs = 30000;
constexpr int kNightStartHour = 20;
constexpr int kNightEndHour = 7;
constexpr uint8_t kDayBrightness = 180;
constexpr uint8_t kNightBrightness = 18;

enum class Mode { Home, Tomorrow, Info };

LGFX lcd;
Scene scene(lcd);
TouchPad touch;
SceneState st;

piglogic::Forecast forecast{};
bool haveForecast = false;
bool fetchFailed = false;
uint32_t lastFetchOkMs = 0;
uint32_t nextFetchMs = 0;
int clockOffsetSec = INT32_MIN;

Mode mode = Mode::Home;
uint32_t modeUntilMs = 0;
uint8_t brightness = 0;

int forcedMood = -1;
int forcedView = -1;

const char* const kViewNames[] = {"today", "tomorrow", "night", "info", "setup", "loading", "offline"};

void setBrightness(uint8_t b) {
  if (b == brightness) return;
  brightness = b;
  lcd.setBrightness(b);
}

void fetchNow(uint32_t now) {
  piglogic::Forecast f{};
  if (net::fetchForecast(f)) {
    forecast = f;
    haveForecast = true;
    fetchFailed = false;
    lastFetchOkMs = now;
    nextFetchMs = now + kRefreshMs;
    if (f.utcOffsetSec != clockOffsetSec) {
      clockOffsetSec = f.utcOffsetSec;
      configTime(clockOffsetSec, 0, "pool.ntp.org", "time.nist.gov");
    }
    Serial.printf("[main] forecast %s now=%.1fF today=%s tomorrow=%s\n", f.date, f.nowF,
                  piglogic::moodName(piglogic::classify(f.days[0])), piglogic::moodName(piglogic::classify(f.days[1])));
  } else {
    fetchFailed = true;
    nextFetchMs = now + kRetryMs;
    Serial.println("[main] forecast fetch failed");
  }
}

void fillInfo(uint32_t now) {
  const Settings& cfg = net::settings();
  snprintf(st.info[0], sizeof st.info[0], "Hi, %s!", cfg.name);
  snprintf(st.info[1], sizeof st.info[1], "Place: %s", net::placeName()[0] ? net::placeName() : "?");
  snprintf(st.info[2], sizeof st.info[2], "Setup: %s", net::ip().c_str());
  if (haveForecast) {
    snprintf(st.info[3], sizeof st.info[3], "Updated %lu min ago", (unsigned long)((now - lastFetchOkMs) / 60000));
  } else {
    snprintf(st.info[3], sizeof st.info[3], "No weather yet");
  }
  snprintf(st.info[4], sizeof st.info[4], "Units: %s", cfg.celsius ? "Celsius" : "Fahrenheit");
}

void buildState(uint32_t now) {
  const Settings& cfg = net::settings();
  struct tm tmv;
  const bool haveTime = getLocalTime(&tmv, 0);
  st.clock[0] = '\0';
  if (haveTime) {
    const int h12 = tmv.tm_hour % 12 == 0 ? 12 : tmv.tm_hour % 12;
    snprintf(st.clock, sizeof st.clock, "%d:%02d", h12, tmv.tm_min);
  }
  const bool night = haveTime && (tmv.tm_hour >= kNightStartHour || tmv.tm_hour < kNightEndHour);

  piglogic::Forecast f = forecast;
  bool have = haveForecast;
  if (forcedMood >= 0 && !have) {
    f = piglogic::Forecast{"2026-01-01", 0, 72, 0, true, {{0, 78, 58, 10, 5, 0}, {0, 75, 55, 20, 5, 0}}};
    have = true;
  }

  View v;
  if (forcedView >= 0) v = static_cast<View>(forcedView);
  else if (!net::online()) v = View::Setup;
  else if (mode == Mode::Info) v = View::Info;
  else if (!have) v = fetchFailed ? View::Offline : View::Loading;
  else if (now - lastFetchOkMs > kStaleMs && forcedMood < 0) v = View::Offline;
  else if (mode == Mode::Tomorrow) v = View::Tomorrow;
  else v = night ? View::Night : View::Today;

  st.view = v;
  st.haveWeather = have;
  st.qr = v == View::Setup ? kApQr : nullptr;

  const bool useTomorrow = v == View::Tomorrow || v == View::Night;
  const piglogic::DayWeather& d = f.days[useTomorrow ? 1 : 0];
  st.mood = forcedMood >= 0 ? static_cast<Mood>(forcedMood) : piglogic::classify(d);
  st.hi = piglogic::toDisplayTemp(d.maxF, cfg.celsius);
  st.lo = piglogic::toDisplayTemp(d.minF, cfg.celsius);
  st.precip = d.precipPct;
  if (v == View::Today) {
    st.bigTemp = piglogic::toDisplayTemp(f.nowF, cfg.celsius);
    st.bigLabel = "now";
  } else {
    st.bigTemp = st.hi;
    st.bigLabel = v == View::Night ? "tmrw" : "high";
  }

  const uint32_t seed = piglogic::seedFromDate(f.date);
  switch (v) {
    case View::Today:
    case View::Tomorrow:
      piglogic::formatMessage(st.mood, v == View::Tomorrow, cfg.name, seed + (v == View::Tomorrow), st.message,
                              sizeof st.message);
      break;
    case View::Night:
      snprintf(st.message, sizeof st.message, "Sleep tight, %s! Tap to see tomorrow.", cfg.name);
      break;
    case View::Loading:
      snprintf(st.message, sizeof st.message, "Oink! Checking the sky...");
      break;
    case View::Offline:
      snprintf(st.message, sizeof st.message, "Oh no! I can't reach the internet.");
      break;
    case View::Setup:
      snprintf(st.message, sizeof st.message, "Hi! I'm your weather pig. Scan me to set up WiFi!");
      break;
    case View::Info:
      fillInfo(now);
      break;
  }

  const uint32_t held = touch.heldFor(now);
  st.resetProgress = (v == View::Info && held > TouchPad::kHoldMs)
                         ? (float)(held - TouchPad::kHoldMs) / (TouchPad::kLongHoldMs - TouchPad::kHoldMs)
                         : -1;

  const bool peeking = mode != Mode::Home;
  setBrightness(night && !peeking && forcedView < 0 ? kNightBrightness : kDayBrightness);
}

void setMode(Mode m, uint32_t now, uint32_t durationMs) {
  mode = m;
  modeUntilMs = m == Mode::Home ? 0 : now + durationMs;
}

void handleTouch(uint32_t now) {
  switch (touch.poll(now)) {
    case TouchPad::Event::Tap:
      st.bounceAtMs = now;
      if (mode == Mode::Home) setMode(Mode::Tomorrow, now, st.view == View::Night ? kNightPeekMs : kTomorrowMs);
      else setMode(Mode::Home, now, 0);
      break;
    case TouchPad::Event::Hold:
      setMode(Mode::Info, now, kInfoMs);
      break;
    case TouchPad::Event::LongHold:
      st = SceneState{};
      st.view = View::Loading;
      snprintf(st.message, sizeof st.message, "Resetting WiFi... See you soon!");
      scene.render(st, now);
      net::resetAll();
      break;
    case TouchPad::Event::None:
      break;
  }
  if (modeUntilMs && now > modeUntilMs && !touch.pressed()) setMode(Mode::Home, now, 0);
}

int indexOf(const char* const* names, int count, const String& name) {
  for (int i = 0; i < count; i++)
    if (name == names[i]) return i;
  return -1;
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
  } else if (line.startsWith("mood ")) {
    const String arg = line.substring(5);
    forcedMood = -1;
    for (int i = 0; i < piglogic::kMoodCount; i++)
      if (arg == piglogic::moodName(static_cast<Mood>(i))) forcedMood = i;
    Serial.printf("forced mood %d\n", forcedMood);
  } else if (line.startsWith("view ")) {
    forcedView = indexOf(kViewNames, 7, line.substring(5));
    Serial.printf("forced view %d\n", forcedView);
  } else if (line == "fetch") {
    fetchNow(now);
  }
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  pinMode(board::kTftPower, OUTPUT);
  digitalWrite(board::kTftPower, LOW);
  delay(20);

  lcd.init();
  setBrightness(kDayBrightness);
  if (!scene.begin()) Serial.println("[main] sprite alloc failed");
  touch.begin(board::kTouch);

  st.view = View::Loading;
  snprintf(st.message, sizeof st.message, "Oink! Waking up...");
  scene.render(st, millis());

  net::begin();
}

void loop() {
  static uint32_t lastFrameMs = 0;
  const uint32_t now = millis();

  net::loop();
  handleSerial(now);
  handleTouch(now);

  if (net::takeSettingsChanged()) nextFetchMs = now;
  if (net::online() && (int32_t)(now - nextFetchMs) >= 0) fetchNow(now);

  if (now - lastFrameMs >= kFrameMs) {
    lastFrameMs = now;
    buildState(now);
    scene.render(st, now);
  }
  delay(5);
}
