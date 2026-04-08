// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#include <gtest/gtest.h>
#include "sketch_api.h"

// ── Fixture ───────────────────────────────────────────────────────────────────
// Reset runtime config to compile-time defaults before each test.
class TempToColorTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg_cold_temp = DEFAULT_COLD_TEMP_F;   // 20 °F
        cfg_hot_temp  = DEFAULT_HOT_TEMP_F;    // 90 °F
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

// Description: The cold endpoint temperature (20°F) maps to hue 160 (blue);
// the resulting color has a dominant blue channel.
TEST_F(TempToColorTest, ColdEndpointIsBlue) {
    RecordProperty("description",
        "The cold endpoint temperature (20F) maps to hue 160 (blue); "
        "the resulting color has a dominant blue channel.");
    CRGB c = tempToColor(20.0f);   // exactly at cold end -> hue 160
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 160);
    EXPECT_GT(c.b, c.r);
}

// Description: The hot endpoint temperature (90°F) maps to hue 0 (red);
// the resulting color has red=255 and no blue.
TEST_F(TempToColorTest, HotEndpointIsRed) {
    RecordProperty("description",
        "The hot endpoint temperature (90F) maps to hue 0 (red); "
        "the resulting color has red=255 and no blue.");
    CRGB c = tempToColor(90.0f);   // exactly at hot end -> hue 0
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 0);
    EXPECT_GT(c.r, c.b);
    EXPECT_EQ(c.r, 255);
}

// Description: The midpoint between cold and hot (55°F) maps to hue 80
// (green-cyan), where green is the dominant channel.
TEST_F(TempToColorTest, MidpointIsGreenish) {
    RecordProperty("description",
        "The midpoint between cold and hot (55F) maps to hue 80 (green-cyan), "
        "where green is the dominant channel.");
    // 55 F -> fraction 0.5 -> hue 80
    CRGB c = tempToColor(55.0f);
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 80);
    EXPECT_GT(c.g, c.r);
    EXPECT_GT(c.g, c.b);
}

// Description: Temperatures far below the cold endpoint clamp to hue 160 (blue)
// rather than wrapping or going negative.
TEST_F(TempToColorTest, BelowColdClampsToBlue) {
    RecordProperty("description",
        "Temperatures far below the cold endpoint clamp to hue 160 (blue) "
        "rather than wrapping or going negative.");
    CRGB cold  = tempToColor(20.0f);
    CRGB below = tempToColor(-40.0f);
    EXPECT_EQ(cold._hue, below._hue);
}

// Description: Temperatures far above the hot endpoint clamp to hue 0 (red)
// rather than wrapping around the hue wheel.
TEST_F(TempToColorTest, AboveHotClampsToRed) {
    RecordProperty("description",
        "Temperatures far above the hot endpoint clamp to hue 0 (red) "
        "rather than wrapping around the hue wheel.");
    CRGB hot   = tempToColor(90.0f);
    CRGB above = tempToColor(200.0f);
    EXPECT_EQ(hot._hue, above._hue);
}

// Description: As temperature rises from cold to hot, the hue decreases
// monotonically (blue→green→red), confirming no reversals in the mapping.
TEST_F(TempToColorTest, HueDecreasesMonotonicallyWithTemperature) {
    RecordProperty("description",
        "As temperature rises from cold to hot, the hue decreases monotonically "
        "(blue to green to red), confirming no reversals in the mapping.");
    CRGB c30 = tempToColor(30.0f);
    CRGB c50 = tempToColor(50.0f);
    CRGB c70 = tempToColor(70.0f);
    CRGB c90 = tempToColor(90.0f);
    EXPECT_GE(c30._hue, c50._hue);
    EXPECT_GE(c50._hue, c70._hue);
    EXPECT_GE(c70._hue, c90._hue);
}

// Description: Changing cfg_cold_temp/cfg_hot_temp at runtime shifts the color
// scale so that the new endpoints produce the expected hues.
TEST_F(TempToColorTest, CustomRangeRespected) {
    RecordProperty("description",
        "Changing cfg_cold_temp/cfg_hot_temp at runtime shifts the color scale "
        "so that the new endpoints produce the expected hues.");
    cfg_cold_temp = 0.0f;
    cfg_hot_temp  = 100.0f;
    EXPECT_EQ(tempToColor(0.0f)._hue,   160);
    EXPECT_EQ(tempToColor(100.0f)._hue,   0);
}

// Description: The hot endpoint (90°F) produces CHSV(0, 255, 255) which
// converts to pure red, so the red channel equals 255.
TEST_F(TempToColorTest, FullyBrightAtHotEnd) {
    RecordProperty("description",
        "The hot endpoint (90F) produces CHSV(0, 255, 255) which converts to "
        "pure red, so the red channel equals 255.");
    // CHSV(0, 255, 255) -> pure red -> r == 255
    EXPECT_EQ(tempToColor(90.0f).r, 255);
}
