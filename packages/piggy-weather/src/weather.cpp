#include "weather.h"

#include "net.h"

namespace weather {
namespace {

constexpr uint32_t kRefreshMs = 20UL * 60 * 1000;
constexpr uint32_t kRetryMs = 60UL * 1000;
constexpr uint32_t kOfflinePollMs = 1000;

SemaphoreHandle_t lock;
TaskHandle_t handle;
Snapshot current{};

void fetchOnce() {
  piglogic::Forecast f{};
  const bool ok = net::fetchForecast(f);
  xSemaphoreTake(lock, portMAX_DELAY);
  if (ok) {
    current.have = true;
    current.failed = false;
    current.lastOkMs = millis();
    current.forecast = f;
    snprintf(current.place, sizeof current.place, "%s", net::placeName());
  } else {
    current.failed = true;
  }
  xSemaphoreGive(lock);
  if (ok) {
    Serial.printf("[weather] %s now=%.1fF today=%s tomorrow=%s\n", f.date, f.nowF,
                  piglogic::moodName(piglogic::classify(f.days[0])), piglogic::moodName(piglogic::classify(f.days[1])));
  } else {
    Serial.println("[weather] fetch failed");
  }
}

void task(void*) {
  for (;;) {
    uint32_t waitMs = kOfflinePollMs;
    if (net::online()) {
      fetchOnce();
      waitMs = current.failed ? kRetryMs : kRefreshMs;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
  }
}

}  // namespace

void begin() {
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "weather", 8192, nullptr, 1, &handle, 0);
}

void refreshNow() { xTaskNotifyGive(handle); }

Snapshot snapshot() {
  xSemaphoreTake(lock, portMAX_DELAY);
  const Snapshot s = current;
  xSemaphoreGive(lock);
  return s;
}

}  // namespace weather
