#include "fleet.h"

#include <HTTPClient.h>
#include <WiFi.h>

namespace fleet {
namespace {

constexpr int kTimeoutMs = 1500;
constexpr uint8_t kFailuresBeforeOffline = 2;

SemaphoreHandle_t lock;
Machine machines[kMaxMachines];
int count = 0;
char pendingList[160];
bool pending = false;

void apply(const char* list) {
  statslogic::Endpoint eps[kMaxMachines];
  const int n = statslogic::parseEndpoints(list, eps, kMaxMachines);
  for (int i = 0; i < n; i++) {
    machines[i] = Machine{};
    machines[i].ep = eps[i];
    snprintf(machines[i].stats.host, sizeof machines[i].stats.host, "%s", eps[i].host);
  }
  count = n;
}

bool fetch(const statslogic::Endpoint& ep, statslogic::Stats& out) {
  char url[64];
  snprintf(url, sizeof url, "http://%s:%u/", ep.host, ep.port);
  HTTPClient http;
  http.setConnectTimeout(kTimeoutMs);
  http.setTimeout(kTimeoutMs);
  if (!http.begin(url)) return false;
  const int code = http.GET();
  bool ok = false;
  if (code == 200) {
    const String body = http.getString();
    ok = statslogic::parseStats(body.c_str(), body.length(), out);
  }
  http.end();
  return ok;
}

void pollOnce() {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (pending) {
    apply(pendingList);
    pending = false;
  }
  const int n = count;
  statslogic::Endpoint eps[kMaxMachines];
  for (int i = 0; i < n; i++) eps[i] = machines[i].ep;
  xSemaphoreGive(lock);

  for (int i = 0; i < n; i++) {
    statslogic::Stats s{};
    const bool ok = fetch(eps[i], s);
    xSemaphoreTake(lock, portMAX_DELAY);
    if (i < count && strcmp(machines[i].ep.host, eps[i].host) == 0) {
      Machine& m = machines[i];
      if (ok) {
        m.stats = s;
        m.everSeen = true;
        m.online = true;
        m.failures = 0;
        m.lastOkMs = millis();
        m.cpu.push(s.cpu);
        m.mem.push(s.mem);
      } else if (++m.failures >= kFailuresBeforeOffline) {
        m.online = false;
      }
    }
    xSemaphoreGive(lock);
  }
}

void task(void*) {
  for (;;) {
    const uint32_t start = millis();
    if (WiFi.status() == WL_CONNECTED) pollOnce();
    const uint32_t spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < kPollMs ? kPollMs - spent : 100));
  }
}

}  // namespace

void begin(const char* endpointList) {
  lock = xSemaphoreCreateMutex();
  apply(endpointList);
  xTaskCreatePinnedToCore(task, "fleet", 8192, nullptr, 1, nullptr, 0);
}

void reconfigure(const char* endpointList) {
  xSemaphoreTake(lock, portMAX_DELAY);
  snprintf(pendingList, sizeof pendingList, "%s", endpointList);
  pending = true;
  xSemaphoreGive(lock);
}

int snapshot(Machine* out, int max) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const int n = count < max ? count : max;
  for (int i = 0; i < n; i++) out[i] = machines[i];
  xSemaphoreGive(lock);
  return n;
}

}  // namespace fleet
