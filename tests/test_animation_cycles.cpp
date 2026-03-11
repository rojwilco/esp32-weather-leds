#include <gtest/gtest.h>
#include <vector>
#include "sketch_api.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

// Compute the fade tick interval for the current cfg_fade_sec, matching
// tickAnimations() exactly.
static unsigned long fadeIntervalMs() {
    unsigned long ms = (unsigned long)(cfg_fade_sec * 1000.0f * FADE_STEP / 255.0f + 0.5f);
    return ms < 1 ? 1 : ms;
}

// Drive tickAnimations() forward by exactly one tick interval + 1 ms,
// returning the blendAmt of LED 0 after the tick.
static int advanceTick() {
    g_mock_millis += fadeIntervalMs() + 1;
    tickAnimations();
    return ledStates[0].blendAmt;
}

// Run until the LED is no longer in holdUntil (base-color hold phase).
// Returns the millis value when the hold phase ends.
static unsigned long waitForHoldExpiry(unsigned long holdUntil) {
    while (g_mock_millis <= holdUntil) {
        g_mock_millis += fadeIntervalMs() + 1;
        tickAnimations();
    }
    return g_mock_millis;
}

// Advance the mock clock past alertHoldUntil without running a tick, so the
// next advanceTick() call captures the first real blendAmt change after the hold.
static void waitForAlertHoldExpiry(unsigned long alertHoldUntil) {
    if (g_mock_millis <= alertHoldUntil)
        g_mock_millis = alertHoldUntil + 1;
}

// ── Parametric fixture ────────────────────────────────────────────────────────

struct CycleParams {
    float       fade_sec;
    float       alert_hold_sec;
    float       hold_sec;
    const char* name;
};

class AnimCycleTest : public ::testing::TestWithParam<CycleParams> {
protected:
    void SetUp() override {
        const CycleParams& p = GetParam();
        g_mock_millis         = 0;
        cfg_num_leds          = 1;
        cfg_fade_sec          = p.fade_sec;
        cfg_alert_hold_sec    = p.alert_hold_sec;
        cfg_hold_sec          = p.hold_sec;
        FastLED.resetShowCount();

        // Initialise LED 0: active alert, no holds pending, at base.
        leds[0]                      = CRGB(0, 0, 255);
        ledStates[0].baseColor       = CRGB(0, 0, 255);
        ledStates[0].alertColor      = CRGB(255, 0, 0);
        ledStates[0].alert           = ALERT_RAIN;
        ledStates[0].blendAmt        = 0;
        ledStates[0].fadeDir         = 1;
        ledStates[0].lastTick        = 0;
        ledStates[0].holdUntil       = 0;
        ledStates[0].alertHoldUntil  = 0;
    }

    // Collect blendAmt for every eligible tick from the current state until
    // blendAmt returns to 0 at the end of one full fade-in + fade-out cycle.
    // Stops after blendAmt hits 0 for the first time (end of cycle).
    // Alert-hold and base-hold waits are skipped transparently.
    // Returns the sequence of blendAmt values observed *after* each tick that
    // actually changes the LED, not including the terminal 0 value itself
    // (which merely resets the hold timer, not progress).
    std::vector<int> runOneCycle() {
        std::vector<int> seq;
        bool peaked = false;
        while (true) {
            // If in alert-hold, skip past it.
            if (ledStates[0].alertHoldUntil > g_mock_millis) {
                waitForAlertHoldExpiry(ledStates[0].alertHoldUntil);
            }
            int v = advanceTick();
            if (!peaked && v >= 255) {
                peaked = true;
                seq.push_back(v);
                continue;
            }
            if (peaked && v == 0) {
                // Cycle complete (blendAmt just wrapped back to 0).
                break;
            }
            seq.push_back(v);
        }
        return seq;
    }
};

INSTANTIATE_TEST_SUITE_P(
    AnimCycles, AnimCycleTest,
    ::testing::Values(
        CycleParams{0.5f,  0.0f, 3.0f,  "default"},
        CycleParams{0.1f,  0.0f, 3.0f,  "fast_fade"},
        CycleParams{2.0f,  0.0f, 3.0f,  "slow_fade"},
        CycleParams{0.5f,  1.0f, 3.0f,  "with_alert_hold"},
        CycleParams{0.5f,  0.0f, 0.5f,  "short_base_hold"},
        CycleParams{0.25f, 0.5f, 5.0f,  "nerdy_combo"}
    ),
    [](const ::testing::TestParamInfo<CycleParams>& info) {
        return info.param.name;
    }
);

// ── Tests ─────────────────────────────────────────────────────────────────────

