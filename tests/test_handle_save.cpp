#include "sketch_api.h"
#include <gtest/gtest.h>
#include <string>

class HandleSaveTest : public ::testing::Test {
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
        g_forceRepoll    = false;
        g_pendingConnect = false;
        g_mock_server_args.clear();
        g_mock_prefs_store.clear();
        g_mock_last_send_code = 0;
        g_mock_last_redirect  = "";
    }
};

// Description: Submitting a new latitude value sets g_forceRepoll=true so the
// next loop() iteration re-fetches weather for the new location.
TEST_F(HandleSaveTest, NewLatSetsForceRepoll) {
    RecordProperty("description",
        "Submitting a new latitude value sets g_forceRepoll=true so the "
        "next loop() iteration re-fetches weather for the new location.");
    g_mock_server_args["latitude"]  = "40.7128";
    g_mock_server_args["longitude"] = DEFAULT_LONGITUDE;
    handleSave();
    EXPECT_TRUE(g_forceRepoll);
}

// Description: Submitting a new longitude value sets g_forceRepoll=true so the
// next loop() iteration re-fetches weather for the new location.
TEST_F(HandleSaveTest, NewLonSetsForceRepoll) {
    RecordProperty("description",
        "Submitting a new longitude value sets g_forceRepoll=true so the "
        "next loop() iteration re-fetches weather for the new location.");
    g_mock_server_args["latitude"]  = DEFAULT_LATITUDE;
    g_mock_server_args["longitude"] = "-74.0060";
    handleSave();
    EXPECT_TRUE(g_forceRepoll);
}

// Description: Submitting the same latitude and longitude as currently
// configured does not set g_forceRepoll, avoiding a redundant weather fetch.
TEST_F(HandleSaveTest, UnchangedLocationNoRepoll) {
    RecordProperty("description",
        "Submitting the same latitude and longitude as currently configured "
        "does not set g_forceRepoll, avoiding a redundant weather fetch.");
    g_mock_server_args["latitude"]  = DEFAULT_LATITUDE;
    g_mock_server_args["longitude"] = DEFAULT_LONGITUDE;
    handleSave();
    EXPECT_FALSE(g_forceRepoll);
}

// Description: A brightness value above the maximum (300) is clamped to 255
// before being applied and saved, preventing out-of-range FastLED values.
TEST_F(HandleSaveTest, BrightnessClampedAt255) {
    RecordProperty("description",
        "A brightness value above the maximum (300) is clamped to 255 before "
        "being applied and saved, preventing out-of-range FastLED values.");
    g_mock_server_args["brightness"] = "300";
    handleSave();
    EXPECT_EQ(cfg_brightness, 255);
}

// Description: A negative brightness value (-5) is clamped to 0 before being
// applied and saved, preventing underflow.
TEST_F(HandleSaveTest, BrightnessClampedAt0) {
    RecordProperty("description",
        "A negative brightness value (-5) is clamped to 0 before being applied "
        "and saved, preventing underflow.");
    g_mock_server_args["brightness"] = "-5";
    handleSave();
    EXPECT_EQ(cfg_brightness, 0);
}

// Description: A poll interval above the maximum (9999 minutes) is clamped to
// 1440 (24 hours) before being saved, preventing unreasonably long poll gaps.
TEST_F(HandleSaveTest, PollMinClampedTo1440) {
    RecordProperty("description",
        "A poll interval above the maximum (9999 minutes) is clamped to 1440 "
        "(24 hours) before being saved, preventing unreasonably long poll gaps.");
    g_mock_server_args["poll_min"] = "9999";
    handleSave();
    EXPECT_EQ(cfg_poll_min, 1440);
}

// Description: If cold_temp is submitted higher than hot_temp (an invalid
// range), hot_temp is auto-corrected to cold_temp+1 to maintain a valid range.
TEST_F(HandleSaveTest, ColdHotTempCorrected) {
    RecordProperty("description",
        "If cold_temp is submitted higher than hot_temp (an invalid range), "
        "hot_temp is auto-corrected to cold_temp+1 to maintain a valid range.");
    g_mock_server_args["cold_temp"] = "50";
    g_mock_server_args["hot_temp"]  = "40";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_hot_temp, 51.0f);
}

