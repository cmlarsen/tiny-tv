#include "statslogic.h"

#include <ArduinoJson.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace statslogic {

namespace {

float clampPct(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

}  // namespace

bool parseStats(const char* json, size_t len, Stats& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  if (!doc["cpu"].is<float>() || !doc["mem"].is<float>()) return false;

  Stats s{};
  strncpy(s.host, doc["host"] | "?", sizeof s.host - 1);
  s.darwin = strcmp(doc["os"] | "", "darwin") == 0;
  s.cpu = clampPct(doc["cpu"]);
  s.mem = clampPct(doc["mem"]);
  s.memUsedGb = doc["mem_used_gb"] | 0.0f;
  s.memTotalGb = doc["mem_total_gb"] | 0.0f;
  s.cores = doc["cores"] | 0;
  for (int i = 0; i < 3; i++) s.load[i] = doc["load"][i] | 0.0f;
  s.ts = doc["ts"] | 0u;
  out = s;
  return true;
}

Level levelFor(float pct) {
  if (pct >= 85) return Level::Hot;
  if (pct >= 60) return Level::Busy;
  return Level::Calm;
}

int parseEndpoints(const char* list, Endpoint* out, int max) {
  int n = 0;
  const char* p = list ? list : "";
  while (*p && n < max) {
    const char* end = strchr(p, ',');
    if (!end) end = p + strlen(p);

    const char* a = p;
    const char* b = end;
    while (a < b && isspace(static_cast<unsigned char>(*a))) a++;
    while (b > a && isspace(static_cast<unsigned char>(b[-1]))) b--;
    if (b - a > 7 && strncmp(a, "http://", 7) == 0) a += 7;
    const char* slash = static_cast<const char*>(memchr(a, '/', b - a));
    if (slash) b = slash;

    const char* colon = static_cast<const char*>(memchr(a, ':', b - a));
    const char* hostEnd = colon ? colon : b;
    long port = kDefaultPort;
    bool ok = hostEnd > a && static_cast<size_t>(hostEnd - a) < sizeof out[0].host;
    if (ok && colon) {
      char digits[8] = {};
      const size_t dl = b - colon - 1;
      ok = dl > 0 && dl < sizeof digits;
      if (ok) {
        memcpy(digits, colon + 1, dl);
        char* stop = nullptr;
        port = strtol(digits, &stop, 10);
        ok = *stop == '\0' && port > 0 && port <= 65535;
      }
    }
    if (ok) {
      Endpoint& e = out[n++];
      memcpy(e.host, a, hostEnd - a);
      e.host[hostEnd - a] = '\0';
      e.port = static_cast<uint16_t>(port);
    }
    p = *end ? end + 1 : end;
  }
  return n;
}

void formatAge(uint32_t seconds, char* out, size_t len) {
  if (seconds < 60) snprintf(out, len, "%lus", static_cast<unsigned long>(seconds));
  else if (seconds < 3600) snprintf(out, len, "%lum", static_cast<unsigned long>(seconds / 60));
  else if (seconds < 86400) snprintf(out, len, "%luh", static_cast<unsigned long>(seconds / 3600));
  else snprintf(out, len, "%lud", static_cast<unsigned long>(seconds / 86400));
}

}  // namespace statslogic
