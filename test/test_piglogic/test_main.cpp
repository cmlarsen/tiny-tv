#include <unity.h>
#include <cstring>
#include <string>

#include "piglogic.h"

using namespace piglogic;

static DayWeather day(int code, float maxF, float minF = 40, int precip = 0, float wind = 5, int cloud = 0) {
  return DayWeather{code, maxF, minF, precip, wind, cloud};
}

void setUp() {}
void tearDown() {}

void test_storm_beats_everything() {
  TEST_ASSERT_EQUAL(Mood::Storm, classify(day(95, 98, 70, 90, 30)));
  TEST_ASSERT_EQUAL(Mood::Storm, classify(day(99, 20)));
}

void test_snow_beats_cold_and_rain() {
  TEST_ASSERT_EQUAL(Mood::Snow, classify(day(73, 25, 10, 80)));
  TEST_ASSERT_EQUAL(Mood::Snow, classify(day(85, 30)));
}

void test_rain_from_code_or_probability() {
  TEST_ASSERT_EQUAL(Mood::Rain, classify(day(61, 60, 50, 20)));
  TEST_ASSERT_EQUAL(Mood::Rain, classify(day(81, 70)));
  TEST_ASSERT_EQUAL(Mood::Rain, classify(day(3, 70, 50, 60)));
  TEST_ASSERT_EQUAL(Mood::Rain, classify(day(0, 95, 70, 55)));
}

void test_low_rain_chance_is_not_rain() {
  TEST_ASSERT_EQUAL(Mood::Cloudy, classify(day(3, 70, 50, 40, 5, 80)));
}

void test_hot_and_cold() {
  TEST_ASSERT_EQUAL(Mood::Hot, classify(day(0, 95)));
  TEST_ASSERT_EQUAL(Mood::Hot, classify(day(3, 88)));
  TEST_ASSERT_EQUAL(Mood::Cold, classify(day(0, 30)));
  TEST_ASSERT_EQUAL(Mood::Cold, classify(day(3, 40)));
}

void test_windy_beats_chilly_but_not_cold() {
  TEST_ASSERT_EQUAL(Mood::Windy, classify(day(1, 65, 50, 0, 30)));
  TEST_ASSERT_EQUAL(Mood::Windy, classify(day(1, 50, 40, 0, 30)));
  TEST_ASSERT_EQUAL(Mood::Cold, classify(day(1, 35, 20, 0, 30)));
}

void test_chilly_fog_cloudy_sunny() {
  TEST_ASSERT_EQUAL(Mood::Chilly, classify(day(2, 52)));
  TEST_ASSERT_EQUAL(Mood::Fog, classify(day(45, 65, 40, 0, 5, 90)));
  TEST_ASSERT_EQUAL(Mood::Cloudy, classify(day(3, 70, 50, 0, 5, 80)));
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(day(0, 72)));
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(day(1, 80)));
}

void test_sky_follows_cloud_cover_not_worst_code() {
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(day(3, 70, 50, 0, 5, 20)));
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(day(2, 70, 50, 0, 5, 30)));
  TEST_ASSERT_EQUAL(Mood::Cloudy, classify(day(2, 70, 50, 0, 5, 60)));
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(day(45, 70, 50, 0, 5, 10)));
}

void test_message_substitutes_day_and_name() {
  char buf[96];
  for (int m = 0; m < kMoodCount; m++) {
    for (uint32_t seed = 0; seed < 12; seed++) {
      for (int tomorrow = 0; tomorrow < 2; tomorrow++) {
        formatMessage(static_cast<Mood>(m), tomorrow, "Zed", seed, buf, sizeof buf);
        std::string s(buf);
        TEST_ASSERT_FALSE_MESSAGE(s.empty(), "empty message");
        TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, s.find('{'), buf);
        TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, s.find('}'), buf);
        TEST_ASSERT_TRUE_MESSAGE(s.size() <= 46, buf);
        if (tomorrow) TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, s.find("oday"), buf);
        else TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, s.find("omorrow"), buf);
      }
    }
  }
}