// Description: After processing the form, handleSave() responds with a 303
// redirect to "/" to prevent duplicate form submission on page refresh.
TEST_F(HandleSaveTest, RedirectsToRoot) {
    RecordProperty("description",
        "After processing the form, handleSave() responds with a 303 redirect "
        "to \"/\" to prevent duplicate form submission on page refresh.");
    handleSave();
    EXPECT_EQ(g_mock_last_send_code, 303);
    EXPECT_EQ(std::string(g_mock_last_redirect.c_str()), "/");
}

// Description: A valid brightness value is written to NVS under the "brightness"
// key so the setting survives a device reboot.
TEST_F(HandleSaveTest, PersistsToBrightness) {
    RecordProperty("description",
        "A valid brightness value is written to NVS under the \"brightness\" key "
        "so the setting survives a device reboot.");
    g_mock_server_args["brightness"] = "100";
    handleSave();
    EXPECT_EQ(g_mock_prefs_store["brightness"], "100");
}

// Description: Submitting a new SSID sets g_pendingConnect=true and updates
// cfg_wifi_ssid, triggering a WiFi reconnect on the next loop() iteration.
TEST_F(HandleSaveTest, NewSsidSetsPendingConnect) {
    RecordProperty("description",
        "Submitting a new SSID sets g_pendingConnect=true and updates "
        "cfg_wifi_ssid, triggering a WiFi reconnect on the next loop() iteration.");
    g_mock_server_args["wifi_ssid"] = "MyNetwork";
    handleSave();
    EXPECT_TRUE(g_pendingConnect);
    EXPECT_EQ(std::string(cfg_wifi_ssid), "MyNetwork");
}

// Description: Submitting the same SSID that is already configured does not
// set g_pendingConnect, avoiding a needless WiFi reconnect.
TEST_F(HandleSaveTest, UnchangedSsidNoPendingConnect) {
    RecordProperty("description",
        "Submitting the same SSID that is already configured does not set "
        "g_pendingConnect, avoiding a needless WiFi reconnect.");
    // ssid is already "" (DEFAULT_WIFI_SSID); sending "" again = no change
    g_mock_server_args["wifi_ssid"] = "";
    handleSave();
    EXPECT_FALSE(g_pendingConnect);
}

// Description: Submitting a new password (with the same SSID) sets
// g_pendingConnect=true so the updated credentials are applied on reconnect.
TEST_F(HandleSaveTest, PasswordSetsPendingConnect) {
    RecordProperty("description",
        "Submitting a new password (with the same SSID) sets g_pendingConnect=true "
        "so the updated credentials are applied on reconnect.");
    g_mock_server_args["wifi_pass"] = "secret123";
    handleSave();
    EXPECT_TRUE(g_pendingConnect);
    EXPECT_EQ(std::string(cfg_wifi_pass), "secret123");
}

// Description: Submitting an empty password field leaves the stored password
// unchanged and does not set g_pendingConnect.
TEST_F(HandleSaveTest, EmptyPasswordIgnored) {
    RecordProperty("description",
        "Submitting an empty password field leaves the stored password unchanged "
        "and does not set g_pendingConnect.");
    strncpy(cfg_wifi_pass, "existing", sizeof(cfg_wifi_pass));
    g_mock_server_args["wifi_pass"] = "";
    handleSave();
    // empty password arg → no update
    EXPECT_FALSE(g_pendingConnect);
    EXPECT_EQ(std::string(cfg_wifi_pass), "existing");
}

// Description: WiFi SSID and password are both written to NVS so they survive
// a reboot and are restored by loadConfig() on next boot.
TEST_F(HandleSaveTest, WifiCredentialsPersistedToNvs) {
    RecordProperty("description",
        "WiFi SSID and password are both written to NVS so they survive a reboot "
        "and are restored by loadConfig() on next boot.");
    g_mock_server_args["wifi_ssid"] = "HomeWifi";
    g_mock_server_args["wifi_pass"] = "pass123";
    handleSave();
    EXPECT_EQ(g_mock_prefs_store["wifi_ssid"], "HomeWifi");
    EXPECT_EQ(g_mock_prefs_store["wifi_pass"],  "pass123");
}

// Description: Submitting a num_leds value within range (4) updates cfg_num_leds
// and sets g_forceRepoll so the API is immediately called with the new day count.
TEST_F(HandleSaveTest, NumLedsInRangeUpdatesAndRepolls) {
    RecordProperty("description",
        "Submitting a num_leds value within range (4) updates cfg_num_leds "
        "and sets g_forceRepoll so the API is immediately called with the new day count.");
    g_mock_server_args["num_leds"] = "4";
    handleSave();
    EXPECT_EQ(cfg_num_leds, 4);
    EXPECT_TRUE(g_forceRepoll);
}

