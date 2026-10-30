#include <Arduino.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>

#include "board.h"
#include "canvas.h"
#include "clip.h"
#include "display.h"
#include "fire.h"
#include "touch.h"

namespace {

constexpr int kBandH = 40;
constexpr uint32_t kStepUs = 1000000 / 60;
constexpr int kMaxCatchUp = 4;
constexpr uint8_t kBrightness = 200;
constexpr int kVideo = fire::kLevels;
constexpr uint32_t kPauseMs = 30000;

LGFX lcd;
TouchPad touch;
JPEGDEC jpeg;
fire::Sim* sim = nullptr;
uint16_t pal[256];
uint16_t* bufs[2];
int bufIdx = 0;
Print* dumpTo = nullptr;
uint16_t* dumpRow = nullptr;
clip::Clip video;
bool haveVideo = false;
int frameIdx = 0;
uint32_t lastFrameUs = 0;
uint32_t pausedAtMs = 0;
bool paused = false;
int mode = 1;
fire::Params params = fire::level(1);

uint8_t toward(uint8_t v, uint8_t goal) { return v < goal ? v + 1 : (v > goal ? v - 1 : v); }

void ease() {
  const fire::Params goal = fire::level(mode == kVideo ? 1 : mode);
  params.fuel = toward(params.fuel, goal.fuel);
  params.cooling = toward(params.cooling, goal.cooling);
}

void drawSim(Print* dump) {
  lcd.startWrite();
  int b = 0;
  for (int y0 = 0; y0 < nmtv::kScreen; y0 += kBandH, b ^= 1) {
    uint16_t* buf = bufs[b];
    for (int y = 0; y < kBandH; y++) fire::renderRow(*sim, y0 + y, pal, buf + y * nmtv::kScreen);
    lcd.pushImageDMA(0, y0, nmtv::kScreen, kBandH, reinterpret_cast<const lgfx::swap565_t*>(buf));
    if (dump) dump->write(reinterpret_cast<const uint8_t*>(buf), nmtv::kScreen * kBandH * 2);
  }
  lcd.endWrite();
}

int onJpegStrip(JPEGDRAW* d) {
  uint16_t* buf = bufs[bufIdx];
  bufIdx ^= 1;
  const size_t bytes = static_cast<size_t>(d->iWidth) * d->iHeight * 2;
  memcpy(buf, d->pPixels, bytes);
  lcd.pushImageDMA(d->x, d->y, d->iWidth, d->iHeight, reinterpret_cast<const lgfx::swap565_t*>(buf));
  if (dumpTo && dumpRow) {
    for (int y = 0; y < d->iHeight; y++)
      memcpy(dumpRow + y * nmtv::kScreen + d->x, d->pPixels + y * d->iWidth, d->iWidth * 2);
    if (d->x + d->iWidth >= nmtv::kScreen)
      dumpTo->write(reinterpret_cast<const uint8_t*>(dumpRow), nmtv::kScreen * d->iHeight * 2);
  }
  return 1;
}

bool drawFrame(int i, Print* dump) {
  size_t len;
  const uint8_t* data = video.frame(i, len);
  if (!jpeg.openRAM(const_cast<uint8_t*>(data), static_cast<int>(len), onJpegStrip)) return false;
  jpeg.setPixelType(RGB565_BIG_ENDIAN);
  dumpTo = dump;
  if (dump) dumpRow = static_cast<uint16_t*>(malloc(nmtv::kScreen * 16 * 2));
  lcd.startWrite();
  const bool ok = jpeg.decode(0, 0, 0);
  lcd.endWrite();
  free(dumpRow);
  dumpRow = nullptr;
  dumpTo = nullptr;
  jpeg.close();
  return ok;
}

void loadVideo() {
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "clip");
  if (!part) {
    Serial.println("[clip] no clip partition");
    return;
  }
  const void* p = nullptr;
  spi_flash_mmap_handle_t handle;
  if (esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, &p, &handle) != ESP_OK) {
    Serial.println("[clip] mmap failed");
    return;
  }
  const uint32_t t0 = millis();
  const clip::Status st = clip::parse(static_cast<const uint8_t*>(p), part->size, video);
  if (st == clip::Status::Ok) {
    haveVideo = true;
    Serial.printf("[clip] %u frames at %u fps, checked in %lu ms\n", video.count, video.fps,
                  (unsigned long)(millis() - t0));
  } else {
    spi_flash_munmap(handle);
    Serial.println(st == clip::Status::Missing ? "[clip] none flashed" : "[clip] corrupt, reflash it");
  }
}

