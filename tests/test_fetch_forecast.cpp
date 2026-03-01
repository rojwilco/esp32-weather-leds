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
        strncpy(cfg_latitude,  DEFAULT_LATITUDE,  sizeof(cfg_latitude));
        strncpy(cfg_longitude, DEFAULT_LONGITUDE, sizeof(cfg_longitude));
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(FetchForecastTest, ValidResponsePopulatesAllDays) {
    DayForecast days[NUM_LEDS];
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

TEST_F(FetchForecastTest, TempAvgIsArithmeticMean) {
    DayForecast days[NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    for (int i = 0; i < NUM_LEDS; i++) {
        float expected = (days[i].tempMax + days[i].tempMin) / 2.0f;
        EXPECT_FLOAT_EQ(days[i].tempAvg, expected) << "Day " << i;
    }
}

TEST_F(FetchForecastTest, HttpErrorReturnsFalse) {
    g_mock_http_code = 500;
    DayForecast days[NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

TEST_F(FetchForecastTest, BeginFailureReturnsFalse) {
    g_mock_http_begin_ok = false;
    DayForecast days[NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

TEST_F(FetchForecastTest, MalformedJsonReturnsFalse) {
    g_mock_http_payload = String("{not valid json{{");
    DayForecast days[NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

TEST_F(FetchForecastTest, MissingTemperatureArrayReturnsFalse) {
    g_mock_http_payload = String(
        R"({"daily":{"precipitation_probability_max":[10,20,30,40,50,60]}})");
    DayForecast days[NUM_LEDS];
    EXPECT_FALSE(fetchForecast(days));
}

TEST_F(FetchForecastTest, MissingPrecipArrayDefaultsToZero) {
    g_mock_http_payload = String(R"({
      "daily": {
        "temperature_2m_max": [80,81,82,83,84,85],
        "temperature_2m_min": [60,61,62,63,64,65]
      }
    })");
    DayForecast days[NUM_LEDS];
    ASSERT_TRUE(fetchForecast(days));
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_FLOAT_EQ(days[i].precipProb, 0.0f) << "Day " << i;
}

TEST_F(FetchForecastTest, DifferentLocationStillParses) {
    // Verify that changing lat/lon doesn't break parsing
    // (the mock always returns VALID_PAYLOAD regardless of the URL)
    strncpy(cfg_latitude,  "51.5074", sizeof(cfg_latitude));
    strncpy(cfg_longitude, "-0.1278", sizeof(cfg_longitude));
    DayForecast days[NUM_LEDS];
    EXPECT_TRUE(fetchForecast(days));
}
