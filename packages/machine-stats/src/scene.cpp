#include "scene.h"

#include <math.h>

#include "canvas.h"
#include "net.h"

using nmtv::Canvas;
using nmtv::rgb;
using statslogic::Level;

namespace {

constexpr int kW = nmtv::kScreen;
constexpr int kOverviewRows = 4;

constexpr uint16_t kBg = rgb(14, 16, 24);
constexpr uint16_t kCard = rgb(28, 32, 46);
constexpr uint16_t kTrack = rgb(48, 54, 72);
constexpr uint16_t kText = rgb(240, 242, 250);
constexpr uint16_t kDim = rgb(130, 138, 160);
constexpr uint16_t kCalm = rgb(70, 214, 132);
constexpr uint16_t kBusy = rgb(255, 186, 60);
constexpr uint16_t kHot = rgb(255, 86, 86);
constexpr uint16_t kCpuLine = rgb(80, 200, 255);
constexpr uint16_t kMemLine = rgb(196, 140, 255);
constexpr uint16_t kOffline = rgb(110, 116, 136);

uint16_t levelColor(float pct) {
  switch (statslogic::levelFor(pct)) {
    case Level::Calm: return kCalm;
    case Level::Busy: return kBusy;
    case Level::Hot: return kHot;
  }
  return kCalm;
}

void bar(Canvas& c, int x, int y, int w, int h, float pct, uint16_t col) {
  c.roundRect(x, y, w, h, h / 2, kTrack);
  const int fw = (int)(w * pct / 100.0f);
  if (fw >= 2) c.roundRect(x, y, fw, h, min(h / 2, fw / 2), col);
}

void percentText(Canvas& c, float pct, int x, int y, uint16_t col, textdatum_t datum) {
  char buf[8];
  snprintf(buf, sizeof buf, "%d%%", (int)lroundf(pct));
  c.text(buf, x, y, col, datum);
}

void statusDot(Canvas& c, int x, int y, const Machine& m) {
  c.circle(x, y, 4, m.online ? kCalm : (m.everSeen ? kHot : kOffline));
}

void offlineLabel(const Machine& m, uint32_t now, char* buf, size_t len) {
  if (!m.everSeen) {
    snprintf(buf, len, "no response");
    return;
  }
  char age[8];
  statslogic::formatAge((now - m.lastOkMs) / 1000, age, sizeof age);
  snprintf(buf, len, "offline - seen %s ago", age);
}

void overview(Canvas& c, const SceneState& s, const float* cpu, const float* mem, uint32_t now) {
  const int n = min(s.count, kOverviewRows);
  if (n == 0) {
    c.text("No machines configured", 120, 130, kDim, middle_center);
    return;
  }
  const int rowH = kW / n;
  for (int i = 0; i < n; i++) {
    const Machine& m = s.machines[i];
    const int y0 = i * rowH;
    c.roundRect(6, y0 + 2, 228, rowH - 4, 10, kCard);

    const int nameY = y0 + rowH * 22 / 100;
    statusDot(c, 18, nameY, m);
    c.font(&fonts::FreeSansBold12pt7b);
    c.text(m.stats.host, 28, nameY, m.online ? kText : kOffline, middle_left);

    c.font(&fonts::FreeSansBold9pt7b);
    if (!m.online) {
      char buf[32];
      offlineLabel(m, now, buf, sizeof buf);
      c.text(buf, 120, y0 + rowH * 62 / 100, kOffline, middle_center);
      continue;
    }

    const int cpuY = y0 + rowH * 55 / 100;
    const int memY = y0 + rowH * 82 / 100;
    c.text("CPU", 16, cpuY, kDim, middle_left);
    bar(c, 62, cpuY - 5, 112, 10, cpu[i], levelColor(cpu[i]));
    percentText(c, cpu[i], 228, cpuY, kText, middle_right);
    c.text("MEM", 16, memY, kDim, middle_left);
    bar(c, 62, memY - 5, 112, 10, mem[i], levelColor(mem[i]));
    percentText(c, mem[i], 228, memY, kText, middle_right);
  }
}

void gauge(Canvas& c, int x, int y, float pct, bool live, const char* label) {
  constexpr int r0 = 36, r1 = 46;
  c.arc(x, y, r0, r1, 0, 360, kTrack);
  if (live && pct > 0.5f) {
    const float end = 270 + 360 * pct / 100.0f;
    const uint16_t col = levelColor(pct);
    if (end <= 360) {
      c.arc(x, y, r0, r1, 270, end, col);
    } else {
      c.arc(x, y, r0, r1, 270, 360, col);
      c.arc(x, y, r0, r1, 0, end - 360, col);
    }
  }
  c.font(&fonts::FreeSansBold18pt7b);
  if (live) {
    char buf[8];
    snprintf(buf, sizeof buf, "%d", (int)lroundf(pct));
    c.text(buf, x, y - 2, kText, middle_center);
  } else {
    c.text("--", x, y - 2, kOffline, middle_center);
  }
  c.font(&fonts::FreeSansBold9pt7b);
  c.text(label, x, y + 24, kDim, middle_center);
}

template <int N>
void sparkline(Canvas& c, const statslogic::History<N>& h, int x, int y, int w, int ht, uint16_t col) {
  const int n = h.size();
  if (n < 2) return;
  float px = 0, py = 0;
  for (int i = 0; i < n; i++) {
    const float sx = x + w - (n - 1 - i) * (float)w / (N - 1);
    const float sy = y + ht - h.at(i) * ht / 100.0f;
    if (i) c.line(px, py, sx, sy, 1, col);
    px = sx;
    py = sy;
  }
}

void detail(Canvas& c, const SceneState& s, const float* cpu, const float* mem, uint32_t now) {
  if (s.count == 0) return;
  const int idx = s.detail % s.count;
  const Machine& m = s.machines[idx];

  statusDot(c, 14, 18, m);
  c.font(&fonts::FreeSansBold12pt7b);
  c.text(m.stats.host, 24, 18, m.online ? kText : kOffline, middle_left);
  gauge(c, 64, 92, cpu[idx], m.online, "CPU");
  gauge(c, 176, 92, mem[idx], m.online, "MEM");

  const int gx = 10, gy = 150, gw = 220, gh = 52;
  c.roundRect(gx - 4, gy - 4, gw + 8, gh + 8, 6, kCard);
  c.hline(gx, gy + gh / 2, gw, kTrack);
  sparkline(c, m.mem, gx, gy, gw, gh, kMemLine);
  sparkline(c, m.cpu, gx, gy, gw, gh, kCpuLine);

  c.font(&fonts::FreeSansBold9pt7b);
  char foot[40];
  if (m.online) {
    snprintf(foot, sizeof foot, "%.1f/%.0f GB", m.stats.memUsedGb, m.stats.memTotalGb);
    c.text(foot, 10, 222, kDim, middle_left);
    snprintf(foot, sizeof foot, "load %.0f / %dc", m.stats.load[0], m.stats.cores);
    c.text(foot, 230, 222, kDim, middle_right);
  } else {
    offlineLabel(m, now, foot, sizeof foot);
    c.text(foot, 10, 222, kDim, middle_left);
  }

  for (int i = 0; i < s.count; i++) {
    const int dx = 120 - (s.count - 1) * 6 + i * 12;
    c.circle(dx, 236, 2, i == idx ? kText : kTrack);
  }
}

void info(Canvas& c, const SceneState& s) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Stats HUD", 120, 24, kCpuLine, middle_center);
  c.font(&fonts::FreeSansBold9pt7b);
  for (int i = 0; i < 5; i++)
    if (s.info[i][0]) c.text(s.info[i], 12, 60 + i * 24, kText, middle_left);
  if (s.resetProgress >= 0) {
    c.text("Keep holding to reset WiFi", 120, 196, kBusy, middle_center);
    c.roundRect(20, 210, 200, 14, 7, kTrack);
    const int w = (int)(200 * min(1.0f, s.resetProgress));
    if (w > 14) c.roundRect(20, 210, w, 14, 7, kHot);
  } else {
    c.text("Tap to go back", 120, 214, kDim, middle_center);
  }
}

