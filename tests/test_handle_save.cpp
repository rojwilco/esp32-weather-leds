#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleSaveTest : public ::testing::Test {
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
        g_forceRepoll = false;
        g_mock_server_args.clear();
        g_mock_prefs_store.clear();
        g_mock_last_send_code = 0;
        g_mock_last_redirect  = "";
    }
};

TEST_F(HandleSaveTest, NewLatSetsForceRepoll) {
    g_mock_server_args["latitude"]  = "40.7128";
    g_mock_server_args["longitude"] = DEFAULT_LONGITUDE;
    handleSave();
    EXPECT_TRUE(g_forceRepoll);
}

TEST_F(HandleSaveTest, NewLonSetsForceRepoll) {
    g_mock_server_args["latitude"]  = DEFAULT_LATITUDE;
    g_mock_server_args["longitude"] = "-74.0060";
    handleSave();
    EXPECT_TRUE(g_forceRepoll);
}

TEST_F(HandleSaveTest, UnchangedLocationNoRepoll) {
    g_mock_server_args["latitude"]  = DEFAULT_LATITUDE;
    g_mock_server_args["longitude"] = DEFAULT_LONGITUDE;
    handleSave();
    EXPECT_FALSE(g_forceRepoll);
}

TEST_F(HandleSaveTest, BrightnessClampedAt255) {
    g_mock_server_args["brightness"] = "300";
    handleSave();
    EXPECT_EQ(cfg_brightness, 255);
}

TEST_F(HandleSaveTest, BrightnessClampedAt0) {
    g_mock_server_args["brightness"] = "-5";
    handleSave();
    EXPECT_EQ(cfg_brightness, 0);
}

TEST_F(HandleSaveTest, PollMinClampedTo1440) {
    g_mock_server_args["poll_min"] = "9999";
    handleSave();
    EXPECT_EQ(cfg_poll_min, 1440);
}

TEST_F(HandleSaveTest, ColdHotTempCorrected) {
    g_mock_server_args["cold_temp"] = "50";
    g_mock_server_args["hot_temp"]  = "40";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_hot_temp, 51.0f);
}

TEST_F(HandleSaveTest, RedirectsToRoot) {
    handleSave();
    EXPECT_EQ(g_mock_last_send_code, 303);
    EXPECT_EQ(std::string(g_mock_last_redirect.c_str()), "/");
}

TEST_F(HandleSaveTest, PersistsToBrightness) {
    g_mock_server_args["brightness"] = "100";
    handleSave();
    EXPECT_EQ(g_mock_prefs_store["brightness"], "100");
}
