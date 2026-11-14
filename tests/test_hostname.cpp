// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#include <gtest/gtest.h>
#include "sketch_api.h"

class HostnameTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_wifi_hostname.clear();
        cfg_friendly_name[0] = '\0';
    }
};

// Description: applyHostname() registers a hostname that starts with
// "weather-leds-" to distinguish the device from generic ESP32 hostnames.
TEST_F(HostnameTest, SetsWeatherLedsPrefix) {
    RecordProperty("description",
        "applyHostname() registers a hostname that starts with "
        "\"weather-leds-\" to distinguish the device from generic ESP32 hostnames.");
    applyHostname();
    EXPECT_EQ(g_mock_wifi_hostname.substr(0, 13), "weather-leds-");
}

// Description: The hostname suffix is the last 3 MAC address bytes in lowercase
// hex, making each device uniquely identifiable on the network.
TEST_F(HostnameTest, UsesMacSuffix) {
    RecordProperty("description",
        "The hostname suffix is the last 3 MAC address bytes in lowercase "
        "hex, making each device uniquely identifiable on the network.");
    applyHostname();
    // Mock MAC is "AA:BB:CC:11:22:33"; last 3 bytes → "112233"
    EXPECT_EQ(g_mock_wifi_hostname, "weather-leds-112233");
}

static std::string sanitize(const char* name) {
    char out[FRIENDLY_NAME_MAX + 1];
    buildHostname(name, out, sizeof(out));
    return out;
}

// Description: A friendly name with spaces and capitals becomes a lowercase,
// dash-separated hostname so the unit is recognisable in the router's client list.
TEST_F(HostnameTest, FriendlyNameBecomesHostname) {
    RecordProperty("description",
        "A friendly name with spaces and capitals becomes a lowercase, "
        "dash-separated hostname so the unit is recognisable in the router's client list.");
    strncpy(cfg_friendly_name, "Kitchen Window", sizeof(cfg_friendly_name));
    applyHostname();
    EXPECT_EQ(g_mock_wifi_hostname, "kitchen-window");
}

// Description: Punctuation and repeated separators collapse to single dashes and
// leading/trailing separators are stripped, since hostname labels may not begin or
// end with '-'.
TEST_F(HostnameTest, SanitizesPunctuationAndEdges) {
    RecordProperty("description",
        "Punctuation and repeated separators collapse to single dashes and "
        "leading/trailing separators are stripped, since hostname labels may not "
        "begin or end with '-'.");
    EXPECT_EQ(sanitize("  Bob's  LEDs!! "), "bob-s-leds");
    EXPECT_EQ(sanitize("--Unit_2--"), "unit-2");
    EXPECT_EQ(sanitize("Café Ø 3"), "caf-3");
}

// Description: A name with no ASCII letters or digits sanitizes to an empty string,
// so applyHostname() falls back to the MAC-based default instead of an invalid name.
TEST_F(HostnameTest, AllInvalidNameFallsBackToMac) {
    RecordProperty("description",
        "A name with no ASCII letters or digits sanitizes to an empty string, "
        "so applyHostname() falls back to the MAC-based default instead of an invalid name.");
    EXPECT_EQ(sanitize("!!! ???"), "");
    strncpy(cfg_friendly_name, "☀️🌧️", sizeof(cfg_friendly_name));
    applyHostname();
    EXPECT_EQ(g_mock_wifi_hostname, "weather-leds-112233");
}

// Description: An empty friendly name keeps the original weather-leds-XXXXXX
// hostname, preserving existing behaviour for units that never set a name.
TEST_F(HostnameTest, EmptyNameUsesMacDefault) {
    RecordProperty("description",
        "An empty friendly name keeps the original weather-leds-XXXXXX "
        "hostname, preserving existing behaviour for units that never set a name.");
    applyHostname();
    EXPECT_EQ(g_mock_wifi_hostname, "weather-leds-112233");
}

// Description: Hostnames are capped at FRIENDLY_NAME_MAX characters and never end
// in '-' after truncation, keeping them within the ESP32 netif limit and valid.
TEST_F(HostnameTest, LongNameTruncatedWithoutTrailingDash) {
    RecordProperty("description",
        "Hostnames are capped at FRIENDLY_NAME_MAX characters and never end "
        "in '-' after truncation, keeping them within the ESP32 netif limit and valid.");
    // 31 letters then a separator then more letters: the dash would land at
    // position 32, so it must be dropped rather than left trailing.
    std::string h = sanitize("abcdefghijklmnopqrstuvwxyzabcde fghij");
    EXPECT_EQ(h, "abcdefghijklmnopqrstuvwxyzabcde");
    std::string h2 = sanitize("abcdefghijklmnopqrstuvwxyz abcdefghijklmnop");
    EXPECT_LE(h2.size(), (size_t)FRIENDLY_NAME_MAX);
    EXPECT_NE(h2.back(), '-');
    EXPECT_EQ(h2, "abcdefghijklmnopqrstuvwxyz-abcde");
}
