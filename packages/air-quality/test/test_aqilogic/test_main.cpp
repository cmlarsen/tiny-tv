#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "aqilogic.h"

using namespace aqilogic;

void setUp() {}
void tearDown() {}

const char* kHeader =
    "\"AQSID\",\"SiteName\",\"Status\",\"EPARegion\",\"Latitude\",\"Longitude\",\"Elevation\",\"GMTOffset\","
    "\"CountryCode\",\"StateName\",\"ValidDate\",\"ValidTime\",\"DataSource\",\"ReportingArea_PipeDelimited\","
    "\"OZONE_AQI\",\"PM10_AQI\",\"PM25_AQI\",\"NO2_AQI\",\"OZONE_Measured\",\"PM10_Measured\",\"PM25_Measured\","
    "\"NO2_Measured\",\"PM25\",\"PM25_Unit\",\"OZONE\",\"OZONE_Unit\",\"NO2\",\"NO2_Unit\",\"CO\",\"CO_Unit\","
    "\"SO2\",\"SO2_Unit\",\"PM10\",\"PM10_Unit\"";
const char* kKalamazoo =
    "\"260770008\",\"Kalamazoo\",\"Active\",\"R5\",\"42.2781\",\"-85.5419\",\"390.4\",\"-5\",\"US\",\"MI\","
    "\"10/30/2026\",\"13:00\",\"Michigan Department of Environment, Great Lakes, and Energy\",\"Kalamazoo\","
    "\"6\",\"\",\"44\",\"\",\"1\",\"0\",\"1\",\"0\",\"8.4\",\"UG/M3\",\"2.0\",\"PPB\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"";
const char* kHolland =
    "\"260050003\",\"Holland\",\"Active\",\"R5\",\"42.767799\",\"-86.148598\",\"390.4\",\"-5\",\"US\",\"MI\","
    "\"10/30/2026\",\"13:00\",\"Michigan EGLE\",\"Grand Rapids\","
    "\"9\",\"\",\"24\",\"8\",\"1\",\"0\",\"1\",\"1\",\"3.4\",\"UG/M3\",\"6.0\",\"PPB\",\"9.3\",\"PPB\",\"\",\"\",\"\",\"\",\"\",\"\"";
const char* kEvansOzoneOnly =
    "\"260810022\",\"Evans\",\"Active\",\"R5\",\"42.30\",\"-85.55\",\"0.0\",\"-5\",\"US\",\"MI\","
    "\"10/30/2026\",\"13:00\",\"Michigan EGLE\",\"Grand Rapids\","
    "\"10\",\"\",\"\",\"\",\"1\",\"0\",\"0\",\"0\",\"\",\"\",\"6.0\",\"PPB\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"";
const char* kInactiveNearby =
    "\"999\",\"Closed\",\"Inactive\",\"R5\",\"42.2711\",\"-85.5419\",\"0.0\",\"-5\",\"US\",\"MI\","
    "\"10/30/2026\",\"13:00\",\"x\",\"y\",\"90\",\"\",\"150\",\"\",\"1\",\"0\",\"1\",\"0\",\"60.0\",\"UG/M3\",\"50.0\",\"PPB\","
    "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"";

void feed(Scanner& s, const char* text) {
  char buf[512];
  strncpy(buf, text, sizeof buf);
  buf[sizeof buf - 1] = '\0';
  s.line(buf);
}

void test_pm25_breakpoints() {
  TEST_ASSERT_EQUAL(0, pm25Aqi(0));
  TEST_ASSERT_EQUAL(47, pm25Aqi(8.4f));
  TEST_ASSERT_EQUAL(50, pm25Aqi(9.0f));
  TEST_ASSERT_EQUAL(51, pm25Aqi(9.1f));
  TEST_ASSERT_EQUAL(50, pm25Aqi(9.04f));
  TEST_ASSERT_EQUAL(100, pm25Aqi(35.4f));
  TEST_ASSERT_EQUAL(101, pm25Aqi(35.5f));
  TEST_ASSERT_EQUAL(151, pm25Aqi(55.5f));
  TEST_ASSERT_EQUAL(500, pm25Aqi(325.4f));
  TEST_ASSERT_EQUAL(500, pm25Aqi(900));
  TEST_ASSERT_EQUAL(0, pm25Aqi(-2));
}

