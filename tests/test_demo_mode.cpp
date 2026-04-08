#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Fixture ───────────────────────────────────────────────────────────────────

class DemoModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_demo_mode  = false;
        g_forceRepoll = false;
        cfg_num_leds        = DEFAULT_NUM_LEDS;  // 6
        cfg_cold_temp       = DEFAULT_COLD_TEMP_F;
        cfg_hot_temp        = DEFAULT_HOT_TEMP_F;
        cfg_freeze_thr      = DEFAULT_FREEZE_THR_F;
        cfg_heat_thr        = DEFAULT_HEAT_THR_F;
        cfg_precip_thr      = DEFAULT_PRECIP_THR_PCT;
        cfg_hold_sec        = DEFAULT_HOLD_SEC;
        cfg_alert_hold_sec  = DEFAULT_ALERT_HOLD_SEC;
        cfg_attack_sec      = DEFAULT_ATTACK_SEC;
        cfg_decay_sec       = DEFAULT_DECAY_SEC;
        cfg_freeze_color    = DEFAULT_FREEZE_COLOR;
        cfg_heat_color      = DEFAULT_HEAT_COLOR;
        cfg_rain_color      = DEFAULT_RAIN_COLOR;
        g_mock_millis       = 0;
        g_mock_last_send_code = 0;
        g_mock_last_redirect  = "";
        g_mock_server_args.clear();
        g_mock_prefs_store.clear();
        FastLED.resetShowCount();
    }
};

// ── Gradient tests ────────────────────────────────────────────────────────────

// Description: With 6 LEDs, LED 0 receives a base color mapped from cfg_cold_temp,
// which produces hue 160 (blue end of the cold-to-hot spectrum).
TEST_F(DemoModeTest, ColdEndIsBlue) {
    RecordProperty("description",
        "With 6 LEDs, LED 0 receives a base color mapped from cfg_cold_temp, "
        "which produces hue 160 (blue end of the cold-to-hot spectrum).");
    pollDemoMode();
    EXPECT_EQ(ledStates[0].baseColor._hue, 160);
}

// Description: With 6 LEDs, LED 5 receives a base color mapped from cfg_hot_temp,
// which produces hue 0 (red end of the cold-to-hot spectrum).
TEST_F(DemoModeTest, HotEndIsRed) {
    RecordProperty("description",
        "With 6 LEDs, LED 5 receives a base color mapped from cfg_hot_temp, "
        "which produces hue 0 (red end of the cold-to-hot spectrum).");
    pollDemoMode();
    EXPECT_EQ(ledStates[cfg_num_leds - 1].baseColor._hue, 0);
}

// ── Alert placement tests ─────────────────────────────────────────────────────

// Description: LED 0 (coldest end) always receives ALERT_FREEZE in demo mode,
// making the freeze indicator visually aligned with the cold/blue base color.
TEST_F(DemoModeTest, FreezeAlertAtIndex0) {
    RecordProperty("description",
        "LED 0 (coldest end) always receives ALERT_FREEZE in demo mode, "
        "making the freeze indicator visually aligned with the cold/blue base color.");
    pollDemoMode();
    EXPECT_EQ(ledStates[0].alert, ALERT_FREEZE);
}

// Description: LED n/2 (middle) receives ALERT_RAIN in demo mode with n=6, so
// the rain indicator appears at index 3, centered between cold and hot ends.
TEST_F(DemoModeTest, RainAlertAtMiddle) {
    RecordProperty("description",
        "LED n/2 (middle) receives ALERT_RAIN in demo mode with n=6, so the "
        "rain indicator appears at index 3, centered between cold and hot ends.");
    pollDemoMode();
    int midIdx = cfg_num_leds / 2;
    EXPECT_EQ(ledStates[midIdx].alert, ALERT_RAIN);
}

// Description: LED n-1 (hottest end) receives ALERT_HEAT in demo mode with n=6,
// making the heat indicator visually aligned with the hot/red base color.
TEST_F(DemoModeTest, HeatAlertAtLastIndex) {
    RecordProperty("description",
        "LED n-1 (hottest end) receives ALERT_HEAT in demo mode with n=6, "
        "making the heat indicator visually aligned with the hot/red base color.");
    pollDemoMode();
    EXPECT_EQ(ledStates[cfg_num_leds - 1].alert, ALERT_HEAT);
}

// Description: The rain alert index scales dynamically with cfg_num_leds; with
// n=8 it lands at index 4, confirming the middle placement is not hard-coded.
TEST_F(DemoModeTest, RainIndexScalesWithLedCount) {
    RecordProperty("description",
        "The rain alert index scales dynamically with cfg_num_leds; with n=8 "
        "it lands at index 4, confirming the middle placement is not hard-coded.");
    cfg_num_leds = 8;
    pollDemoMode();
    EXPECT_EQ(ledStates[4].alert, ALERT_RAIN);
}

// ── Show count ────────────────────────────────────────────────────────────────

// Description: pollDemoMode() calls FastLED.show() exactly once, consistent with
// the single-show-per-poll-cycle constraint that prevents LED flicker.
TEST_F(DemoModeTest, CallsShowOnce) {
    RecordProperty("description",
        "pollDemoMode() calls FastLED.show() exactly once, consistent with "
        "the single-show-per-poll-cycle constraint that prevents LED flicker.");
    pollDemoMode();
    EXPECT_EQ(FastLED.showCount, 1);
}

// ── handleDemo() tests ────────────────────────────────────────────────────────

// Description: handleDemo() sets g_demo_mode=true when it was false, toggling
// the device into demo mode.
TEST_F(DemoModeTest, HandleDemoTogglesOn) {
    RecordProperty("description",
        "handleDemo() sets g_demo_mode=true when it was false, toggling "
        "the device into demo mode.");
    g_demo_mode = false;
    handleDemo();
    EXPECT_TRUE(g_demo_mode);
}

// Description: handleDemo() sets g_demo_mode=false when it was true, toggling
// the device back to normal weather polling.
TEST_F(DemoModeTest, HandleDemoTogglesOff) {
    RecordProperty("description",
        "handleDemo() sets g_demo_mode=false when it was true, toggling "
        "the device back to normal weather polling.");
    g_demo_mode = true;
    handleDemo();
    EXPECT_FALSE(g_demo_mode);
}

// Description: handleDemo() sets g_forceRepoll=true so the next loop() iteration
// immediately applies the new mode (demo or weather) without waiting for the
// scheduled poll interval.
TEST_F(DemoModeTest, HandleDemoSetsForceRepoll) {
    RecordProperty("description",
        "handleDemo() sets g_forceRepoll=true so the next loop() iteration "
        "immediately applies the new mode (demo or weather) without waiting "
        "for the scheduled poll interval.");
    handleDemo();
    EXPECT_TRUE(g_forceRepoll);
}

// Description: handleDemo() sends a 303 redirect to '/' so the browser reloads
// the config page and reflects the updated demo mode button state.
TEST_F(DemoModeTest, HandleDemoRedirects) {
    RecordProperty("description",
        "handleDemo() sends a 303 redirect to '/' so the browser reloads the "
        "config page and reflects the updated demo mode button state.");
    handleDemo();
    EXPECT_EQ(g_mock_last_send_code, 303);
    EXPECT_EQ(g_mock_last_redirect, String("/"));
}
