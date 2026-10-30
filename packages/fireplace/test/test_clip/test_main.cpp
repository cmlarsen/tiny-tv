#include <unity.h>

#include <string.h>

#include <vector>

#include "clip.h"

void setUp() {}
void tearDown() {}

static void put16(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(v & 0xFF);
  b.push_back(v >> 8);
}

static void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF);
}

static std::vector<uint8_t> makeClip(const std::vector<std::vector<uint8_t>>& frames, uint16_t fps = 30) {
  std::vector<uint8_t> body;
  uint32_t off = 0;
  for (const auto& f : frames) {
    put32(body, off);
    off += f.size();
  }
  put32(body, off);
  for (const auto& f : frames) body.insert(body.end(), f.begin(), f.end());

  std::vector<uint8_t> out = {'F', 'C', 'L', 'P'};
  put16(out, 240);
  put16(out, 240);
  put16(out, fps);
  put16(out, static_cast<uint16_t>(frames.size()));
  put32(out, body.size());
  put32(out, clip::crc32(body.data(), body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

void test_crc32_matches_the_standard_check_value() {
  const char* s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, clip::crc32(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_crc32_can_run_in_chunks() {
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7};
  const uint32_t whole = clip::crc32(data, 7);
  TEST_ASSERT_EQUAL_HEX32(whole, clip::crc32(data + 3, 4, clip::crc32(data, 3)));
}

void test_parses_frames() {
  auto bytes = makeClip({{0xAA, 0xBB}, {0xCC}, {0xDD, 0xEE, 0xFF}}, 25);
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Ok, clip::parse(bytes.data(), bytes.size(), c));
  TEST_ASSERT_EQUAL(3, c.count);
  TEST_ASSERT_EQUAL(25, c.fps);
  TEST_ASSERT_EQUAL(240, c.width);
  size_t len;
  const uint8_t* f = c.frame(2, len);
  TEST_ASSERT_EQUAL(3, len);
  TEST_ASSERT_EQUAL_HEX8(0xDD, f[0]);
  f = c.frame(1, len);
  TEST_ASSERT_EQUAL(1, len);
  TEST_ASSERT_EQUAL_HEX8(0xCC, f[0]);
}

void test_trailing_space_after_the_clip_is_fine() {
  auto bytes = makeClip({{1, 2}, {3}});
  bytes.resize(bytes.size() + 1000, 0xFF);
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Ok, clip::parse(bytes.data(), bytes.size(), c));
}

void test_erased_flash_has_no_clip() {
  std::vector<uint8_t> bytes(4096, 0xFF);
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Missing, clip::parse(bytes.data(), bytes.size(), c));
}

void test_corrupt_body_fails_the_crc() {
  auto bytes = makeClip({{1, 2}, {3}});
  bytes.back() ^= 0x40;
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Corrupt, clip::parse(bytes.data(), bytes.size(), c));
}

void test_truncated_clip_is_rejected() {
  auto bytes = makeClip({{1, 2}, {3}});
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Corrupt, clip::parse(bytes.data(), bytes.size() - 1, c));
  TEST_ASSERT_EQUAL(clip::Status::Corrupt, clip::parse(bytes.data(), 10, c));
}

void test_empty_or_wrong_size_clip_is_rejected() {
  auto bytes = makeClip({});
  clip::Clip c;
  TEST_ASSERT_EQUAL(clip::Status::Corrupt, clip::parse(bytes.data(), bytes.size(), c));
  bytes = makeClip({{1}});
  bytes[4] = 100;
  TEST_ASSERT_EQUAL(clip::Status::Corrupt, clip::parse(bytes.data(), bytes.size(), c));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_matches_the_standard_check_value);
  RUN_TEST(test_crc32_can_run_in_chunks);
  RUN_TEST(test_parses_frames);
  RUN_TEST(test_trailing_space_after_the_clip_is_fine);
  RUN_TEST(test_erased_flash_has_no_clip);
  RUN_TEST(test_corrupt_body_fails_the_crc);
  RUN_TEST(test_truncated_clip_is_rejected);
  RUN_TEST(test_empty_or_wrong_size_clip_is_rejected);
  return UNITY_END();
}
