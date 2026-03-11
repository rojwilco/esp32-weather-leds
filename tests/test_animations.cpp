#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Fixture ───────────────────────────────────────────────────────────────────

class AnimationsTest : public ::testing::Test {
protected:
    // Compute the fade tick interval from the current cfg_fade_sec setting,
    // matching the formula used by tickAnimations().
    unsigned long fadeIntervalMs() const {
        unsigned long ms = (unsigned long)(cfg_fade_sec * 1000.0f * FADE_STEP / 255.0f + 0.5f);
        return ms < 1 ? 1 : ms;
    }

    void SetUp() override {
        g_mock_millis = 0;
        cfg_num_leds  = DEFAULT_NUM_LEDS;
        cfg_hold_sec       = DEFAULT_HOLD_SEC;
        cfg_alert_hold_sec = DEFAULT_ALERT_HOLD_SEC;
        cfg_fade_sec       = DEFAULT_FADE_SEC;
        FastLED.resetShowCount();
        for (int i = 0; i < MAX_LEDS; i++) {
            leds[i]               = CRGB(0, 0, 0);
            ledStates[i].alert    = ALERT_NONE;
            ledStates[i].blendAmt = 0;
            ledStates[i].fadeDir  = 1;
            ledStates[i].lastTick = 0;
            ledStates[i].holdUntil      = 0;
            ledStates[i].alertHoldUntil = 0;
            ledStates[i].baseColor  = CRGB(0, 0, 255);   // blue
            ledStates[i].alertColor = CRGB(255, 0, 0);   // red
        }
    }

    // Set LED i to active alert, hold phase already expired.
    void makeActive(int i, AlertType alert = ALERT_RAIN) {
        ledStates[i].alert          = alert;
        ledStates[i].blendAmt       = 0;
        ledStates[i].fadeDir        = 1;
        ledStates[i].lastTick       = 0;
        ledStates[i].holdUntil      = 0;
        ledStates[i].alertHoldUntil = 0;
    }

