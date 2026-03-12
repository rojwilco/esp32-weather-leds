#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleRootTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg_num_leds   = DEFAULT_NUM_LEDS;
        cfg_brightness = DEFAULT_BRIGHTNESS;
        cfg_poll_min   = DEFAULT_POLL_MIN;
        cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
        cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
        strncpy(cfg_latitude,  DEFAULT_LATITUDE,  sizeof(cfg_latitude));
        strncpy(cfg_longitude, DEFAULT_LONGITUDE, sizeof(cfg_longitude));
        cfg_freeze_thr  = DEFAULT_FREEZE_THR_F;
        cfg_heat_thr    = DEFAULT_HEAT_THR_F;
        cfg_precip_thr  = DEFAULT_PRECIP_THR_PCT;
        cfg_hold_sec        = DEFAULT_HOLD_SEC;
        cfg_alert_hold_sec  = DEFAULT_ALERT_HOLD_SEC;
        cfg_attack_sec      = DEFAULT_ATTACK_SEC;
        cfg_decay_sec       = DEFAULT_DECAY_SEC;
        strncpy(cfg_wifi_ssid, DEFAULT_WIFI_SSID, sizeof(cfg_wifi_ssid));
        strncpy(cfg_wifi_pass, DEFAULT_WIFI_PASS, sizeof(cfg_wifi_pass));
        g_ap_mode = false;
        g_mock_last_send_code = 0;
        g_mock_last_send_body = "";
    }
};

// Description: handleRoot() responds with HTTP 200, confirming the handler
// is registered and reachable.
TEST_F(HandleRootTest, Returns200WithHtml) {
    RecordProperty("description",
        "handleRoot() responds with HTTP 200, confirming the handler "
        "is registered and reachable.");
    handleRoot();
    EXPECT_EQ(g_mock_last_send_code, 200);
}

// Description: The rendered HTML page contains a closing </html> tag, confirming
// the static page buffer is large enough to hold the full output without truncation.
TEST_F(HandleRootTest, BodyNotTruncated) {
    RecordProperty("description",
        "The rendered HTML page contains a closing </html> tag, confirming "
        "the static page buffer is large enough to hold the full output without truncation.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.rfind("</html>"), std::string::npos) << "page buffer too small — body was truncated";
}

// Description: The config page includes a country dropdown with the id
// 'countrySelect' and 'US' selected by default.
TEST_F(HandleRootTest, ContainsCountrySelect) {
    RecordProperty("description",
        "The config page includes a country dropdown with the id "
        "'countrySelect' and 'US' selected by default.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("countrySelect"), std::string::npos);
    EXPECT_NE(body.find("value=\"US\" selected"), std::string::npos);
}

// Description: The current cfg_latitude and cfg_longitude values are injected
// into the rendered HTML so the form shows the device's configured location.
TEST_F(HandleRootTest, InjectsConfigValues) {
    RecordProperty("description",
        "The current cfg_latitude and cfg_longitude values are injected into "
        "the rendered HTML so the form shows the device's configured location.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find(DEFAULT_LATITUDE),  std::string::npos);
    EXPECT_NE(body.find(DEFAULT_LONGITUDE), std::string::npos);
}

// Description: When the device is in station mode (g_ap_mode=false), the AP
// setup banner is hidden via 'display:none' so it doesn't clutter normal use.
TEST_F(HandleRootTest, APBannerHiddenInStaMode) {
    RecordProperty("description",
        "When the device is in station mode (g_ap_mode=false), the AP setup "
        "banner is hidden via 'display:none' so it doesn't clutter normal use.");
    g_ap_mode = false;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("display:none"), std::string::npos);
}

// Description: When the device is in AP mode (g_ap_mode=true), the AP setup
// banner is shown via 'display:block' to guide the user through WiFi configuration.
TEST_F(HandleRootTest, APBannerVisibleInAPMode) {
    RecordProperty("description",
        "When the device is in AP mode (g_ap_mode=true), the AP setup banner "
        "is shown via 'display:block' to guide the user through WiFi configuration.");
    g_ap_mode = true;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("display:block"), std::string::npos);
}

// Description: The current cfg_wifi_ssid value is injected into the rendered
// HTML so the form pre-fills the SSID field with the device's saved network name.
TEST_F(HandleRootTest, InjectsWifiSsid) {
    RecordProperty("description",
        "The current cfg_wifi_ssid value is injected into the rendered HTML so "
        "the form pre-fills the SSID field with the device's saved network name.");
    strncpy(cfg_wifi_ssid, "MyNetwork", sizeof(cfg_wifi_ssid));
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("MyNetwork"), std::string::npos);
}

// Description: The firmware version string is injected into the rendered HTML
// so the user can confirm which build is running without connecting to serial.
TEST_F(HandleRootTest, InjectsFirmwareVersion) {
    RecordProperty("description",
        "The firmware version string is injected into the rendered HTML "
        "so the user can confirm which build is running without connecting to serial.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find(FIRMWARE_VERSION), std::string::npos);
}

// Description: The rendered page includes a favicon link tag so browsers
// display a weather icon in tabs and bookmarks without any external fetch.
TEST_F(HandleRootTest, ContainsFaviconEmoji) {
    RecordProperty("description",
        "The rendered page includes a favicon link tag so browsers display a "
        "weather icon in tabs and bookmarks without any external fetch.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("<link rel=\"icon\""), std::string::npos);
}

