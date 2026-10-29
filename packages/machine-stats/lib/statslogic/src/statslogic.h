#pragma once

#include <stddef.h>
#include <stdint.h>

namespace statslogic {

constexpr uint16_t kDefaultPort = 9110;

struct Stats {
  char host[24];
  bool darwin;
  float cpu;
  float mem;
  float memUsedGb;
  float memTotalGb;
  int cores;
  float load[3];
  uint32_t ts;
};

enum class Level { Calm, Busy, Hot };

struct Endpoint {
  char host[40];
  uint16_t port;
};

bool parseStats(const char* json, size_t len, Stats& out);
Level levelFor(float pct);
int parseEndpoints(const char* list, Endpoint* out, int max);
void formatAge(uint32_t seconds, char* out, size_t len);

template <int N>
class History {
 public:
  void push(float v) {
    buf_[head_] = v;
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }
  int size() const { return count_; }
  float at(int i) const { return buf_[(head_ + N - count_ + i) % N]; }
  void clear() { head_ = count_ = 0; }

 private:
  float buf_[N] = {};
  int head_ = 0;
  int count_ = 0;
};

}  // namespace statslogic
