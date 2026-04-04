#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Helper ────────────────────────────────────────────────────────────────────

static String makePayload(
    float mx[DEFAULT_NUM_LEDS], float mn[DEFAULT_NUM_LEDS], float pr[DEFAULT_NUM_LEDS])
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
        cfg_num_leds   = DEFAULT_NUM_LEDS;
        cfg_freeze_thr = DEFAULT_FREEZE_THR_F;
        cfg_heat_thr   = DEFAULT_HEAT_THR_F;
        cfg_precip_thr = DEFAULT_PRECIP_THR_PCT;
        cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
        cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
        cfg_hold_sec        = DEFAULT_HOLD_SEC;
        cfg_alert_hold_sec  = DEFAULT_ALERT_HOLD_SEC;
        cfg_attack_sec      = DEFAULT_ATTACK_SEC;
        cfg_decay_sec       = DEFAULT_DECAY_SEC;
        cfg_freeze_color    = DEFAULT_FREEZE_COLOR;
        cfg_heat_color      = DEFAULT_HEAT_COLOR;
        cfg_rain_color      = DEFAULT_RAIN_COLOR;
    }

    void setAllDays(float tempMax, float tempMin, float precip) {
        float mx[6], mn[6], pr[6];
        for (int i = 0; i < 6; i++) { mx[i]=tempMax; mn[i]=tempMin; pr[i]=precip; }
        g_mock_http_payload = makePayload(mx, mn, pr);
    }
};

// ── Alert priority ────────────────────────────────────────────────────────────

// Description: A day with moderate temperature and low precipitation (20% rain,
// 75°F max, 55°F min) produces ALERT_NONE on every LED.
TEST_F(PollWeatherTest, NoAlertForNormalConditions) {
    RecordProperty("description",
        "A day with moderate temperature and low precipitation (20% rain, "
        "75F max, 55F min) produces ALERT_NONE on every LED.");
    setAllDays(75.0f, 55.0f, 20.0f);
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_NONE) << "LED " << i;
}

