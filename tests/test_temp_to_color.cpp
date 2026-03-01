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

TEST_F(TempToColorTest, ColdEndpointIsBlue) {
    CRGB c = tempToColor(20.0f);   // exactly at cold end -> hue 160
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 160);
    EXPECT_GT(c.b, c.r);
}

TEST_F(TempToColorTest, HotEndpointIsRed) {
    CRGB c = tempToColor(90.0f);   // exactly at hot end -> hue 0
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 0);
    EXPECT_GT(c.r, c.b);
    EXPECT_EQ(c.r, 255);
}

TEST_F(TempToColorTest, MidpointIsGreenish) {
    // 55 F -> fraction 0.5 -> hue 80
    CRGB c = tempToColor(55.0f);
    EXPECT_TRUE(c._fromHSV);
    EXPECT_EQ(c._hue, 80);
    EXPECT_GT(c.g, c.r);
    EXPECT_GT(c.g, c.b);
}

TEST_F(TempToColorTest, BelowColdClampsToBlue) {
    CRGB cold  = tempToColor(20.0f);
    CRGB below = tempToColor(-40.0f);
    EXPECT_EQ(cold._hue, below._hue);
}

TEST_F(TempToColorTest, AboveHotClampsToRed) {
    CRGB hot   = tempToColor(90.0f);
    CRGB above = tempToColor(200.0f);
    EXPECT_EQ(hot._hue, above._hue);
}

TEST_F(TempToColorTest, HueDecreasesMonotonicallyWithTemperature) {
    CRGB c30 = tempToColor(30.0f);
    CRGB c50 = tempToColor(50.0f);
    CRGB c70 = tempToColor(70.0f);
    CRGB c90 = tempToColor(90.0f);
    EXPECT_GE(c30._hue, c50._hue);
    EXPECT_GE(c50._hue, c70._hue);
    EXPECT_GE(c70._hue, c90._hue);
}

TEST_F(TempToColorTest, CustomRangeRespected) {
    cfg_cold_temp = 0.0f;
    cfg_hot_temp  = 100.0f;
    EXPECT_EQ(tempToColor(0.0f)._hue,   160);
    EXPECT_EQ(tempToColor(100.0f)._hue,   0);
}

TEST_F(TempToColorTest, FullyBrightAtHotEnd) {
    // CHSV(0, 255, 255) -> pure red -> r == 255
    EXPECT_EQ(tempToColor(90.0f).r, 255);
}