void test_o3_breakpoints() {
  TEST_ASSERT_EQUAL(0, o3Aqi(0));
  TEST_ASSERT_EQUAL(50, o3Aqi(54));
  TEST_ASSERT_EQUAL(51, o3Aqi(55));
  TEST_ASSERT_EQUAL(100, o3Aqi(70));
  TEST_ASSERT_EQUAL(101, o3Aqi(71));
  TEST_ASSERT_EQUAL(201, o3Aqi(106));
  TEST_ASSERT_EQUAL(300, o3Aqi(200));
  TEST_ASSERT_EQUAL(300, o3Aqi(201));
  TEST_ASSERT_EQUAL(300, o3Aqi(404));
  TEST_ASSERT_EQUAL(301, o3Aqi(405));
}

void test_levels() {
  TEST_ASSERT_EQUAL(Level::Good, level(50));
  TEST_ASSERT_EQUAL(Level::Moderate, level(51));
  TEST_ASSERT_EQUAL(Level::Sensitive, level(150));
  TEST_ASSERT_EQUAL(Level::Unhealthy, level(151));
  TEST_ASSERT_EQUAL(Level::VeryUnhealthy, level(300));
  TEST_ASSERT_EQUAL(Level::Hazardous, level(301));
  TEST_ASSERT_EQUAL_STRING("Unhealthy for", levelLine1(Level::Sensitive));
  TEST_ASSERT_EQUAL_STRING("sensitive groups", levelLine2(Level::Sensitive));
  TEST_ASSERT_EQUAL_STRING("", levelLine2(Level::Good));
}

void test_distance() {
  TEST_ASSERT_FLOAT_WITHIN(1, 0.8f, distanceKm(42.2711f, -85.5419f, 42.2781f, -85.5419f));
  TEST_ASSERT_FLOAT_WITHIN(3, 72, distanceKm(42.2711f, -85.5419f, 42.767799f, -86.148598f));
}

