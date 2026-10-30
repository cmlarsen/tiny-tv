#include "clip.h"

#include <string.h>

namespace clip {

namespace {

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

struct Table {
  uint32_t v[256];
  Table() {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      v[i] = c;
    }
  }
};

}  // namespace

const uint8_t* Clip::frame(int i, size_t& len) const {
  const uint32_t a = le32(offsets + 4 * i);
  len = le32(offsets + 4 * (i + 1)) - a;
  return data + a;
}

uint32_t crc32(const uint8_t* p, size_t len, uint32_t crc) {
  static const Table table;
  crc = ~crc;
  for (size_t i = 0; i < len; i++) crc = table.v[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

Status parse(const uint8_t* p, size_t avail, Clip& out) {
  if (avail < kHeaderSize) return Status::Corrupt;
  if (memcmp(p, "FCLP", 4) != 0) return Status::Missing;
  Clip c;
  c.width = le16(p + 4);
  c.height = le16(p + 6);
  c.fps = le16(p + 8);
  c.count = le16(p + 10);
  const uint32_t bodyLen = le32(p + 12);
  if (c.width != kSize || c.height != kSize || c.fps == 0 || c.count == 0) return Status::Corrupt;
  const size_t tableLen = 4 * (static_cast<size_t>(c.count) + 1);
  if (bodyLen < tableLen || bodyLen > avail - kHeaderSize) return Status::Corrupt;
  const uint8_t* body = p + kHeaderSize;
  if (crc32(body, bodyLen) != le32(p + 16)) return Status::Corrupt;
  c.offsets = body;
  c.data = body + tableLen;
  uint32_t prev = 0;
  for (int i = 0; i <= c.count; i++) {
    const uint32_t o = le32(c.offsets + 4 * i);
    if (o < prev) return Status::Corrupt;
    prev = o;
  }
  if (le32(c.offsets) != 0 || prev != bodyLen - tableLen) return Status::Corrupt;
  out = c;
  return Status::Ok;
}

}  // namespace clip
