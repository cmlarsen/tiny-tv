#include <unity.h>
#include <cstring>

#include "statslogic.h"

using namespace statslogic;

void setUp() {}
void tearDown() {}

void test_parse_linux_stats() {
  const char* json =
      R"({"host": "deltron", "os": "linux", "cpu": 0.2, "mem": 5.2, "mem_used_gb": 1.6, "mem_total_gb": 30.64,)"
      R"( "cores": 12, "load": [0.01, 0.0, 0.0], "ts": 1793287267})";
  Stats s{};
  TEST_ASSERT_TRUE(parseStats(json, strlen(json), s));
  TEST_ASSERT_EQUAL_STRING("deltron", s.host);
  TEST_ASSERT_FALSE(s.darwin);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.2, s.cpu);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 5.2, s.mem);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.6, s.memUsedGb);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 30.64, s.memTotalGb);
  TEST_ASSERT_EQUAL(12, s.cores);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.01, s.load[0]);
  TEST_ASSERT_EQUAL_UINT32(1793287267u, s.ts);
}

void test_parse_darwin_stats() {
  const char* json =
      R"({"host":"caleb-work2","os":"darwin","cpu":21.8,"mem":82.4,"mem_used_gb":19.77,"mem_total_gb":24.0,)"
      R"("cores":15,"load":[7.9,8.49,8.28],"ts":1793287267})";
  Stats s{};
  TEST_ASSERT_TRUE(parseStats(json, strlen(json), s));
  TEST_ASSERT_EQUAL_STRING("caleb-work2", s.host);
  TEST_ASSERT_TRUE(s.darwin);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 8.28, s.load[2]);
}

void test_parse_rejects_missing_fields_and_garbage() {
  Stats s{};
  const char* noCpu = R"({"host":"x","os":"linux","mem":5})";
  TEST_ASSERT_FALSE(parseStats(noCpu, strlen(noCpu), s));
  const char* noMem = R"({"host":"x","os":"linux","cpu":5})";
  TEST_ASSERT_FALSE(parseStats(noMem, strlen(noMem), s));
  TEST_ASSERT_FALSE(parseStats("<html>404</html>", 16, s));
}

void test_parse_clamps_and_truncates() {
  const char* json = R"({"host":"a-really-long-hostname-that-keeps-going","os":"linux","cpu":140,"mem":-3})";
  Stats s{};
  TEST_ASSERT_TRUE(parseStats(json, strlen(json), s));
  TEST_ASSERT_EQUAL(sizeof s.host - 1, strlen(s.host));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 100, s.cpu);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0, s.mem);
}

void test_level_thresholds() {
  TEST_ASSERT_EQUAL(Level::Calm, levelFor(0));
  TEST_ASSERT_EQUAL(Level::Calm, levelFor(59.9f));
  TEST_ASSERT_EQUAL(Level::Busy, levelFor(60));
  TEST_ASSERT_EQUAL(Level::Busy, levelFor(84.9f));
  TEST_ASSERT_EQUAL(Level::Hot, levelFor(85));
}

void test_parse_endpoints() {
  Endpoint e[6];
  const int n = parseEndpoints(" 192.168.68.77, 192.168.68.59:9200,,http://shop.local:9110/stats ", e, 6);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL_STRING("192.168.68.77", e[0].host);
  TEST_ASSERT_EQUAL(9110, e[0].port);
  TEST_ASSERT_EQUAL_STRING("192.168.68.59", e[1].host);
  TEST_ASSERT_EQUAL(9200, e[1].port);
  TEST_ASSERT_EQUAL_STRING("shop.local", e[2].host);
  TEST_ASSERT_EQUAL(9110, e[2].port);
}

void test_parse_endpoints_skips_bad_ports_and_caps() {
  Endpoint e[2];
  TEST_ASSERT_EQUAL(1, parseEndpoints("a:abc, b:0, c:70000, d", e, 2));
  TEST_ASSERT_EQUAL_STRING("d", e[0].host);
  TEST_ASSERT_EQUAL(2, parseEndpoints("a,b,c,d", e, 2));
  TEST_ASSERT_EQUAL(0, parseEndpoints("", e, 2));
}

void test_format_age() {
  char buf[8];
  formatAge(3, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("3s", buf);
  formatAge(59, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("59s", buf);
  formatAge(60, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("1m", buf);
  formatAge(3599, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("59m", buf);
  formatAge(3600, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("1h", buf);
  formatAge(200000, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("2d", buf);
}

void test_history_keeps_last_n_oldest_first() {
  History<4> h;
  TEST_ASSERT_EQUAL(0, h.size());
  h.push(1);
  h.push(2);
  h.push(3);
  TEST_ASSERT_EQUAL(3, h.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1, h.at(0));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 3, h.at(2));
  h.push(4);
  h.push(5);
  h.push(6);
  TEST_ASSERT_EQUAL(4, h.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 3, h.at(0));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 6, h.at(3));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_linux_stats);
  RUN_TEST(test_parse_darwin_stats);
  RUN_TEST(test_parse_rejects_missing_fields_and_garbage);
  RUN_TEST(test_parse_clamps_and_truncates);
  RUN_TEST(test_level_thresholds);
  RUN_TEST(test_parse_endpoints);
  RUN_TEST(test_parse_endpoints_skips_bad_ports_and_caps);
  RUN_TEST(test_format_age);
  RUN_TEST(test_history_keeps_last_n_oldest_first);
  return UNITY_END();
}
