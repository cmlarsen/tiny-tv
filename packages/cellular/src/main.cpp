#include <Arduino.h>

#include "board.h"
#include "canvas.h"
#include "display.h"
#include "touch.h"
#include "zoo.h"

namespace {

constexpr int kBandH = 40;
constexpr uint32_t kFrameUs = 1000000 / 30;
constexpr uint8_t kBrightness = 200;
constexpr uint16_t kPill = nmtv::rgb(8, 8, 16);
constexpr uint16_t kTitle = nmtv::rgb(250, 250, 250);
constexpr uint16_t kSubtitle = nmtv::rgb(160, 160, 180);

LGFX lcd;
TouchPad touch;
LGFX_Sprite bands[2] = {LGFX_Sprite(&lcd), LGFX_Sprite(&lcd)};
ca::Zoo* zoo = nullptr;

TaskHandle_t worker = nullptr;
TaskHandle_t caller = nullptr;
ca::RowFn jobFn = nullptr;
void* jobCtx = nullptr;
int jobY0 = 0, jobY1 = 0, jobResult = 0;

void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    jobResult = jobFn(jobCtx, jobY0, jobY1);
    xTaskNotifyGive(caller);
  }
}

int dualCoreRows(ca::RowFn fn, void* ctx, int rows) {
  jobFn = fn;
  jobCtx = ctx;
  jobY0 = rows / 2;
  jobY1 = rows;
  caller = xTaskGetCurrentTaskHandle();
  xTaskNotifyGive(worker);
  const int mine = fn(ctx, 0, rows / 2);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  return mine + jobResult;
}

void drawLabel(nmtv::Canvas& c) {
  c.roundRect(12, 184, 216, 46, 14, kPill);
  c.font(&fonts::FreeSansBold9pt7b);
  c.text(zoo->mode().name(), 120, 198, kTitle, middle_center);
  c.font(&fonts::FreeSans9pt7b);
  c.text(zoo->mode().variantName(), 120, 218, kSubtitle, middle_center);
}

void draw(bool label, Print* dump = nullptr) {
  ca::Mode& mode = zoo->mode();
  lcd.startWrite();
  int b = 0;
  for (int y0 = 0; y0 < nmtv::kScreen; y0 += kBandH, b ^= 1) {
    LGFX_Sprite& band = bands[b];
    uint16_t* buf = static_cast<uint16_t*>(band.getBuffer());
    for (int y = 0; y < kBandH; y++) mode.renderRow(y0 + y, buf + y * nmtv::kScreen);
    if (label && y0 + kBandH > 184) {
      nmtv::Canvas c{band, y0};
      drawLabel(c);
    }
    lcd.pushImageDMA(0, y0, nmtv::kScreen, kBandH, reinterpret_cast<const lgfx::swap565_t*>(buf));
    if (dump) dump->write(reinterpret_cast<const uint8_t*>(buf), nmtv::kScreen * kBandH * 2);
  }
  lcd.endWrite();
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  const uint32_t now = millis();
  if (line == "shot") {
    Serial.print("\nSHOT\n");
    draw(zoo->labelVisible(millis()), &Serial);
    Serial.print("\nENDSHOT\n");
  } else if (line == "perf") {
    uint32_t t0 = micros();
    zoo->mode().tick();
    const uint32_t tickUs = micros() - t0;
    t0 = micros();
    draw(false);
    const uint32_t drawUs = micros() - t0;
    Serial.printf("%s: tick %lu us, draw %lu us, heap %lu free, largest block %lu\n", zoo->mode().name(),
                  (unsigned long)tickUs, (unsigned long)drawUs, (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMaxAllocHeap());
  } else if (line.startsWith("mode ")) {
    const int space = line.indexOf(' ', 5);
    const int variant = space > 0 ? line.substring(space + 1).toInt() : -1;
    zoo->select(constrain(line.substring(5).toInt(), 0, ca::kModes - 1), variant, now);
  } else if (line == "next") {
    zoo->next(now);
  } else if (line == "variant") {
    zoo->nextVariant(now);
  } else if (line.startsWith("ticks ")) {
    for (int i = line.substring(6).toInt(); i > 0; i--) {
      zoo->mode().tick();
      delay(1);
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  nmtv::beginDisplay(lcd);
  lcd.setBrightness(kBrightness);
  lcd.fillScreen(0);
  for (auto& band : bands) {
    band.setColorDepth(16);
    if (!band.createSprite(nmtv::kScreen, kBandH)) Serial.println("[main] band alloc failed");
  }
  touch.begin(board::kTouch);
  if (xTaskCreatePinnedToCore(workerTask, "rows", 4096, nullptr, 2, &worker, 0) == pdPASS)
    ca::setRowRunner(dualCoreRows);
  else
    Serial.println("[main] worker task failed, running on one core");
  zoo = new ca::Zoo(esp_random());
  zoo->begin(millis());
  Serial.printf("[main] heap %lu free, largest block %lu\n", (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMaxAllocHeap());
}

void loop() {
  static uint32_t lastFrameUs = micros();
  handleSerial();
  const uint32_t now = millis();
  if (touch.poll(now) == TouchPad::Event::Tap) zoo->next(now);
  const uint32_t elapsed = micros() - lastFrameUs;
  if (elapsed < kFrameUs) delay((kFrameUs - elapsed) / 1000);
  lastFrameUs = micros();
  zoo->mode().tick();
  draw(zoo->labelVisible(now));
}