// Description: Precipitation at exactly the rain threshold (50%) triggers
// ALERT_RAIN; the threshold boundary is inclusive.
TEST_F(PollWeatherTest, RainAlertAtThreshold) {
    RecordProperty("description",
        "Precipitation at exactly the rain threshold (50%) triggers ALERT_RAIN; "
        "the threshold boundary is inclusive.");
    setAllDays(75.0f, 55.0f, 50.0f);   // precip == cfg_precip_thr
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

// Description: Precipitation above the rain threshold (80%) triggers ALERT_RAIN
// on all LEDs.
TEST_F(PollWeatherTest, RainAlertAboveThreshold) {
    RecordProperty("description",
        "Precipitation above the rain threshold (80%) triggers ALERT_RAIN "
        "on all LEDs.");
    setAllDays(75.0f, 55.0f, 80.0f);
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

// Description: A minimum temperature below 32°F (28°F) triggers ALERT_FREEZE
// even when precipitation is low.
TEST_F(PollWeatherTest, FreezeAlertBelowThreshold) {
    RecordProperty("description",
        "A minimum temperature below 32F (28F) triggers ALERT_FREEZE "
        "even when precipitation is low.");
    setAllDays(50.0f, 28.0f, 10.0f);
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

// Description: A minimum temperature at exactly 32°F triggers ALERT_FREEZE;
// the freeze threshold boundary is inclusive.
TEST_F(PollWeatherTest, FreezeAlertAtThreshold) {
    RecordProperty("description",
        "A minimum temperature at exactly 32F triggers ALERT_FREEZE; "
        "the freeze threshold boundary is inclusive.");
    setAllDays(50.0f, 32.0f, 10.0f);   // tempMin == cfg_freeze_thr
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

// Description: A maximum temperature at exactly the heat threshold (95°F)
// triggers ALERT_HEAT; the heat threshold boundary is inclusive.
TEST_F(PollWeatherTest, HeatAlertAtThreshold) {
    RecordProperty("description",
        "A maximum temperature at exactly the heat threshold (95F) triggers "
        "ALERT_HEAT; the heat threshold boundary is inclusive.");
    setAllDays(95.0f, 70.0f, 10.0f);   // tempMax == cfg_heat_thr
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_HEAT) << "LED " << i;
}

// Description: A maximum temperature above the heat threshold (100°F) triggers
// ALERT_HEAT on all LEDs.
TEST_F(PollWeatherTest, HeatAlertAboveThreshold) {
    RecordProperty("description",
        "A maximum temperature above the heat threshold (100F) triggers "
        "ALERT_HEAT on all LEDs.");
    setAllDays(100.0f, 75.0f, 10.0f);
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_HEAT) << "LED " << i;
}

// Description: When rain, freeze, and heat conditions all apply simultaneously,
// ALERT_RAIN takes the highest priority.
TEST_F(PollWeatherTest, RainBeatsFreezeAndHeat) {
    RecordProperty("description",
        "When rain, freeze, and heat conditions all apply simultaneously, "
        "ALERT_RAIN takes the highest priority.");
    setAllDays(100.0f, 28.0f, 70.0f);  // all three conditions simultaneously
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_RAIN) << "LED " << i;
}

// Description: When both freeze and heat conditions apply but not rain,
// ALERT_FREEZE takes priority over ALERT_HEAT.
TEST_F(PollWeatherTest, FreezeBeatsHeat) {
    RecordProperty("description",
        "When both freeze and heat conditions apply but not rain, "
        "ALERT_FREEZE takes priority over ALERT_HEAT.");
    setAllDays(100.0f, 28.0f, 10.0f);  // high max AND low min, no rain
    pollWeather();
    for (int i = 0; i < cfg_num_leds; i++)
        EXPECT_EQ(ledStates[i].alert, ALERT_FREEZE) << "LED " << i;
}

// ── LED state initialisation ──────────────────────────────────────────────────

// Description: A successful weather poll calls FastLED.show() exactly once,
// regardless of how many LEDs are updated.
TEST_F(PollWeatherTest, SuccessCallsShowOnce) {
    RecordProperty("description",
        "A successful weather poll calls FastLED.show() exactly once, "
        "regardless of how many LEDs are updated.");
    setAllDays(75.0f, 55.0f, 10.0f);
    FastLED.resetShowCount();
    pollWeather();
    EXPECT_EQ(FastLED.showCount, 1);
}

// Description: When the HTTP fetch fails, all LEDs are set to a dim white
// (10,10,10) error color, ALERT_NONE is set, and show() is called once.
TEST_F(PollWeatherTest, FailedFetchShowsDimWhite) {
    RecordProperty("description",
        "When the HTTP fetch fails, all LEDs are set to a dim white (10,10,10) "
        "error color, ALERT_NONE is set, and show() is called once.");
    g_mock_http_code = 500;
    FastLED.resetShowCount();
    pollWeather();
    EXPECT_EQ(FastLED.showCount, 1);
    for (int i = 0; i < cfg_num_leds; i++) {
        EXPECT_EQ(leds[i], CRGB(10, 10, 10)) << "LED " << i;
        EXPECT_EQ(ledStates[i].alert, ALERT_NONE) << "LED " << i;
    }
}

// Description: A day with a 90°F average produces a base color with red=255 and
// blue=0, confirming the temperature-to-color mapping runs through the full poll path.
TEST_F(PollWeatherTest, HotDayBaseColorIsReddish) {
    RecordProperty("description",
        "A day with a 90F average produces a base color with red=255 and blue=0, "
        "confirming the temperature-to-color mapping runs through the full poll path.");
    setAllDays(90.0f, 90.0f, 10.0f);   // avg == 90 F -> hue 0 -> red
    pollWeather();
    EXPECT_EQ(ledStates[0].baseColor.r, 255);
    EXPECT_EQ(ledStates[0].baseColor.b, 0);
}

// Description: After a poll, alert LEDs have blendAmt=0, fadeDir=+1, holdUntil
// set to poll time + cfg_hold_sec*1000, and alertHoldUntil=0, ready to animate.
TEST_F(PollWeatherTest, AnimationInitialisedAtPollTime) {
    RecordProperty("description",
        "After a poll, alert LEDs have blendAmt=0, fadeDir=+1, holdUntil set to "
        "poll time + cfg_hold_sec*1000, and alertHoldUntil=0, ready to animate.");
    setAllDays(100.0f, 28.0f, 70.0f);   // rain alert
    g_mock_millis = 5000;
    pollWeather();
    unsigned long expectedHold = 5000UL + (unsigned long)(cfg_hold_sec * 1000.0f);
    for (int i = 0; i < cfg_num_leds; i++) {
        EXPECT_EQ(ledStates[i].blendAmt, 0)                 << "LED " << i;
        EXPECT_EQ(ledStates[i].fadeDir,  1)                 << "LED " << i;
        EXPECT_EQ(ledStates[i].holdUntil, expectedHold)     << "LED " << i;
        EXPECT_EQ(ledStates[i].alertHoldUntil, 0UL)         << "LED " << i;
    }
}

// Description: Each LED's alert is determined independently by its own day's
// data, so different LEDs can show different alert types in the same poll.
TEST_F(PollWeatherTest, PerLedAlertsAreIndependent) {
    RecordProperty("description",
        "Each LED's alert is determined independently by its own day's data, "
        "so different LEDs can show different alert types in the same poll.");
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

// Description: When cfg_freeze_color is set to a custom value, a freeze-alert
// LED's alertColor matches the custom color after pollWeather().
TEST_F(PollWeatherTest, FreezeAlertUsesConfiguredColor) {
    RecordProperty("description",
        "When cfg_freeze_color is set to a custom value, a freeze-alert LED's "
        "alertColor matches the custom color after pollWeather().");
    cfg_freeze_color = 0xAABBCCU;
    setAllDays(50.0f, 28.0f, 10.0f);  // freeze condition
    pollWeather();
    EXPECT_EQ(ledStates[0].alertColor.r, 0xAA) << "freeze alertColor red";
    EXPECT_EQ(ledStates[0].alertColor.g, 0xBB) << "freeze alertColor green";
    EXPECT_EQ(ledStates[0].alertColor.b, 0xCC) << "freeze alertColor blue";
}

// Description: When cfg_heat_color is set to a custom value, a heat-alert
// LED's alertColor matches the custom color after pollWeather().
TEST_F(PollWeatherTest, HeatAlertUsesConfiguredColor) {
    RecordProperty("description",
        "When cfg_heat_color is set to a custom value, a heat-alert LED's "
        "alertColor matches the custom color after pollWeather().");
    cfg_heat_color = 0x112233U;
    setAllDays(100.0f, 70.0f, 10.0f);  // heat condition
    pollWeather();
    EXPECT_EQ(ledStates[0].alertColor.r, 0x11) << "heat alertColor red";
    EXPECT_EQ(ledStates[0].alertColor.g, 0x22) << "heat alertColor green";
    EXPECT_EQ(ledStates[0].alertColor.b, 0x33) << "heat alertColor blue";
}

// Description: When cfg_rain_color is set to a custom value, a rain-alert
// LED's alertColor matches the custom color after pollWeather().
TEST_F(PollWeatherTest, RainAlertUsesConfiguredColor) {
    RecordProperty("description",
        "When cfg_rain_color is set to a custom value, a rain-alert LED's "
        "alertColor matches the custom color after pollWeather().");
    cfg_rain_color = 0x998877U;
    setAllDays(75.0f, 55.0f, 80.0f);  // rain condition
    pollWeather();
    EXPECT_EQ(ledStates[0].alertColor.r, 0x99) << "rain alertColor red";
    EXPECT_EQ(ledStates[0].alertColor.g, 0x88) << "rain alertColor green";
    EXPECT_EQ(ledStates[0].alertColor.b, 0x77) << "rain alertColor blue";
}

// ── Return value ──────────────────────────────────────────────────────────────

// Description: pollWeather() returns true when fetchForecast() succeeds so
// the caller can distinguish a good poll from a failed one.
TEST_F(PollWeatherTest, ReturnsTrueOnSuccess) {
    RecordProperty("description",
        "pollWeather() returns true when fetchForecast() succeeds so the "
        "caller can distinguish a good poll from a failed one.");
    setAllDays(75.0f, 55.0f, 10.0f);
    EXPECT_TRUE(pollWeather());
}

// Description: pollWeather() returns false when the HTTP fetch fails so
// the caller can schedule a shorter retry interval instead of waiting the
// full poll period.
TEST_F(PollWeatherTest, ReturnsFalseOnFetchFailure) {
    RecordProperty("description",
        "pollWeather() returns false when the HTTP fetch fails so the caller "
        "can schedule a shorter retry interval instead of waiting the full "
        "poll period.");
    g_mock_http_code = 500;
    EXPECT_FALSE(pollWeather());
}
