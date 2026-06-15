// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

// Uses empty cfg_wifi_ssid so setup() takes the fast AP-mode path, avoiding the
// WiFi-connect loop. NVS is backed by g_mock_prefs_store (in-memory map).
class FactoryResetTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_prefs_store.clear();
        g_ap_mode        = false;
        g_pendingConnect = false;
        g_demo_mode      = false;
        g_mock_millis    = 0;
        g_mock_wifi_status = WL_DISCONNECTED;
        // Empty SSID → setup() enters AP mode without attempting WiFi connect
        memset(cfg_wifi_ssid, 0, sizeof(cfg_wifi_ssid));
    }
};

// Description: A boot with no prior rst_count in NVS (first RST press) increments
// the counter to 1 without triggering a factory reset.
TEST_F(FactoryResetTest, FirstPressIncrementsCount) {
    RecordProperty("description",
        "A boot with no prior rst_count in NVS (first RST press) increments "
        "the counter to 1 without triggering a factory reset.");
    setup();
    Preferences p;
    p.begin("wxleds", false);
    EXPECT_EQ(p.getUChar("rst_count", 0), 1) << "rst_count should be 1 after first press";
    p.end();
    EXPECT_FALSE(g_mock_prefs_store.empty()) << "prefs should not have been cleared";
}

// Description: A boot with rst_count=1 (second RST press) increments the counter
// to 2 without triggering a factory reset.
TEST_F(FactoryResetTest, SecondPressIncrementsCount) {
    RecordProperty("description",
        "A boot with rst_count=1 (second RST press) increments the counter to 2 "
        "without triggering a factory reset.");
    g_mock_prefs_store["rst_count"] = "1";
    setup();
    Preferences p;
    p.begin("wxleds", false);
    EXPECT_EQ(p.getUChar("rst_count", 0), 2) << "rst_count should be 2 after second press";
    p.end();
    EXPECT_FALSE(g_mock_prefs_store.empty()) << "prefs should not have been cleared";
}

// Description: A boot with rst_count=2 (third RST press) calls prefs.clear(),
// wiping all NVS keys — WiFi credentials, location, and config — to force a
// fresh-start AP-mode reconfigure. This verifies the factory reset trigger.
TEST_F(FactoryResetTest, ThirdPressClearsAllNVS) {
    RecordProperty("description",
        "A boot with rst_count=2 (third RST press) calls prefs.clear(), wiping "
        "all NVS keys — WiFi credentials, location, and config — to force a "
        "fresh-start AP-mode reconfigure. This verifies the factory reset trigger.");
    g_mock_prefs_store["rst_count"] = "2";
    g_mock_prefs_store["wifi_ssid"] = "MyNetwork";  // sentinel: confirms clear wipes more than rst_count
    setup();
    EXPECT_TRUE(g_mock_prefs_store.empty())
        << "prefs.clear() should wipe all NVS keys on the third RST press";
}

// Description: A stale rst_count=4 (e.g. from an interrupted previous session)
// also triggers the factory reset, confirming the >=3 threshold handles any
// accumulated value, not just exactly 3.
TEST_F(FactoryResetTest, StaleHighCountAlsoClearsNVS) {
    RecordProperty("description",
        "A stale rst_count=4 (e.g. from an interrupted previous session) "
        "also triggers the factory reset, confirming the >=3 threshold handles "
        "any accumulated value, not just exactly 3.");
    g_mock_prefs_store["rst_count"] = "4";
    setup();
    EXPECT_TRUE(g_mock_prefs_store.empty())
        << "any rst_count >= 3 should trigger factory reset";
}

// Description: loop() clears rst_count to 0 in NVS on its very first call,
// ensuring that once the device reaches normal operation the triple-reset
// window closes and a fresh sequence must start from 0.
// NOTE: This test consumes the loop()-internal s_rstCountCleared static flag.
// It must be the only test in this binary that calls loop().
TEST_F(FactoryResetTest, LoopClearsCounterOnFirstCall) {
    RecordProperty("description",
        "loop() clears rst_count to 0 in NVS on its very first call, "
        "ensuring that once the device reaches normal operation the triple-reset "
        "window closes and a fresh sequence must start from 0.");
    g_mock_prefs_store["rst_count"] = "1";
    g_ap_mode = true;   // stay in AP-mode branch to avoid WiFi reconnect path
    loop();
    Preferences p;
    p.begin("wxleds", false);
    EXPECT_EQ(p.getUChar("rst_count", 0), 0)
        << "loop() should zero rst_count in NVS on its first call";
    p.end();
}