void test_message_examples() {
  char buf[96];
  formatMessage(Mood::Hot, false, "Zed", 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Today will be toasty! Drink lots of water.", buf);
  formatMessage(Mood::Hot, true, "Zed", 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Tomorrow will be toasty! Drink lots of water.", buf);
  formatMessage(Mood::Rain, false, "Zed", 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Today might rain. Bring a raincoat!", buf);
  formatMessage(Mood::Cold, false, "Zed", 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Brrr! Super cold today. Wear a hat!", buf);
}

void test_message_is_deterministic_and_varies() {
  char a[96], b[96], c[96];
  formatMessage(Mood::Sunny, false, "Zed", 1, a, sizeof a);
  formatMessage(Mood::Sunny, false, "Zed", 1, b, sizeof b);
  formatMessage(Mood::Sunny, false, "Zed", 2, c, sizeof c);
  TEST_ASSERT_EQUAL_STRING(a, b);
  TEST_ASSERT_NOT_EQUAL(0, strcmp(a, c));
}

void test_message_truncates_safely() {
  char buf[8];
  formatMessage(Mood::Hot, false, "Zed", 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("Today w", buf);
}

static std::string hourlyArray(int (*value)(int hour)) {
  std::string out = "[";
  for (int i = 0; i < 48; i++) {
    if (i) out += ",";
    out += std::to_string(value(i));
  }
  return out + "]";
}

static int kalamazooCode(int h) { return (h == 4 || h == 6 || h == 10) ? 3 : (h == 33 ? 61 : 0); }
static int kalamazooCloud(int h) { return (h == 4 || h == 6 || h == 10) ? 100 : (h >= 24 ? 70 : 0); }
static int kalamazooPrecip(int h) { return h == 33 ? 40 : (h == 35 ? 35 : 2); }

static std::string forecastJson(const char* currentTime) {
  return std::string(R"({"latitude":42.29,"longitude":-85.58,"utc_offset_seconds":-14400,"timezone":"America/Detroit",)") +
         R"("current":{"time":")" + currentTime + R"(","interval":900,"temperature_2m":66.4,"weather_code":0,"is_day":1},)" +
         R"("hourly":{"weather_code":)" + hourlyArray(kalamazooCode) + R"(,"cloud_cover":)" + hourlyArray(kalamazooCloud) +
         R"(,"precipitation_probability":)" + hourlyArray(kalamazooPrecip) + "}," +
         R"("daily":{"time":["2026-10-21","2026-10-22"],"weather_code":[3,61],"temperature_2m_max":[67.4,71.9],)"
         R"("temperature_2m_min":[38.2,53.1],"precipitation_probability_max":[3,40],"wind_speed_10m_max":[7.3,7.1]}})";
}

void test_parse_forecast() {
  const std::string json = forecastJson("2026-10-21T14:45");
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json.c_str(), json.size(), f));
  TEST_ASSERT_EQUAL_STRING("2026-10-21", f.date);
  TEST_ASSERT_EQUAL(-14400, f.utcOffsetSec);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 66.4, f.nowF);
  TEST_ASSERT_EQUAL(0, f.nowCode);
  TEST_ASSERT_TRUE(f.isDay);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 67.4, f.days[0].maxF);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 53.1, f.days[1].minF);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 7.3, f.days[0].windMph);
}

void test_today_ignores_hours_already_past() {
  const std::string json = forecastJson("2026-10-21T14:45");
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json.c_str(), json.size(), f));
  TEST_ASSERT_EQUAL(0, f.days[0].code);
  TEST_ASSERT_EQUAL(0, f.days[0].cloudPct);
  TEST_ASSERT_EQUAL(2, f.days[0].precipPct);
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(f.days[0]));
}

void test_early_morning_covers_whole_daytime() {
  const std::string json = forecastJson("2026-10-21T05:10");
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json.c_str(), json.size(), f));
  TEST_ASSERT_EQUAL(3, f.days[0].code);
  TEST_ASSERT_EQUAL(100 / 14, f.days[0].cloudPct);
  TEST_ASSERT_EQUAL(Mood::Sunny, classify(f.days[0]));
}

void test_late_evening_uses_rest_of_day() {
  const std::string json = forecastJson("2026-10-21T22:30");
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json.c_str(), json.size(), f));
  TEST_ASSERT_EQUAL(0, f.days[0].code);
}

void test_tomorrow_uses_daytime_hours() {
  const std::string json = forecastJson("2026-10-21T14:45");
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json.c_str(), json.size(), f));
  TEST_ASSERT_EQUAL(61, f.days[1].code);
  TEST_ASSERT_EQUAL(70, f.days[1].cloudPct);
  TEST_ASSERT_EQUAL(40, f.days[1].precipPct);
  TEST_ASSERT_EQUAL(Mood::Rain, classify(f.days[1]));
}

void test_missing_hourly_falls_back_to_daily() {
  const char* json =
      R"({"utc_offset_seconds":0,"current":{"time":"2026-10-21T12:00","temperature_2m":50,"weather_code":0,"is_day":1},)"
      R"("daily":{"time":["2026-10-21","2026-10-22"],"weather_code":[3,61],"temperature_2m_max":[60,61],)"
      R"("temperature_2m_min":[40,41],"precipitation_probability_max":[10,70],"wind_speed_10m_max":[1,2]}})";
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json, strlen(json), f));
  TEST_ASSERT_EQUAL(3, f.days[0].code);
  TEST_ASSERT_EQUAL(100, f.days[0].cloudPct);
  TEST_ASSERT_EQUAL(70, f.days[1].precipPct);
}