// Description: A num_leds value above MAX_LEDS is clamped to MAX_LEDS (16)
// before being applied, preventing out-of-bounds LED array access.
TEST_F(HandleSaveTest, NumLedsClampedAtMax) {
    RecordProperty("description",
        "A num_leds value above MAX_LEDS is clamped to MAX_LEDS (16) before "
        "being applied, preventing out-of-bounds LED array access.");
    g_mock_server_args["num_leds"] = "99";
    handleSave();
    EXPECT_EQ(cfg_num_leds, MAX_LEDS);
}

// Description: Submitting the same num_leds as currently configured does not
// set g_forceRepoll, avoiding a redundant weather fetch.
TEST_F(HandleSaveTest, UnchangedNumLedsNoRepoll) {
    RecordProperty("description",
        "Submitting the same num_leds as currently configured does not set "
        "g_forceRepoll, avoiding a redundant weather fetch.");
    g_mock_server_args["num_leds"] = "6";   // same as DEFAULT_NUM_LEDS
    handleSave();
    EXPECT_FALSE(g_forceRepoll);
}

// Description: The num_leds value is written to NVS under the "num_leds" key
// so the configured LED count persists across device reboots.
TEST_F(HandleSaveTest, NumLedsPersistedToNvs) {
    RecordProperty("description",
        "The num_leds value is written to NVS under the \"num_leds\" key so "
        "the configured LED count persists across device reboots.");
    g_mock_server_args["num_leds"] = "8";
    handleSave();
    EXPECT_EQ(g_mock_prefs_store["num_leds"], "8");
}

// Description: A hold_sec value above the 60 s maximum is clamped to 60
// so the device never waits an unreasonable time between animation cycles.
TEST_F(HandleSaveTest, HoldSecClampedAtMax) {
    RecordProperty("description",
        "A hold_sec value above the 60 s maximum is clamped to 60 so the "
        "device never waits an unreasonable time between animation cycles.");
    g_mock_server_args["hold_sec"] = "999";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_hold_sec, 60.0f);
}

// Description: A hold_sec value below the 0.1 s minimum is clamped to 0.1
// so animation always has a measurable hold phase.
TEST_F(HandleSaveTest, HoldSecClampedAtMin) {
    RecordProperty("description",
        "A hold_sec value below the 0.1 s minimum is clamped to 0.1 so "
        "animation always has a measurable hold phase.");
    g_mock_server_args["hold_sec"] = "0.0";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_hold_sec, 0.1f);
}

// Description: A fractional hold_sec value like 1.5 is stored exactly,
// confirming that sub-second resolution is accepted.
TEST_F(HandleSaveTest, HoldSecAcceptsFloat) {
    RecordProperty("description",
        "A fractional hold_sec value like 1.5 is stored exactly, confirming "
        "that sub-second resolution is accepted.");
    g_mock_server_args["hold_sec"] = "1.5";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_hold_sec, 1.5f);
}

// Description: An attack_sec value above the 10 s maximum is clamped to 10
// so the attack duration stays within a reasonable range.
TEST_F(HandleSaveTest, AttackSecClampedAtMax) {
    RecordProperty("description",
        "An attack_sec value above the 10 s maximum is clamped to 10 so the "
        "attack duration stays within a reasonable range.");
    g_mock_server_args["attack_sec"] = "999";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_attack_sec, 10.0f);
}

// Description: An attack_sec value below the 0.1 s minimum is clamped to 0.1
// so the attack phase is always visibly perceptible.
TEST_F(HandleSaveTest, AttackSecClampedAtMin) {
    RecordProperty("description",
        "An attack_sec value below the 0.1 s minimum is clamped to 0.1 so "
        "the attack phase is always visibly perceptible.");
    g_mock_server_args["attack_sec"] = "0.0";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_attack_sec, 0.1f);
}

// Description: A fractional attack_sec value like 0.25 is stored exactly,
// confirming that quarter-second resolution is accepted.
TEST_F(HandleSaveTest, AttackSecAcceptsFloat) {
    RecordProperty("description",
        "A fractional attack_sec value like 0.25 is stored exactly, confirming "
        "that quarter-second resolution is accepted.");
    g_mock_server_args["attack_sec"] = "0.25";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_attack_sec, 0.25f);
}

