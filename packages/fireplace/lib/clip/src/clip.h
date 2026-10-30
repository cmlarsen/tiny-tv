#pragma once

#include <stddef.h>
#include <stdint.h>

namespace clip {

constexpr size_t kHeaderSize = 20;
constexpr uint16_t kSize = 240;

enum class Status { Ok, Missing, Corrupt };

struct Clip {
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t fps = 0;
  uint16_t count = 0;
  const uint8_t* offsets = nullptr;
  const uint8_t* data = nullptr;

  const uint8_t* frame(int i, size_t& len) const;
};

uint32_t crc32(const uint8_t* p, size_t len, uint32_t crc = 0);
Status parse(const uint8_t* p, size_t avail, Clip& out);

}  // namespace clip
