#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Helper ────────────────────────────────────────────────────────────────────

static String makePayload(
    float mx[NUM_LEDS], float mn[NUM_LEDS], float pr[NUM_LEDS])
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"daily\":{"
        "\"temperature_2m_max\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
        "\"temperature_2m_min\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
        "\"precipitation_probability_max\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f]"
        "}}",
        mx[0],mx[1],mx[2],mx[3],mx[4],mx[5],
        mn[0],mn[1],mn[2],mn[3],mn[4],mn[5],
        pr[0],pr[1],pr[2],pr[3],pr[4],pr[5]);
    return String(buf);
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class PollWeatherTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_http_begin_ok = true;
        g_mock_http_code     = HTTP_CODE_OK;
        g_mock_millis        = 0;
        g_mock_wifi_status   = WL_CONNECTED;
        FastLED.resetShowCount();
        cfg_freeze_thr = DEFAULT_FREEZE_THR_F;
        cfg_heat_thr   = DEFAULT_HEAT_THR_F;
        cfg_precip_thr = DEFAULT_PRECIP_THR_PCT;
        cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
        cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
    }

    void setAllDays(float tempMax, float tempMin, float precip) {
        float mx[6], mn[6], pr[6];
        for (int i = 0; i < 6; i++) { mx[i]=tempMax; mn[i]=tempMin; pr[i]=precip; }
        g_mock_http_payload = makePayload(mx, mn, pr);
    }
};

// ── Alert priority ────────────────────────────────────────────────────────────

TEST_F(PollWeatherTest, NoAlertForNormalConditions) {
    setAllDays(75.0f, 55.0f, 20.0f);
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_NONE) << "LED " << i;
}

TEST_F(PollWeatherTest, RainAlertAtThreshold) {
    setAllDays(75.0f, 55.0f, 50.0f);   // precip == cfg_precip_thr
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

TEST_F(PollWeatherTest, RainAlertAboveThreshold) {
    setAllDays(75.0f, 55.0f, 80.0f);
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

TEST_F(PollWeatherTest, FreezeAlertBelowThreshold) {
    setAllDays(50.0f, 28.0f, 10.0f);
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

TEST_F(PollWeatherTest, FreezeAlertAtThreshold) {
    setAllDays(50.0f, 32.0f, 10.0f);   // tempMin == cfg_freeze_thr
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

TEST_F(PollWeatherTest, HeatAlertAtThreshold) {
    setAllDays(95.0f, 70.0f, 10.0f);   // tempMax == cfg_heat_thr
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_HEAT) << "LED " << i;
}

TEST_F(PollWeatherTest, HeatAlertAboveThreshold) {
    setAllDays(100.0f, 75.0f, 10.0f);
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_HEAT) << "LED " << i;
}

TEST_F(PollWeatherTest, RainBeatsFreezeAndHeat) {
    setAllDays(100.0f, 28.0f, 70.0f);  // all three conditions simultaneously
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

TEST_F(PollWeatherTest, FreezeBeatsHeat) {
    setAllDays(100.0f, 28.0f, 10.0f);  // high max AND low min, no rain
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

// ── LED state initialisation ──────────────────────────────────────────────────

TEST_F(PollWeatherTest, SuccessCallsShowOnce) {
    setAllDays(75.0f, 55.0f, 10.0f);
    FastLED.resetShowCount();
    pollWeather();
    EXPECT_EQ(FastLED.showCount, 1);
}

TEST_F(PollWeatherTest, FailedFetchShowsDimWhite) {
    g_mock_http_code = 500;
    FastLED.resetShowCount();
    pollWeather();
    EXPECT_EQ(FastLED.showCount, 1);
    for (int i = 0; i < NUM_LEDS; i++) {
        EXPECT_EQ(leds[i], CRGB(10, 10, 10)) << "LED " << i;
        EXPECT_EQ(ledStates[i].alert, ALERT_NONE) << "LED " << i;
    }
}

TEST_F(PollWeatherTest, HotDayBaseColorIsReddish) {
    setAllDays(90.0f, 90.0f, 10.0f);   // avg == 90 F -> hue 0 -> red
    pollWeather();
    EXPECT_EQ(ledStates[0].baseColor.r, 255);
    EXPECT_EQ(ledStates[0].baseColor.b, 0);
}

TEST_F(PollWeatherTest, AnimationInitialisedAtPollTime) {
    setAllDays(100.0f, 28.0f, 70.0f);   // rain alert
    g_mock_millis = 5000;
    pollWeather();
    for (int i = 0; i < NUM_LEDS; i++) {
        EXPECT_EQ(ledStates[i].blendAmt, 0)                           << "LED " << i;
        EXPECT_EQ(ledStates[i].fadeDir,  1)                           << "LED " << i;
        EXPECT_EQ(ledStates[i].holdUntil, 5000UL + HOLD_MS)           << "LED " << i;
    }
}

TEST_F(PollWeatherTest, PerLedAlertsAreIndependent) {
    float mx[6] = {75.0f, 100.0f, 45.0f, 70.0f, 70.0f, 70.0f};
    float mn[6] = {55.0f,  70.0f, 28.0f, 50.0f, 50.0f, 50.0f};
    float pr[6] = {60.0f,  10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    g_mock_http_payload = makePayload(mx, mn, pr);
    pollWeather();

    EXPECT_EQ(ledStates[0].alert, ALERT_RAIN)   << "LED 0";
    EXPECT_EQ(ledStates[1].alert, ALERT_HEAT)   << "LED 1";
    EXPECT_EQ(ledStates[2].alert, ALERT_FREEZE) << "LED 2";
    EXPECT_EQ(ledStates[3].alert, ALERT_NONE)   << "LED 3";
    EXPECT_EQ(ledStates[4].alert, ALERT_NONE)   << "LED 4";
    EXPECT_EQ(ledStates[5].alert, ALERT_NONE)   << "LED 5";
}
