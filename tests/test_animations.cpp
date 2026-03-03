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

// Description: An LED with ALERT_NONE is not modified by tickAnimations() and
// show() is not called, so non-alert LEDs are never touched.
TEST_F(AnimationsTest, NoAlertLedUnchanged) {
    RecordProperty("description",
        "An LED with ALERT_NONE is not modified by tickAnimations() and "
        "show() is not called, so non-alert LEDs are never touched.");
    ledStates[0].alert = ALERT_NONE;
    leds[0] = CRGB(100, 100, 100);
    tick();
    EXPECT_EQ(leds[0], CRGB(100, 100, 100));
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: While holdUntil is in the future, the LED color does not change
// and show() is not called, pausing the animation during the hold phase.
TEST_F(AnimationsTest, HoldPhaseBlocksUpdate) {
    RecordProperty("description",
        "While holdUntil is in the future, the LED color does not change and "
        "show() is not called, pausing the animation during the hold phase.");
    makeActive(0);
    ledStates[0].holdUntil = 99999;   // far in the future
    leds[0] = CRGB(0, 0, 255);
    tick();
    EXPECT_EQ(leds[0], CRGB(0, 0, 255));   // unchanged during hold
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: If less than FADE_INTERVAL_MS has elapsed since the last tick,
// the LED is not updated, preventing animation from running faster than intended.
TEST_F(AnimationsTest, FadeIntervalThrottlesUpdates) {
    RecordProperty("description",
        "If less than FADE_INTERVAL_MS has elapsed since the last tick, "
        "the LED is not updated, preventing animation from running faster than intended.");
    makeActive(0);
    g_mock_millis += 1;   // only 1 ms — below FADE_INTERVAL_MS
    tickAnimations();
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: Each call to tickAnimations() increments blendAmt by FADE_STEP
// when fadeDir is +1, progressing the LED from base color toward alert color.
TEST_F(AnimationsTest, BlendAmountIncreasesEachTick) {
    RecordProperty("description",
        "Each call to tickAnimations() increments blendAmt by FADE_STEP when "
        "fadeDir is +1, progressing the LED from base color toward alert color.");
    makeActive(0);
    EXPECT_EQ(ledStates[0].blendAmt, 0);
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, FADE_STEP);
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 2 * FADE_STEP);
}

// Description: When blendAmt reaches 255 (fully alert color), it is clamped to
// 255 and fadeDir flips to -1 to begin the fade-out phase.
TEST_F(AnimationsTest, BlendAmountCapsAt255AndReverses) {
    RecordProperty("description",
        "When blendAmt reaches 255 (fully alert color), it is clamped to 255 "
        "and fadeDir flips to -1 to begin the fade-out phase.");
    makeActive(0);
    ledStates[0].blendAmt = 254;
    ledStates[0].fadeDir  = 1;
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 255);
    EXPECT_EQ(ledStates[0].fadeDir,  -1);
}

// Description: When fadeDir is -1, each tick decrements blendAmt by FADE_STEP,
// fading the LED back from alert color toward base color.
TEST_F(AnimationsTest, BlendAmountDecreasesInReversePhase) {
    RecordProperty("description",
        "When fadeDir is -1, each tick decrements blendAmt by FADE_STEP, "
        "fading the LED back from alert color toward base color.");
    makeActive(0);
    ledStates[0].blendAmt = 255;
    ledStates[0].fadeDir  = -1;
    tick();
    EXPECT_EQ(ledStates[0].blendAmt, 255 - FADE_STEP);
    EXPECT_EQ(ledStates[0].fadeDir,  -1);
}

// Description: When blendAmt reaches 0 in the fade-out phase, fadeDir flips
// back to +1 and holdUntil is reset to now + HOLD_MS to pause before the next cycle.
TEST_F(AnimationsTest, ReachingZeroRestartsHold) {
    RecordProperty("description",
        "When blendAmt reaches 0 in the fade-out phase, fadeDir flips back to "
        "+1 and holdUntil is reset to now + HOLD_MS to pause before the next cycle.");
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

// Description: The LED's displayed color is the CRGB blend of baseColor and
// alertColor at the current blendAmt ratio, verified after one tick.
TEST_F(AnimationsTest, LedColorIsBlendOfBaseAndAlert) {
    RecordProperty("description",
        "The LED's displayed color is the CRGB blend of baseColor and alertColor "
        "at the current blendAmt ratio, verified after one tick.");
    makeActive(0);
    ledStates[0].blendAmt   = 0;
    ledStates[0].baseColor  = CRGB(0, 0, 255);
    ledStates[0].alertColor = CRGB(255, 0, 0);
    tick();   // blendAmt goes 0 -> FADE_STEP
    CRGB expected = blend(CRGB(0, 0, 255), CRGB(255, 0, 0), FADE_STEP);
    EXPECT_EQ(leds[0], expected);
}

// Description: A single tickAnimations() call with one active LED calls
// show() exactly once; subsequent ticks each add one more show() call.
TEST_F(AnimationsTest, ShowCalledOncePerTickWithChanges) {
    RecordProperty("description",
        "A single tickAnimations() call with one active LED calls show() exactly "
        "once; subsequent ticks each add one more show() call.");
    makeActive(0);
    tick();
    EXPECT_EQ(FastLED.showCount, 1);
    tick();
    EXPECT_EQ(FastLED.showCount, 2);
}

// Description: Multiple active alert LEDs are all updated in a single
// tickAnimations() call, with exactly one show() call batching all changes.
TEST_F(AnimationsTest, MultipleActiveLedsTriggerOneShow) {
    RecordProperty("description",
        "Multiple active alert LEDs are all updated in a single tickAnimations() "
        "call, with exactly one show() call batching all changes.");
    makeActive(0, ALERT_RAIN);
    makeActive(1, ALERT_HEAT);
    makeActive(2, ALERT_FREEZE);
    tick();
    // Three LEDs changed, but show() batches into one call per tick
    EXPECT_EQ(FastLED.showCount, 1);
}

// Description: An LED with ALERT_NONE is not modified when a neighboring LED
// is actively animating, confirming per-LED independence.
TEST_F(AnimationsTest, NonActiveLedUnaffectedByNeighbour) {
    RecordProperty("description",
        "An LED with ALERT_NONE is not modified when a neighboring LED is "
        "actively animating, confirming per-LED independence.");
    makeActive(0, ALERT_RAIN);
    ledStates[1].alert = ALERT_NONE;
    leds[1] = CRGB(50, 100, 150);
    tick();
    EXPECT_EQ(leds[1], CRGB(50, 100, 150));
}
