// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Sample payloads ───────────────────────────────────────────────────────────

static const char* VALID_PAYLOAD = R"({
  "daily": {
    "temperature_2m_max":            [85.0, 90.0, 78.0, 65.0, 72.0, 88.0],
    "temperature_2m_min":            [70.0, 75.0, 60.0, 50.0, 55.0, 72.0],
    "precipitation_probability_max": [10.0, 20.0, 30.0, 40.0, 50.0, 60.0]
  }
})";

// ── Fixture ───────────────────────────────────────────────────────────────────

class FetchForecastTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_http_code     = HTTP_CODE_OK;
        g_mock_http_payload  = String(VALID_PAYLOAD);
        g_mock_http_begin_ok = true;
        cfg_num_leds         = DEFAULT_NUM_LEDS;
        strncpy(cfg_latitude,  DEFAULT_LATITUDE,  sizeof(cfg_latitude));
        strncpy(cfg_longitude, DEFAULT_LONGITUDE, sizeof(cfg_longitude));
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

// Description: A well-formed Open-Meteo JSON response fills all 6 forecast days
// with the correct max, min, average, and precipitation values.
TEST_F(FetchForecastTest, ValidResponsePopulatesAllDays) {
    RecordProperty("description",
        "A well-formed Open-Meteo JSON response fills all 6 forecast days "
        "with the correct max, min, average, and precipitation values.");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));

    EXPECT_FLOAT_EQ(days[0].tempMax,    85.0f);
    EXPECT_FLOAT_EQ(days[0].tempMin,    70.0f);
    EXPECT_FLOAT_EQ(days[0].tempAvg,    77.5f);
    EXPECT_FLOAT_EQ(days[0].precipProb, 10.0f);

    EXPECT_FLOAT_EQ(days[5].tempMax,    88.0f);
    EXPECT_FLOAT_EQ(days[5].tempMin,    72.0f);
    EXPECT_FLOAT_EQ(days[5].tempAvg,    80.0f);
    EXPECT_FLOAT_EQ(days[5].precipProb, 60.0f);
}

// Description: Each day's tempAvg is exactly (tempMax + tempMin) / 2,
// with no rounding or offset applied.
TEST_F(FetchForecastTest, TempAvgIsArithmeticMean) {
    RecordProperty("description",
        "Each day's tempAvg is exactly (tempMax + tempMin) / 2, "
        "with no rounding or offset applied.");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    for (int i = 0; i < cfg_num_leds; i++) {
        float expected = (days[i].tempMax + days[i].tempMin) / 2.0f;
        EXPECT_FLOAT_EQ(days[i].tempAvg, expected) << "Day " << i;
    }
}

