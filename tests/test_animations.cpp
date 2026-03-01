#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Fixture ───────────────────────────────────────────────────────────────────

class AnimationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_millis = 0;
        FastLED.resetShowCount();
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i]               = CRGB(0, 0, 0);
            ledStates[i].alert    = ALERT_NONE;
            ledStates[i].blendAmt = 0;
            ledStates[i].fadeDir  = 1;
            ledStates[i].lastTick = 0;
            ledStates[i].holdUntil  = 0;
            ledStates[i].baseColor  = CRGB(0, 0, 255);   // blue
            ledStates[i].alertColor = CRGB(255, 0, 0);   // red
        }
    }

    // Set LED i to active alert, hold phase already expired.
    void makeActive(int i, AlertType alert = ALERT_RAIN) {
        ledStates[i].alert     = alert;
        ledStates[i].blendAmt  = 0;
        ledStates[i].fadeDir   = 1;
        ledStates[i].lastTick  = 0;
        ledStates[i].holdUntil = 0;
    }

    void tick(unsigned long elapsedMs = FADE_INTERVAL_MS + 1) {
        g_mock_millis += elapsedMs;
        tickAnimations();
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(AnimationsTest, NoAlertLedUnchanged) {
    ledStates[0].alert = ALERT_NONE;
    leds[0] = CRGB(100, 100, 100);
    tick();
    EXPECT_EQ(leds[0], CRGB(100, 100, 100));
    EXPECT_EQ(FastLED.showCount, 0);
}

TEST_F(AnimationsTest, HoldPhaseBlocksUpdate) {
    makeActive(0);
    ledStates[0].holdUntil = 99999;   // far in the future
    leds[0] = CRGB(0, 0, 255);
    tick();
    EXPECT_EQ(leds[0], CRGB(0, 0, 255));   // unchanged during hold
    EXPECT_EQ(FastLED.showCount, 0);
}

TEST_F(AnimationsTest, FadeIntervalThrottlesUpdates) {
    makeActive(0);
    g_mock_millis += 1;   // only 1 ms — below FADE_INTERVAL_MS
    tickAnimations();
    EXPECT_EQ(FastLED.showCount, 0);
}

TEST_F(AnimationsTest, BlendAmountIncreasesEachTick) {
    makeActive(0);
    EXPECT_EQ(ledStates[0].blendAmt, 0);
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, FADE_STEP);
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 2 * FADE_STEP);
}

TEST_F(AnimationsTest, BlendAmountCapsAt255AndReverses) {
    makeActive(0);
    ledStates[0].blendAmt = 254;
    ledStates[0].fadeDir  = 1;
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 255);
    EXPECT_EQ(ledStates[0].fadeDir,  -1);
}

TEST_F(AnimationsTest, BlendAmountDecreasesInReversePhase) {
    makeActive(0);
    ledStates[0].blendAmt = 255;
    ledStates[0].fadeDir  = -1;
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 255 - FADE_STEP);
    EXPECT_EQ(ledStates[0].fadeDir,  -1);
}

TEST_F(AnimationsTest, ReachingZeroRestartsHold) {
    makeActive(0);
    ledStates[0].blendAmt = FADE_STEP;   // one step from zero
    ledStates[0].fadeDir  = -1;
    g_mock_millis = 10000;
    unsigned long elapsed = FADE_INTERVAL_MS + 1;
    g_mock_millis += elapsed;
    tickAnimations();
    EXPECT_EQ(ledStates[0].blendAmt, 0);
    EXPECT_EQ(ledStates[0].fadeDir,  1);
    EXPECT_EQ(ledStates[0].holdUntil, 10000UL + elapsed + HOLD_MS);
}

TEST_F(AnimationsTest, LedColorIsBlendOfBaseAndAlert) {
    makeActive(0);
    ledStates[0].blendAmt   = 0;
    ledStates[0].baseColor  = CRGB(0, 0, 255);
    ledStates[0].alertColor = CRGB(255, 0, 0);
    tick();   // blendAmt goes 0 -> FADE_STEP
    CRGB expected = blend(CRGB(0, 0, 255), CRGB(255, 0, 0), FADE_STEP);
    EXPECT_EQ(leds[0], expected);
}

TEST_F(AnimationsTest, ShowCalledOncePerTickWithChanges) {
    makeActive(0);
    tick();
    EXPECT_EQ(FastLED.showCount, 1);
    tick();
    EXPECT_EQ(FastLED.showCount, 2);
}

TEST_F(AnimationsTest, MultipleActiveLedsTriggerOneShow) {
    makeActive(0, ALERT_RAIN);
    makeActive(1, ALERT_HEAT);
    makeActive(2, ALERT_FREEZE);
    tick();
    // Three LEDs changed, but show() batches into one call per tick
    EXPECT_EQ(FastLED.showCount, 1);
}

TEST_F(AnimationsTest, NonActiveLedUnaffectedByNeighbour) {
    makeActive(0, ALERT_RAIN);
    ledStates[1].alert = ALERT_NONE;
    leds[1] = CRGB(50, 100, 150);
    tick();
    EXPECT_EQ(leds[1], CRGB(50, 100, 150));
}