void test_obs_path() {
  char buf[80];
  obsPath(1793368800 / 3600, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("/airnow/2026/20261030/HourlyAQObs_2026103014.dat", buf);
}

void test_age_label() {
  char buf[24];
  const int32_t h = 1793368800 / 3600;
  ageLabel(1793368800 + 3600 + 20 * 60, h, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("20 min ago", buf);
  ageLabel(1793368800 + 3600 + 130 * 60, h, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("2 h ago", buf);
  ageLabel(1793368800 + 3000, h, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("just now", buf);
}

void test_scanner_picks_nearest_active_site_per_pollutant() {
  Scanner s(42.2711f, -85.5419f, 100);
  feed(s, kHeader);
  TEST_ASSERT_TRUE(s.headerOk());
  feed(s, kHolland);
  feed(s, kInactiveNearby);
  feed(s, kKalamazoo);
  feed(s, kEvansOzoneOnly);
  const HourObs& o = s.result();
  TEST_ASSERT_TRUE(o.pm25.have);
  TEST_ASSERT_EQUAL_STRING("Kalamazoo", o.pm25.site);
  TEST_ASSERT_EQUAL(44, o.pm25.aqi);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.4f, o.pm25.conc);
  TEST_ASSERT_TRUE(o.o3.have);
  TEST_ASSERT_EQUAL_STRING("Kalamazoo", o.o3.site);
  TEST_ASSERT_EQUAL(6, o.o3.aqi);
  TEST_ASSERT_EQUAL(44, o.aqi());
  TEST_ASSERT_EQUAL_STRING("PM2.5", o.dominant());
}

void test_scanner_ignores_sites_out_of_range() {
  Scanner s(42.2711f, -85.5419f, 50);
  feed(s, kHeader);
  feed(s, kHolland);
  TEST_ASSERT_FALSE(s.result().any());
}

void test_scanner_needs_header() {
  Scanner s(42.2711f, -85.5419f, 100);
  feed(s, "<?xml version=\"1.0\"?><Error>");
  feed(s, kKalamazoo);
  TEST_ASSERT_FALSE(s.headerOk());
  TEST_ASSERT_FALSE(s.result().any());
}

void test_scanner_handles_crlf_and_short_lines() {
  Scanner s(42.2711f, -85.5419f, 100);
  char header[600];
  snprintf(header, sizeof header, "%s\r", kHeader);
  s.line(header);
  feed(s, "\"1\",\"x\"");
  char row[512];
  snprintf(row, sizeof row, "%s\r", kKalamazoo);
  s.line(row);
  TEST_ASSERT_EQUAL(44, s.result().pm25.aqi);
}

HourObs obs(float pm, float o3) {
  HourObs o{};
  if (pm >= 0) {
    o.pm25.have = true;
    o.pm25.conc = pm;
  }
  if (o3 >= 0) {
    o.o3.have = true;
    o.o3.conc = o3;
  }
  return o;
}

void test_history_average_uses_mean_concentration() {
  History h;
  const int32_t now = 500000;
  h.put(now, obs(4, 20));
  h.put(now - 1, obs(8, 30));
  h.put(now - 2, obs(12, 40));
  const Average a = h.average(now, 3);
  TEST_ASSERT_TRUE(a.have);
  TEST_ASSERT_EQUAL(3, a.hours);
  TEST_ASSERT_EQUAL(pm25Aqi(8), a.aqi);
  TEST_ASSERT_EQUAL_STRING("PM2.5", a.dominant);
  TEST_ASSERT_EQUAL(pm25Aqi(12), h.hourlyAqi(now - 2));
  TEST_ASSERT_EQUAL(-1, h.hourlyAqi(now - 3));
}

void test_history_average_window_and_ozone_dominant() {
  History h;
  const int32_t now = 500000;
  h.put(now, obs(2, 60));
  h.put(now - 5, obs(100, 10));
  const Average a = h.average(now, 4);
  TEST_ASSERT_EQUAL(1, a.hours);
  TEST_ASSERT_EQUAL(o3Aqi(60), a.aqi);
  TEST_ASSERT_EQUAL_STRING("Ozone", a.dominant);
  TEST_ASSERT_FALSE(h.average(now + 10, 4).have);
}

void test_history_ring_overwrites_old_hours() {
  History h;
  h.put(100, obs(5, -1));
  h.put(100 + History::kMax, obs(6, -1));
  TEST_ASSERT_FALSE(h.has(100));
  TEST_ASSERT_TRUE(h.has(100 + History::kMax));
  h.put(100, obs(7, -1));
  TEST_ASSERT_TRUE(h.has(100 + History::kMax));
  TEST_ASSERT_FALSE(h.has(100));
}

void test_history_missing_newest_first() {
  History h;
  h.put(1000, obs(5, -1));
  h.put(998, obs(5, -1));
  int32_t out[8];
  const int n = h.missing(1000, 4, out, 8);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL(999, out[0]);
  TEST_ASSERT_EQUAL(997, out[1]);
}

void test_parse_geocode() {
  const char* json = "{\"results\":[{\"name\":\"Kalamazoo\",\"latitude\":42.29171,\"longitude\":-85.58723}]}";
  Place p{};
  TEST_ASSERT_TRUE(parseGeocode(json, strlen(json), p));
  TEST_ASSERT_EQUAL_STRING("Kalamazoo", p.name);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.2917f, p.lat);
  const char* none = "{\"generationtime_ms\":0.1}";
  TEST_ASSERT_FALSE(parseGeocode(none, strlen(none), p));
}

void test_parse_ip_location() {
  const char* json = "{\"status\":\"success\",\"lat\":42.2711,\"lon\":-85.5419,\"city\":\"Kalamazoo\"}";
  Place p{};
  TEST_ASSERT_TRUE(parseIpLocation(json, strlen(json), p));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -85.5419f, p.lon);
}

void test_parse_hours() {
  TEST_ASSERT_EQUAL(24, parseHours("", 24));
  TEST_ASSERT_EQUAL(8, parseHours("8", 24));
  TEST_ASSERT_EQUAL(24, parseHours("0", 24));
  TEST_ASSERT_EQUAL(24, parseHours("72", 12));
  TEST_ASSERT_EQUAL(12, parseHours("abc", 12));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pm25_breakpoints);
  RUN_TEST(test_o3_breakpoints);
  RUN_TEST(test_levels);
  RUN_TEST(test_distance);
  RUN_TEST(test_obs_path);
  RUN_TEST(test_age_label);
  RUN_TEST(test_scanner_picks_nearest_active_site_per_pollutant);
  RUN_TEST(test_scanner_ignores_sites_out_of_range);
  RUN_TEST(test_scanner_needs_header);
  RUN_TEST(test_scanner_handles_crlf_and_short_lines);
  RUN_TEST(test_history_average_uses_mean_concentration);
  RUN_TEST(test_history_average_window_and_ozone_dominant);
  RUN_TEST(test_history_ring_overwrites_old_hours);
  RUN_TEST(test_history_missing_newest_first);
  RUN_TEST(test_parse_geocode);
  RUN_TEST(test_parse_ip_location);
  RUN_TEST(test_parse_hours);
  return UNITY_END();
}