void setup(Canvas& c, const SceneState& s) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Set up WiFi", 120, 18, kText, middle_center);
  if (s.qr) {
    c.roundRect(70, 36, 100, 100, 8, rgb(255, 255, 255));
    c.qr(s.qr, 74, 40, 92);
  }
  c.font(&fonts::FreeSansBold9pt7b);
  c.text("1. Scan, or join WiFi", 120, 156, kDim, middle_center);
  c.text(kApName, 120, 176, kText, middle_center);
  c.text("2. Open in a browser", 120, 202, kDim, middle_center);
  c.text("192.168.4.1", 120, 222, kCpuLine, middle_center);
}

void connecting(Canvas& c, uint32_t now) {
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("Connecting...", 120, 110, kText, middle_center);
  for (int i = 0; i < 3; i++) {
    const bool on = (now / 300) % 3 == (uint32_t)i;
    c.circle(108 + i * 12, 140, 4, on ? kCpuLine : kTrack);
  }
}

}  // namespace

bool Scene::begin() {
  band_.setColorDepth(16);
  return band_.createSprite(kW, nmtv::kBandH) != nullptr;
}

void Scene::render(const SceneState& s, uint32_t now, Print* dump) {
  for (int i = 0; i < s.count && i < kMaxMachines; i++) {
    const Machine& m = s.machines[i];
    const float tc = m.online ? m.stats.cpu : 0;
    const float tm = m.online ? m.stats.mem : 0;
    shownCpu_[i] += (tc - shownCpu_[i]) * 0.25f;
    shownMem_[i] += (tm - shownMem_[i]) * 0.25f;
    if (dump) {
      shownCpu_[i] = tc;
      shownMem_[i] = tm;
    }
  }
  for (int oy = 0; oy < kW; oy += nmtv::kBandH) {
    Canvas c{band_, oy};
    band_.fillScreen(kBg);
    switch (s.view) {
      case View::Overview: overview(c, s, shownCpu_, shownMem_, now); break;
      case View::Detail: detail(c, s, shownCpu_, shownMem_, now); break;
      case View::Info: info(c, s); break;
      case View::Setup: setup(c, s); break;
      case View::Connecting: connecting(c, now); break;
    }
    band_.pushSprite(0, oy);
    if (dump) dump->write(static_cast<const uint8_t*>(band_.getBuffer()), kW * nmtv::kBandH * 2);
  }
}