// Description: An HTTP 500 response causes fetchForecast() to return false
// without populating any day data.
TEST_F(FetchForecastTest, HttpErrorReturnsFalse) {
    RecordProperty("description",
        "An HTTP 500 response causes fetchForecast() to return false "
        "without populating any day data.");
    g_mock_http_code = 500;
    DayForecast days[DEFAULT_NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

// Description: When HTTPClient.begin() fails (network unreachable),
// fetchForecast() returns false immediately without attempting a request.
TEST_F(FetchForecastTest, BeginFailureReturnsFalse) {
    RecordProperty("description",
        "When HTTPClient.begin() fails (network unreachable), "
        "fetchForecast() returns false immediately without attempting a request.");
    g_mock_http_begin_ok = false;
    DayForecast days[DEFAULT_NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

// Description: A response body that is not valid JSON causes fetchForecast()
// to return false rather than producing garbage data.
TEST_F(FetchForecastTest, MalformedJsonReturnsFalse) {
    RecordProperty("description",
        "A response body that is not valid JSON causes fetchForecast() "
        "to return false rather than producing garbage data.");
    g_mock_http_payload = String("{not valid json{{");
    DayForecast days[DEFAULT_NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

// Description: A JSON body that lacks the temperature arrays (max/min) causes
// fetchForecast() to return false; temperature data is required.
TEST_F(FetchForecastTest, MissingTemperatureArrayReturnsFalse) {
    RecordProperty("description",
        "A JSON body that lacks the temperature arrays (max/min) causes "
        "fetchForecast() to return false; temperature data is required.");
    g_mock_http_payload = String(
        R"({"daily":{"precipitation_probability_max":[10,20,30,40,50,60]}})");
    DayForecast days[DEFAULT_NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

// Description: A JSON body with valid temperature arrays but no precipitation
// array defaults all days to 0% precipitation and still returns true.
TEST_F(FetchForecastTest, MissingPrecipArrayDefaultsToZero) {
    RecordProperty("description",
        "A JSON body with valid temperature arrays but no precipitation array "
        "defaults all days to 0% precipitation and still returns true.");
    g_mock_http_payload = String(R"({
      "daily": {
        "temperature_2m_max": [80,81,82,83,84,85],
        "temperature_2m_min": [60,61,62,63,64,65]
      }
    })");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_FLOAT_EQ(days[i].precipProb, 0.0f) << "Day " << i;
}

// Description: A null element in temperature_2m_max marks that day as invalid
// so a missing value is never silently promoted to 0°F and triggers a false
// freeze alert; adjacent valid days are unaffected.
TEST_F(FetchForecastTest, NullMaxElementDisablesThatDay) {
    RecordProperty("description",
        "A null element in temperature_2m_max marks that day as invalid so a "
        "missing value is never silently promoted to 0F and triggers a false "
        "freeze alert; adjacent valid days are unaffected.");
    g_mock_http_payload = String(R"({
      "daily": {
        "temperature_2m_max": [85.0, null, 78.0, 65.0, 72.0, 88.0],
        "temperature_2m_min": [70.0, 75.0, 60.0, 50.0, 55.0, 72.0],
        "precipitation_probability_max": [10.0, 20.0, 30.0, 40.0, 50.0, 60.0]
      }
    })");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FALSE(days[1].valid) << "null max element should mark day invalid";
    EXPECT_TRUE(days[0].valid) << "adjacent day should remain valid";
    EXPECT_TRUE(days[2].valid) << "adjacent day should remain valid";
}

// Description: A null element in temperature_2m_min marks that day as invalid
// so a missing low temperature cannot falsely trigger a freeze alert.
TEST_F(FetchForecastTest, NullMinElementDisablesThatDay) {
    RecordProperty("description",
        "A null element in temperature_2m_min marks that day as invalid so a "
        "missing low temperature cannot falsely trigger a freeze alert.");
    g_mock_http_payload = String(R"({
      "daily": {
        "temperature_2m_max": [85.0, 90.0, 78.0, 65.0, 72.0, 88.0],
        "temperature_2m_min": [70.0, null, 60.0, 50.0, 55.0, 72.0],
        "precipitation_probability_max": [10.0, 20.0, 30.0, 40.0, 50.0, 60.0]
      }
    })");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FALSE(days[1].valid) << "null min element should mark day invalid";
    EXPECT_TRUE(days[0].valid) << "adjacent day should remain valid";
    EXPECT_TRUE(days[2].valid) << "adjacent day should remain valid";
}

// Description: Changing cfg_latitude/cfg_longitude to non-US coordinates does
// not affect JSON parsing; the API response structure is location-independent.
TEST_F(FetchForecastTest, DifferentLocationStillParses) {
    RecordProperty("description",
        "Changing cfg_latitude/cfg_longitude to non-US coordinates does not "
        "affect JSON parsing; the API response structure is location-independent.");
    // Verify that changing lat/lon doesn't break parsing
    // (the mock always returns VALID_PAYLOAD regardless of the URL)
    strncpy(cfg_latitude,  "51.5074", sizeof(cfg_latitude));
    strncpy(cfg_longitude, "-0.1278", sizeof(cfg_longitude));
    DayForecast days[DEFAULT_NUM_LEDS];
    EXPECT_TRUE(fetchForecast(days));
}

// ── Rest-of-today narrowing for day 0 (issue #61) ────────────────────────────
// daily.time[0] = 1763269200 is "today's" local midnight; hourly times start at
// 14:00 local (todayStart + 14h). Entries at/after todayStart + 86400 belong to
// tomorrow and must be ignored.

static const long TODAY = 1763269200L;
static const long H14   = TODAY + 14 * 3600L;

static String restOfDayPayload(const char* hourlyTemps, const char* hourlyPrecip,
                               int nHours) {
    std::string times = "[";
    for (int h = 0; h < nHours; h++) {
        if (h) times += ",";
        times += std::to_string(H14 + h * 3600L);
    }
    times += "]";
    char buf[2048];
    snprintf(buf, sizeof(buf), R"({
      "daily": {
        "time": [%ld, %ld, %ld, %ld, %ld, %ld],
        "temperature_2m_max":            [99.0, 90.0, 78.0, 65.0, 72.0, 88.0],
        "temperature_2m_min":            [28.0, 75.0, 60.0, 50.0, 55.0, 72.0],
        "precipitation_probability_max": [90.0, 80.0, 30.0, 40.0, 50.0, 60.0]
      },
      "hourly": {
        "time": %s,
        "temperature_2m": %s,
        "precipitation_probability": %s
      }
    })", TODAY, TODAY + 86400L, TODAY + 2 * 86400L, TODAY + 3 * 86400L,
         TODAY + 4 * 86400L, TODAY + 5 * 86400L,
         times.c_str(), hourlyTemps, hourlyPrecip);
    return String(buf);
}

// Description: The request URL asks for 24 hours of hourly temperature and
// precipitation with integer epoch times, so day 0 can be narrowed to the
// remaining hours without a second request or string-time storage.
TEST_F(FetchForecastTest, UrlRequestsHourlyRestOfDayData) {
    RecordProperty("description",
        "The request URL asks for 24 hours of hourly temperature and precipitation "
        "with integer epoch times, so day 0 can be narrowed to the remaining hours "
        "without a second request or string-time storage.");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    std::string url = g_mock_http_url.c_str();
    EXPECT_NE(url.find("hourly=temperature_2m,precipitation_probability"), std::string::npos) << url;
    EXPECT_NE(url.find("forecast_hours=24"), std::string::npos) << url;
    EXPECT_NE(url.find("timeformat=unixtime"), std::string::npos) << url;
}

// Description: Rain that fell earlier today (daily max 90%) no longer counts once
// the remaining hours are dry; day 0's precipProb becomes the rest-of-day max
// while later days keep their daily values.
TEST_F(FetchForecastTest, PastRainTodayIgnored) {
    RecordProperty("description",
        "Rain that fell earlier today (daily max 90%) no longer counts once the "
        "remaining hours are dry; day 0's precipProb becomes the rest-of-day max "
        "while later days keep their daily values.");
    g_mock_http_payload = restOfDayPayload(
        "[60, 60, 60]", "[5, 10, 0]", 3);
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FLOAT_EQ(days[0].precipProb, 10.0f);
    EXPECT_FLOAT_EQ(days[1].precipProb, 80.0f);
}

// Description: Rain still forecast later today keeps day 0's precipitation
// probability high, so the fix does not suppress genuine upcoming rain.
TEST_F(FetchForecastTest, UpcomingRainTodayKept) {
    RecordProperty("description",
        "Rain still forecast later today keeps day 0's precipitation probability "
        "high, so the fix does not suppress genuine upcoming rain.");
    g_mock_http_payload = restOfDayPayload(
        "[60, 60, 60]", "[5, 70, 20]", 3);
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FLOAT_EQ(days[0].precipProb, 70.0f);
}

// Description: A cold morning and a hot spell that have already passed do not
// drive day 0's min/max; they become the rest-of-day extremes, while tempAvg
// (the base color) still reflects the full-day daily values.
TEST_F(FetchForecastTest, PastTempExtremesIgnoredButAvgKept) {
    RecordProperty("description",
        "A cold morning and a hot spell that have already passed do not drive "
        "day 0's min/max; they become the rest-of-day extremes, while tempAvg "
        "(the base color) still reflects the full-day daily values.");
    g_mock_http_payload = restOfDayPayload(
        "[55, 48, 40]", "[0, 0, 0]", 3);
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FLOAT_EQ(days[0].tempMin, 40.0f);
    EXPECT_FLOAT_EQ(days[0].tempMax, 55.0f);
    EXPECT_FLOAT_EQ(days[0].tempAvg, (99.0f + 28.0f) / 2.0f);
    EXPECT_FLOAT_EQ(days[1].tempMin, 75.0f);
}

// Description: Hourly entries at or after tomorrow's midnight are excluded, so
// tomorrow's rain or cold cannot leak into today's alerts.
TEST_F(FetchForecastTest, TomorrowHoursExcludedFromToday) {
    RecordProperty("description",
        "Hourly entries at or after tomorrow's midnight are excluded, so "
        "tomorrow's rain or cold cannot leak into today's alerts.");
    // 14:00 .. 23:00 today (10 hours), then 00:00 and 01:00 tomorrow.
    g_mock_http_payload = restOfDayPayload(
        "[60,60,60,60,60,60,60,60,60,60, 10, 10]",
        "[0,0,0,0,0,0,0,0,0,0, 100, 100]", 12);
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FLOAT_EQ(days[0].precipProb, 0.0f);
    EXPECT_FLOAT_EQ(days[0].tempMin, 60.0f);
}

// Description: Null hourly values are skipped; if every remaining hour is null
// for a field, that field falls back to its daily value instead of zero.
TEST_F(FetchForecastTest, NullHourlyValuesFallBackToDaily) {
    RecordProperty("description",
        "Null hourly values are skipped; if every remaining hour is null for a "
        "field, that field falls back to its daily value instead of zero.");
    g_mock_http_payload = restOfDayPayload(
        "[null, 50, null]", "[null, null, null]", 3);
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    EXPECT_FLOAT_EQ(days[0].tempMin, 50.0f);
    EXPECT_FLOAT_EQ(days[0].tempMax, 50.0f);
    EXPECT_FLOAT_EQ(days[0].precipProb, 90.0f) << "all-null precip keeps daily value";
}

// Description: A response without hourly data (or daily.time) leaves day 0 at
// its daily values, preserving the previous behaviour as a safe fallback.
TEST_F(FetchForecastTest, NoHourlyDataKeepsDailyValues) {
    RecordProperty("description",
        "A response without hourly data (or daily.time) leaves day 0 at its "
        "daily values, preserving the previous behaviour as a safe fallback.");
    DayForecast days[DEFAULT_NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));  // VALID_PAYLOAD has no hourly block
    EXPECT_FLOAT_EQ(days[0].tempMax, 85.0f);
    EXPECT_FLOAT_EQ(days[0].tempMin, 70.0f);
    EXPECT_FLOAT_EQ(days[0].precipProb, 10.0f);
}
