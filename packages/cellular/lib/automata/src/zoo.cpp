#include "zoo.h"

#include "modes.h"

namespace ca {

Mode* makeMode(int index) {
  switch (index) {
    case 0: return new Life();
    case 1: return new Cyclic();
    case 2: return new Physarum();
    case 3: return new Brain();
    default: return new GrayScott();
  }
}

void Zoo::select(int index, int variant, uint32_t nowMs) {
  if (index != index_ || !mode_) {
    delete mode_;
    mode_ = makeMode(index);
    index_ = index;
  }
  restart(variant < 0 ? rng_.below(mode_->variants()) : variant % mode_->variants(), nowMs);
}

void Zoo::nextVariant(uint32_t nowMs) { restart((mode_->variant() + 1) % mode_->variants(), nowMs); }

void Zoo::restart(int variant, uint32_t nowMs) {
  mode_->start(rng_.next(), variant);
  shownAtMs_ = nowMs;
}

}  // namespace ca
