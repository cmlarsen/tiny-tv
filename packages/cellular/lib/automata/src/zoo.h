#pragma once

#include <stdint.h>

#include "mode.h"

namespace ca {

constexpr int kModes = 5;

Mode* makeMode(int index);

class Zoo {
 public:
  static constexpr uint32_t kLabelMs = 2500;

  explicit Zoo(uint32_t seed) : rng_(seed) {}
  ~Zoo() { delete mode_; }

  void begin(uint32_t nowMs) { select(0, -1, nowMs); }
  void select(int index, int variant, uint32_t nowMs);
  void next(uint32_t nowMs) { select((index_ + 1) % kModes, -1, nowMs); }
  void nextVariant(uint32_t nowMs);
  bool labelVisible(uint32_t nowMs) const { return nowMs - shownAtMs_ < kLabelMs; }

  Mode& mode() { return *mode_; }
  int index() const { return index_; }

 private:
  void restart(int variant, uint32_t nowMs);

  Mode* mode_ = nullptr;
  int index_ = -1;
  uint32_t shownAtMs_ = 0;
  Rng rng_;
};

}  // namespace ca