    void tick(unsigned long elapsedMs = 0) {
        g_mock_millis += elapsedMs ? elapsedMs : fadeIntervalMs() + 1;
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

// Description: If less than the computed fade tick interval has elapsed since the last tick,
// the LED is not updated, preventing animation from running faster than intended.
TEST_F(AnimationsTest, FadeIntervalThrottlesUpdates) {
    RecordProperty("description",
        "If less than the computed fade tick interval has elapsed since the last tick, "
        "the LED is not updated, preventing animation from running faster than intended.");
    makeActive(0);
    g_mock_millis += 1;   // only 1 ms — below default fadeIntervalMs (~6ms)
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
// back to +1 and holdUntil is reset to now + cfg_hold_sec*1000 to pause before the next cycle.
TEST_F(AnimationsTest, ReachingZeroRestartsHold) {
    RecordProperty("description",
        "When blendAmt reaches 0 in the fade-out phase, fadeDir flips back to "
        "+1 and holdUntil is reset to now + cfg_hold_sec*1000 to pause before the next cycle.");
    makeActive(0);
    ledStates[0].blendAmt = FADE_STEP;   // one step from zero
    ledStates[0].fadeDir  = -1;
    g_mock_millis = 10000;
    unsigned long elapsed = fadeIntervalMs() + 1;
    g_mock_millis += elapsed;
    tickAnimations();
    EXPECT_EQ(ledStates[0].blendAmt, 0);
    EXPECT_EQ(ledStates[0].fadeDir,  1);
    EXPECT_EQ(ledStates[0].holdUntil, 10000UL + elapsed + (unsigned long)(cfg_hold_sec * 1000.0f));
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

// Description: A longer cfg_hold_sec keeps the LED in the hold phase for the
// configured duration rather than the old compile-time constant.
TEST_F(AnimationsTest, LongerHoldSecExtendsHoldPhase) {
    RecordProperty("description",
        "A longer cfg_hold_sec keeps the LED in the hold phase for the "
        "configured duration rather than the old compile-time constant.");
    cfg_hold_sec = 10.0f;   // 10-second hold
    makeActive(0);
    ledStates[0].holdUntil = (unsigned long)(cfg_hold_sec * 1000.0f);   // 10 000 ms
    leds[0] = CRGB(0, 0, 255);
    // Advance only 5 s — still within hold
    g_mock_millis = 5000;
    tickAnimations();
    EXPECT_EQ(leds[0], CRGB(0, 0, 255));   // unchanged; hold not expired
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: A shorter cfg_hold_sec causes holdUntil to be set closer in
// time so the animation loop resumes sooner after blendAmt returns to 0.
TEST_F(AnimationsTest, ShorterHoldSecSetsNearerHoldUntil) {
    RecordProperty("description",
        "A shorter cfg_hold_sec causes holdUntil to be set closer in time so "
        "the animation loop resumes sooner after blendAmt returns to 0.");
    cfg_hold_sec = 0.5f;   // 500 ms hold
    makeActive(0);
    ledStates[0].blendAmt = FADE_STEP;
    ledStates[0].fadeDir  = -1;
    g_mock_millis = 1000;
    unsigned long elapsed = fadeIntervalMs() + 1;
    g_mock_millis += elapsed;
    tickAnimations();
    EXPECT_EQ(ledStates[0].holdUntil, 1000UL + elapsed + 500UL);
}

// Description: A faster cfg_fade_sec reduces the tick interval so the LED
// updates on every eligible call rather than waiting the default ~6 ms.
TEST_F(AnimationsTest, FasterFadeSecReducesTickInterval) {
    RecordProperty("description",
        "A faster cfg_fade_sec reduces the tick interval so the LED updates on "
        "every eligible call rather than waiting the default ~6 ms.");
    cfg_fade_sec = 0.1f;   // very short fade → tiny interval (~1 ms)
    makeActive(0);
    // Advance by exactly 1 ms — should be enough for a fast fade
    g_mock_millis += 2;
    tickAnimations();
    EXPECT_GT(ledStates[0].blendAmt, 0);   // animation advanced
}

// Description: A slower cfg_fade_sec increases the tick interval so the LED
// does not advance within a small elapsed time.
TEST_F(AnimationsTest, SlowerFadeSecIncreasesTickInterval) {
    RecordProperty("description",
        "A slower cfg_fade_sec increases the tick interval so the LED does not "
        "advance within a small elapsed time.");
    cfg_fade_sec = 5.0f;   // slow fade → interval ≈ 59 ms
    makeActive(0);
    // Advance only 10 ms — below the ~59 ms interval for a 5 s fade
    g_mock_millis += 10;
    tickAnimations();
    EXPECT_EQ(ledStates[0].blendAmt, 0);   // not yet updated
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: When blendAmt reaches 255, alertHoldUntil is set to
// now + cfg_alert_hold_sec*1000 so the LED pauses at full alert color.
TEST_F(AnimationsTest, AlertHoldUntilSetWhenBlendPeaks) {
    RecordProperty("description",
        "When blendAmt reaches 255, alertHoldUntil is set to "
        "now + cfg_alert_hold_sec*1000 so the LED pauses at full alert color.");
    makeActive(0);
    ledStates[0].blendAmt = 255 - FADE_STEP;   // one step from peak
    ledStates[0].fadeDir  = 1;
    g_mock_millis = 2000;
    unsigned long elapsed = fadeIntervalMs() + 1;
    g_mock_millis += elapsed;
    tickAnimations();
    EXPECT_EQ(ledStates[0].blendAmt, 255);
    EXPECT_EQ(ledStates[0].alertHoldUntil,
              2000UL + elapsed + (unsigned long)(cfg_alert_hold_sec * 1000.0f));
}

// Description: While alertHoldUntil is in the future, blendAmt stays at 255
// and show() is not called, holding the LED on the alert color.
TEST_F(AnimationsTest, AlertHoldBlocksReversal) {
    RecordProperty("description",
        "While alertHoldUntil is in the future, blendAmt stays at 255 and "
        "show() is not called, holding the LED on the alert color.");
    makeActive(0);
    ledStates[0].blendAmt       = 255;
    ledStates[0].fadeDir        = -1;
    ledStates[0].alertHoldUntil = 99999;   // far in the future
    leds[0] = blend(ledStates[0].baseColor, ledStates[0].alertColor, 255);
    CRGB before = leds[0];
    tick();
    EXPECT_EQ(leds[0], before);           // color unchanged during alert hold
    EXPECT_EQ(ledStates[0].blendAmt, 255);
    EXPECT_EQ(FastLED.showCount, 0);
}

// Description: Once alertHoldUntil passes, blendAmt begins decreasing on the
// next eligible tick, starting the fade-out back to the temperature color.
TEST_F(AnimationsTest, AlertHoldExpiryAllowsFadeOut) {
    RecordProperty("description",
        "Once alertHoldUntil passes, blendAmt begins decreasing on the next "
        "eligible tick, starting the fade-out back to the temperature color.");
    cfg_alert_hold_sec = 0.1f;   // 100 ms hold
    makeActive(0);
    ledStates[0].blendAmt       = 255;
    ledStates[0].fadeDir        = -1;
    ledStates[0].alertHoldUntil = 500;
    g_mock_millis = 501;   // just past the alert hold
    g_mock_millis += fadeIntervalMs() + 1;
    tickAnimations();
    EXPECT_LT(ledStates[0].blendAmt, 255);   // fade-out has started
}