// Description: With FADE_STEP=1, one complete fade-in + fade-out cycle always
// visits exactly 254 intermediate blendAmt values (1..254) plus the peak at
// 255, for a total of 255 ticks up and 255 ticks down (510 ticks total),
// regardless of the timing settings.
TEST_P(AnimCycleTest, CycleHasExactly510Steps) {
    RecordProperty("description",
        "With FADE_STEP=1, one complete fade-in + fade-out cycle always visits "
        "exactly 254 intermediate blendAmt values (1..254) plus the peak at 255, "
        "for a total of 255 ticks up and 255 ticks down (510 ticks total), "
        "regardless of the timing settings.");
    std::vector<int> seq = runOneCycle();
    // seq contains [1..255, 254..1] = 255 + 254 = 509 values.
    EXPECT_EQ(seq.size(), 509u);
}

// Description: The blendAmt values produced during a full cycle are exactly
// [1, 2, …, 255, 254, …, 1] in order — the shape is invariant across all
// timing settings; only the wall-clock duration changes.
TEST_P(AnimCycleTest, BlendSequenceIsCorrect) {
    RecordProperty("description",
        "The blendAmt values produced during a full cycle are exactly "
        "[1, 2, ..., 255, 254, ..., 1] in order — the shape is invariant across "
        "all timing settings; only the wall-clock duration changes.");
    std::vector<int> seq = runOneCycle();
    std::vector<int> expected;
    for (int v = 1; v <= 255; ++v) expected.push_back(v);
    for (int v = 254; v >= 1;  --v) expected.push_back(v);
    EXPECT_EQ(seq, expected);
}

// Description: When blendAmt first reaches 255 the alertHoldUntil timestamp is
// set to millis() + cfg_alert_hold_sec*1000; with alert_hold_sec=0 the hold is
// zero-length (alertHoldUntil == now), so reversal is immediate.
TEST_P(AnimCycleTest, AlertHoldUntilSetAtPeak) {
    RecordProperty("description",
        "When blendAmt first reaches 255 the alertHoldUntil timestamp is set to "
        "millis() + cfg_alert_hold_sec*1000; with alert_hold_sec=0 the hold is "
        "zero-length (alertHoldUntil == now), so reversal is immediate.");

    // Advance until blendAmt reaches 255.
    while (ledStates[0].blendAmt < 255) {
        advanceTick();
    }
    unsigned long peakTime     = g_mock_millis;
    unsigned long expectedHold = peakTime + (unsigned long)(cfg_alert_hold_sec * 1000.0f);
    EXPECT_EQ(ledStates[0].alertHoldUntil, expectedHold);
    EXPECT_EQ(ledStates[0].fadeDir, -1);
}

// Description: After a full cycle blendAmt returns to 0 and holdUntil is set
// to millis() + cfg_hold_sec*1000, pausing the LED on its temperature color
// for the configured hold duration before the next cycle begins.
TEST_P(AnimCycleTest, BaseHoldSetAfterCycleEnd) {
    RecordProperty("description",
        "After a full cycle blendAmt returns to 0 and holdUntil is set to "
        "millis() + cfg_hold_sec*1000, pausing the LED on its temperature color "
        "for the configured hold duration before the next cycle begins.");

    // Run until blendAmt is on the way down and one step from 0.
    while (!(ledStates[0].fadeDir == -1 && ledStates[0].blendAmt == FADE_STEP)) {
        if (ledStates[0].alertHoldUntil > g_mock_millis)
            waitForAlertHoldExpiry(ledStates[0].alertHoldUntil);
        advanceTick();
    }
    // One more tick drives blendAmt to 0 and sets holdUntil.
    g_mock_millis += fadeIntervalMs() + 1;
    tickAnimations();
    unsigned long endTime = g_mock_millis;

    EXPECT_EQ(ledStates[0].blendAmt, 0);
    EXPECT_EQ(ledStates[0].fadeDir,  1);
    EXPECT_EQ(ledStates[0].holdUntil,
              endTime + (unsigned long)(cfg_hold_sec * 1000.0f));
}

// Description: After the cycle ends the LED holds at its base (temperature)
// color for exactly cfg_hold_sec seconds — ticks during the hold do not
// advance blendAmt, and show() is not called during that window.
TEST_P(AnimCycleTest, HoldPhaseBlocksProgressAfterCycle) {
    RecordProperty("description",
        "After the cycle ends the LED holds at its base (temperature) color for "
        "exactly cfg_hold_sec seconds — ticks during the hold do not advance "
        "blendAmt, and show() is not called during that window.");

    // Run a full cycle so holdUntil is set.
    runOneCycle();
    unsigned long holdUntil = ledStates[0].holdUntil;

    FastLED.resetShowCount();
    int blendBefore = ledStates[0].blendAmt;   // should be 0

    // Advance to just before holdUntil expires.
    if (holdUntil > g_mock_millis + 1) {
        g_mock_millis = holdUntil - 1;
        tickAnimations();
        EXPECT_EQ(ledStates[0].blendAmt, blendBefore);
        EXPECT_EQ(FastLED.showCount, 0);
    }
}
