#include <gtest/gtest.h>
#include "sketch_api.h"

class HostnameTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_wifi_hostname.clear();
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