void nextMode() {
  const int modes = haveVideo ? fire::kLevels + 1 : fire::kLevels;
  mode = (mode + 1) % modes;
}

void draw(Print* dump = nullptr) {
  if (mode == kVideo) drawFrame(frameIdx, dump);
  else drawSim(dump);
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line == "shot") {
    Serial.print("\nSHOT\n");
    draw(&Serial);
    Serial.print("\nENDSHOT\n");
  } else if (line == "perf") {
    uint32_t t0 = micros();
    sim->step(params);
    const uint32_t stepUs = micros() - t0;
    t0 = micros();
    drawSim(nullptr);
    const uint32_t simUs = micros() - t0;
    uint32_t videoUs = 0;
    if (haveVideo) {
      t0 = micros();
      drawFrame(frameIdx, nullptr);
      videoUs = micros() - t0;
    }
    Serial.printf("step %lu us, sim draw %lu us, video frame %lu us, heap %lu free\n", (unsigned long)stepUs,
                  (unsigned long)simUs, (unsigned long)videoUs, (unsigned long)ESP.getFreeHeap());
  } else if (line.startsWith("level ")) {
    mode = constrain(line.substring(6).toInt(), 0, fire::kLevels - 1);
    params = fire::level(mode);
  } else if (line == "video") {
    paused = false;
    if (haveVideo) mode = kVideo;
  } else if (line.startsWith("frame ")) {
    if (haveVideo) {
      mode = kVideo;
      paused = true;
      pausedAtMs = millis();
      frameIdx = constrain(line.substring(6).toInt(), 0, video.count - 1);
    }
  } else if (line.startsWith("steps ")) {
    for (int i = line.substring(6).toInt(); i > 0; i--) sim->step(params);
  }
}

void tickSim() {
  static uint32_t lastStepUs = micros();
  const uint32_t now = micros();
  int n = 0;
  while (now - lastStepUs >= kStepUs && n < kMaxCatchUp) {
    sim->step(params);
    lastStepUs += kStepUs;
    n++;
  }
  if (n == kMaxCatchUp) lastStepUs = now;
}

void tickVideo() {
  const uint32_t frameUs = 1000000 / video.fps;
  while (micros() - lastFrameUs < frameUs) {
  }
  lastFrameUs = micros();
  if (paused && millis() - pausedAtMs >= kPauseMs) paused = false;
  if (!paused) frameIdx = (frameIdx + 1) % video.count;
}

}  // namespace

void setup() {
  Serial.begin(230400);
  Serial.setTimeout(50);
  nmtv::beginDisplay(lcd);
  lcd.setBrightness(kBrightness);
  lcd.fillScreen(0);
  for (auto& buf : bufs) {
    buf = static_cast<uint16_t*>(heap_caps_malloc(nmtv::kScreen * kBandH * 2, MALLOC_CAP_DMA));
    if (!buf) Serial.println("[main] band alloc failed");
  }
  sim = new fire::Sim(esp_random());
  fire::buildPalette(pal);
  touch.begin(board::kTouch);
  loadVideo();
  mode = haveVideo ? kVideo : 1;
}

void loop() {
  handleSerial();
  if (touch.poll(millis()) == TouchPad::Event::Tap) {
    paused = false;
    nextMode();
  }
  ease();
  if (mode == kVideo) tickVideo();
  else tickSim();
  draw();
}
