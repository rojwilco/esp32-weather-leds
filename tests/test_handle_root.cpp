#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleRootTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg_brightness = DEFAULT_BRIGHTNESS;
        cfg_poll_min   = DEFAULT_POLL_MIN;
        cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
        cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
        strncpy(cfg_latitude,  DEFAULT_LATITUDE,  sizeof(cfg_latitude));
        strncpy(cfg_longitude, DEFAULT_LONGITUDE, sizeof(cfg_longitude));
        cfg_freeze_thr  = DEFAULT_FREEZE_THR_F;
        cfg_heat_thr    = DEFAULT_HEAT_THR_F;
        cfg_precip_thr  = DEFAULT_PRECIP_THR_PCT;
        strncpy(cfg_wifi_ssid, DEFAULT_WIFI_SSID, sizeof(cfg_wifi_ssid));
        strncpy(cfg_wifi_pass, DEFAULT_WIFI_PASS, sizeof(cfg_wifi_pass));
        g_ap_mode = false;
        g_mock_last_send_code = 0;
        g_mock_last_send_body = "";
    }
};

TEST_F(HandleRootTest, Returns200WithHtml) {
    handleRoot();
    EXPECT_EQ(g_mock_last_send_code, 200);
}

TEST_F(HandleRootTest, BodyNotTruncated) {
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.rfind("</html>"), std::string::npos) << "page buffer too small — body was truncated";
}

TEST_F(HandleRootTest, ContainsCountrySelect) {
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("countrySelect"), std::string::npos);
    EXPECT_NE(body.find("value=\"US\" selected"), std::string::npos);
}

TEST_F(HandleRootTest, InjectsConfigValues) {
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find(DEFAULT_LATITUDE),  std::string::npos);
    EXPECT_NE(body.find(DEFAULT_LONGITUDE), std::string::npos);
}

TEST_F(HandleRootTest, APBannerHiddenInStaMode) {
    g_ap_mode = false;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("display:none"), std::string::npos);
}

TEST_F(HandleRootTest, APBannerVisibleInAPMode) {
    g_ap_mode = true;
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("display:block"), std::string::npos);
}

TEST_F(HandleRootTest, InjectsWifiSsid) {
    strncpy(cfg_wifi_ssid, "MyNetwork", sizeof(cfg_wifi_ssid));
    handleRoot();
    std::string body = g_mock_last_send_body.c_str();
    EXPECT_NE(body.find("MyNetwork"), std::string::npos);
}