void test_parse_forecast_rejects_garbage() {
  Forecast f{};
  const char* bad = "{\"error\":true,\"reason\":\"nope\"}";
  TEST_ASSERT_FALSE(parseForecast(bad, strlen(bad), f));
  TEST_ASSERT_FALSE(parseForecast("not json", 8, f));
}

void test_parse_forecast_null_precip_counts_as_zero() {
  const char* json =
      R"({"utc_offset_seconds":0,"current":{"temperature_2m":50,"weather_code":0,"is_day":0},)"
      R"("daily":{"time":["2026-10-21","2026-10-22"],"weather_code":[0,0],"temperature_2m_max":[60,61],)"
      R"("temperature_2m_min":[40,41],"precipitation_probability_max":[null,null],"wind_speed_10m_max":[1,2]}})";
  Forecast f{};
  TEST_ASSERT_TRUE(parseForecast(json, strlen(json), f));
  TEST_ASSERT_EQUAL(0, f.days[0].precipPct);
  TEST_ASSERT_FALSE(f.isDay);
}

void test_parse_geocode() {
  const char* json =
      R"({"results":[{"id":5574991,"name":"Boulder","latitude":40.01499,"longitude":-105.27055,)"
      R"("country_code":"US","admin1":"Colorado"}],"generationtime_ms":0.5})";
  Place p{};
  TEST_ASSERT_TRUE(parseGeocode(json, strlen(json), p));
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 40.01499, p.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, -105.27055, p.lon);
  TEST_ASSERT_EQUAL_STRING("Boulder", p.name);
}

void test_parse_geocode_no_results() {
  const char* json = R"({"generationtime_ms":0.5})";
  Place p{};
  TEST_ASSERT_FALSE(parseGeocode(json, strlen(json), p));
}

void test_parse_ip_location() {
  const char* json = R"({"lat":39.7392,"lon":-104.9903,"city":"Denver"})";
  Place p{};
  TEST_ASSERT_TRUE(parseIpLocation(json, strlen(json), p));
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 39.7392, p.lat);
  TEST_ASSERT_EQUAL_STRING("Denver", p.name);
}

void test_display_temp() {
  TEST_ASSERT_EQUAL(69, toDisplayTemp(68.7f, false));
  TEST_ASSERT_EQUAL(0, toDisplayTemp(32.0f, true));
  TEST_ASSERT_EQUAL(-40, toDisplayTemp(-40.0f, true));
  TEST_ASSERT_EQUAL(-18, toDisplayTemp(0.0f, true));
  TEST_ASSERT_EQUAL(-5, toDisplayTemp(-4.6f, false));
}

void test_seed_from_date() {
  TEST_ASSERT_EQUAL(seedFromDate("2026-10-21"), seedFromDate("2026-10-21"));
  TEST_ASSERT_NOT_EQUAL(seedFromDate("2026-10-21"), seedFromDate("2026-10-22"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_storm_beats_everything);
  RUN_TEST(test_snow_beats_cold_and_rain);
  RUN_TEST(test_rain_from_code_or_probability);
  RUN_TEST(test_low_rain_chance_is_not_rain);
  RUN_TEST(test_hot_and_cold);
  RUN_TEST(test_windy_beats_chilly_but_not_cold);
  RUN_TEST(test_chilly_fog_cloudy_sunny);
  RUN_TEST(test_sky_follows_cloud_cover_not_worst_code);
  RUN_TEST(test_message_substitutes_day_and_name);
  RUN_TEST(test_message_examples);
  RUN_TEST(test_message_is_deterministic_and_varies);
  RUN_TEST(test_message_truncates_safely);
  RUN_TEST(test_parse_forecast);
  RUN_TEST(test_today_ignores_hours_already_past);
  RUN_TEST(test_early_morning_covers_whole_daytime);
  RUN_TEST(test_late_evening_uses_rest_of_day);
  RUN_TEST(test_tomorrow_uses_daytime_hours);
  RUN_TEST(test_missing_hourly_falls_back_to_daily);
  RUN_TEST(test_parse_forecast_rejects_garbage);
  RUN_TEST(test_parse_forecast_null_precip_counts_as_zero);
  RUN_TEST(test_parse_geocode);
  RUN_TEST(test_parse_geocode_no_results);
  RUN_TEST(test_parse_ip_location);
  RUN_TEST(test_display_temp);
  RUN_TEST(test_seed_from_date);
  return UNITY_END();
}
