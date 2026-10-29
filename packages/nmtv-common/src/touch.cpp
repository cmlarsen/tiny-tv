#include "touch.h"

void TouchPad::begin(uint8_t pin) {
  pin_ = pin;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += touchRead(pin_);
  logic_.begin(sum / 16);
}

uint32_t TouchPad::read() const { return (touchRead(pin_) + touchRead(pin_)) / 2; }