// Description: The build timestamp label appears in the rendered HTML alongside
// the version string so the user can distinguish rebuilds of the same version number.
// The exact timestamp value differs per translation unit so we check for the label.
TEST_F(HandleRootTest, InjectsBuildTimestamp) {
    RecordProperty("description",
        "The build timestamp label appears in the rendered HTML alongside the version "
        "string so the user can distinguish rebuilds of the same version number.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("built "), std::string::npos);
}

// Description: In AP mode the main config section (brightness, temps, location)
// is hidden so the user only sees the WiFi setup fields they need.
TEST_F(HandleRootTest, APModeHidesMainConfig) {
    RecordProperty("description",
        "In AP mode the main config section (brightness, temps, location) "
        "is hidden so the user only sees the WiFi setup fields they need.");
    g_ap_mode = true;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    // The main-cfg div must be present and set to display:none
    size_t divPos = body.find("id=\"main-cfg\"");
    ASSERT_NE(divPos, std::string::npos);
    size_t stylePos = body.rfind("display:", divPos);
    ASSERT_NE(stylePos, std::string::npos);
    EXPECT_NE(body.find("display:none", stylePos), std::string::npos)
        << "main-cfg should be hidden in AP mode";
}

// Description: In station mode the main config section is visible so the user
// can adjust brightness, temperature thresholds, and location.
TEST_F(HandleRootTest, StationModeShowsMainConfig) {
    RecordProperty("description",
        "In station mode the main config section is visible so the user "
        "can adjust brightness, temperature thresholds, and location.");
    g_ap_mode = false;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    size_t divPos = body.find("id=\"main-cfg\"");
    ASSERT_NE(divPos, std::string::npos);
    size_t stylePos = body.rfind("display:", divPos);
    ASSERT_NE(stylePos, std::string::npos);
    EXPECT_NE(body.find("display:block", stylePos), std::string::npos)
        << "main-cfg should be visible in station mode";
}

// Description: In station mode the SSID selector appears after the main config
// section, keeping WiFi settings at the bottom for already-configured devices.
TEST_F(HandleRootTest, StationModeSSIDAfterMainConfig) {
    RecordProperty("description",
        "In station mode the SSID selector appears after the main config section, "
        "keeping WiFi settings at the bottom for already-configured devices.");
    g_ap_mode = false;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    size_t mainCfgPos = body.find("id=\"main-cfg\"");
    size_t ssidPos    = body.find("id=\"ssidSelect\"");
    ASSERT_NE(mainCfgPos, std::string::npos);
    ASSERT_NE(ssidPos,    std::string::npos);
    EXPECT_GT(ssidPos, mainCfgPos)
        << "ssidSelect should appear after the main-cfg section";
}

// Description: In AP mode the Poll Now and Firmware Update sections are hidden
// because those actions require an internet connection that isn't yet available.
TEST_F(HandleRootTest, APModeHidesStationOnlySection) {
    RecordProperty("description",
        "In AP mode the Poll Now and Firmware Update sections are hidden "
        "because those actions require an internet connection that isn't yet available.");
    g_ap_mode = true;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    size_t divPos = body.find("id=\"station-only\"");
    ASSERT_NE(divPos, std::string::npos);
    size_t stylePos = body.rfind("display:", divPos);
    ASSERT_NE(stylePos, std::string::npos);
    EXPECT_NE(body.find("display:none", stylePos), std::string::npos)
        << "station-only section should be hidden in AP mode";
}

// Description: handleRoot() injects cfg_num_leds into the num_leds input
// field value so the form shows the current configured LED count.
TEST_F(HandleRootTest, NumLedsInjectedIntoPage) {
    RecordProperty("description",
        "handleRoot() injects cfg_num_leds into the num_leds input field value "
        "so the form shows the current configured LED count.");
    cfg_num_leds = 9;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("name=\"num_leds\""), std::string::npos) << "num_leds field missing";
    EXPECT_NE(body.find("value=\"9\""),       std::string::npos) << "cfg_num_leds not injected";
}

// Description: handleRoot() renders a resetNerdyDefaults() JS function whose
// body contains the DEFAULT_* constant values injected via snprintf, so the
// firmware is the single source of truth for animation defaults.
TEST_F(HandleRootTest, NerdyDefaultsButtonPresent) {
    RecordProperty("description",
        "handleRoot() renders a resetNerdyDefaults() JS function whose body "
        "contains the DEFAULT_* constant values injected via snprintf, so the "
        "firmware is the single source of truth for animation defaults.");
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("resetNerdyDefaults"),      std::string::npos) << "resetNerdyDefaults missing";
    EXPECT_NE(body.find("Revert to defaults"),      std::string::npos) << "button label missing";
    // DEFAULT_HOLD_SEC=3.0, DEFAULT_ATTACK_SEC=0.5, DEFAULT_DECAY_SEC=0.5 should appear in the JS.
    EXPECT_NE(body.find("value='3.00'"),            std::string::npos) << "default hold_sec not injected";
    EXPECT_NE(body.find("attack_sec"),              std::string::npos) << "attack_sec field missing";
    EXPECT_NE(body.find("decay_sec"),               std::string::npos) << "decay_sec field missing";
}