// Description: A decay_sec value above the 10 s maximum is clamped to 10
// so the decay duration stays within a reasonable range.
TEST_F(HandleSaveTest, DecaySecClampedAtMax) {
    RecordProperty("description",
        "A decay_sec value above the 10 s maximum is clamped to 10 so the "
        "decay duration stays within a reasonable range.");
    g_mock_server_args["decay_sec"] = "999";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_decay_sec, 10.0f);
}

// Description: A decay_sec value below the 0.1 s minimum is clamped to 0.1
// so the decay phase is always visibly perceptible.
TEST_F(HandleSaveTest, DecaySecClampedAtMin) {
    RecordProperty("description",
        "A decay_sec value below the 0.1 s minimum is clamped to 0.1 so "
        "the decay phase is always visibly perceptible.");
    g_mock_server_args["decay_sec"] = "0.0";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_decay_sec, 0.1f);
}

// Description: A fractional decay_sec value like 1.5 is stored exactly,
// confirming that sub-second resolution is accepted for long decays.
TEST_F(HandleSaveTest, DecaySecAcceptsFloat) {
    RecordProperty("description",
        "A fractional decay_sec value like 1.5 is stored exactly, confirming "
        "that sub-second resolution is accepted for long decays.");
    g_mock_server_args["decay_sec"] = "1.5";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_decay_sec, 1.5f);
}

// Description: An alert_hold_sec value above the 10 s maximum is clamped to 10
// so the LED does not freeze indefinitely on the alert color.
TEST_F(HandleSaveTest, AlertHoldSecClampedAtMax) {
    RecordProperty("description",
        "An alert_hold_sec value above the 10 s maximum is clamped to 10 so "
        "the LED does not freeze indefinitely on the alert color.");
    g_mock_server_args["alert_hold_sec"] = "999";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_alert_hold_sec, 10.0f);
}

// Description: An alert_hold_sec of 0 is valid (no hold at peak) and is
// stored as-is, allowing the flash to immediately start fading back.
TEST_F(HandleSaveTest, AlertHoldSecZeroIsValid) {
    RecordProperty("description",
        "An alert_hold_sec of 0 is valid (no hold at peak) and is stored as-is, "
        "allowing the flash to immediately start fading back.");
    g_mock_server_args["alert_hold_sec"] = "0";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_alert_hold_sec, 0.0f);
}

// Description: A fractional alert_hold_sec like 0.75 is accepted exactly,
// confirming that sub-second resolution is supported.
TEST_F(HandleSaveTest, AlertHoldSecAcceptsFloat) {
    RecordProperty("description",
        "A fractional alert_hold_sec like 0.75 is accepted exactly, confirming "
        "that sub-second resolution is supported.");
    g_mock_server_args["alert_hold_sec"] = "0.75";
    handleSave();
    EXPECT_FLOAT_EQ(cfg_alert_hold_sec, 0.75f);
}

// Description: The hold_sec, attack_sec, and decay_sec values are written to
// NVS so they survive a device reboot and are restored by loadConfig().
TEST_F(HandleSaveTest, AnimTimingPersistedToNvs) {
    RecordProperty("description",
        "The hold_sec, attack_sec, and decay_sec values are written to NVS so "
        "they survive a device reboot and are restored by loadConfig().");
    g_mock_server_args["hold_sec"]       = "2.5";
    g_mock_server_args["alert_hold_sec"] = "0.3";
    g_mock_server_args["attack_sec"]     = "0.2";
    g_mock_server_args["decay_sec"]      = "1.5";
    handleSave();
    ASSERT_NE(g_mock_prefs_store.find("hold_sec"),       g_mock_prefs_store.end());
    ASSERT_NE(g_mock_prefs_store.find("alert_hold_sec"), g_mock_prefs_store.end());
    ASSERT_NE(g_mock_prefs_store.find("attack_sec"),     g_mock_prefs_store.end());
    ASSERT_NE(g_mock_prefs_store.find("decay_sec"),      g_mock_prefs_store.end());
    EXPECT_FLOAT_EQ(std::stof(g_mock_prefs_store["hold_sec"]),       2.5f);
    EXPECT_FLOAT_EQ(std::stof(g_mock_prefs_store["alert_hold_sec"]), 0.3f);
    EXPECT_FLOAT_EQ(std::stof(g_mock_prefs_store["attack_sec"]),     0.2f);
    EXPECT_FLOAT_EQ(std::stof(g_mock_prefs_store["decay_sec"]),      1.5f);
}
