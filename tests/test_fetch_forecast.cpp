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
