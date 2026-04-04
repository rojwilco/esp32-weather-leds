// sketch_api.h — declarations for all sketch symbols that tests depend on.
//
// Test files include this header instead of re-including the .ino.
// The actual definitions come from sketch_lib (sketch_wrapper.cpp).
//
// Keeps all mock headers and sketch constants in one place so individual
// test files stay concise.

#pragma once

#include "mocks/Arduino.h"
#include "mocks/FastLED.h"
#include "mocks/WiFi.h"
#include "mocks/HTTPClient.h"
#include "mocks/WiFiClientSecure.h"
#include "mocks/Preferences.h"
#include "mocks/WebServer.h"
#include "mocks/Update.h"
#include "../version.h"

// ── Sketch compile-time constants ────────────────────────────────────────────
#define LED_PIN  23
#define MAX_LEDS  16   // Open-Meteo API maximum for forecast_days

#define DEFAULT_NUM_LEDS        6
#define DEFAULT_BRIGHTNESS      50
#define DEFAULT_POLL_MIN        30
#define DEFAULT_COLD_TEMP_F     20.0f
#define DEFAULT_HOT_TEMP_F      90.0f
#define DEFAULT_LATITUDE        "38.9947"
#define DEFAULT_LONGITUDE       "-77.0284"
#define DEFAULT_FREEZE_THR_F    32.0f
#define DEFAULT_HEAT_THR_F      95.0f
#define DEFAULT_PRECIP_THR_PCT  50.0f
#define DEFAULT_WIFI_SSID       ""
#define DEFAULT_WIFI_PASS       ""
#define DEFAULT_HOLD_SEC        3.0f
#define DEFAULT_ALERT_HOLD_SEC  0.0f
#define DEFAULT_ATTACK_SEC      0.5f
#define DEFAULT_DECAY_SEC       0.5f
#define DEFAULT_FREEZE_COLOR    0xC8C8FFU
#define DEFAULT_HEAT_COLOR      0xFF8C00U
#define DEFAULT_RAIN_COLOR      0x00C8C8U

#define FADE_STEP  1

// ── Sketch types ─────────────────────────────────────────────────────────────
struct DayForecast {
    float tempMax;
    float tempMin;
    float tempAvg;
    float precipProb;
};

enum AlertType { ALERT_NONE, ALERT_HEAT, ALERT_FREEZE, ALERT_RAIN };

struct LEDState {
    CRGB      baseColor;
    CRGB      alertColor;
    AlertType alert;
    int       blendAmt;
    int       fadeDir;
    unsigned long lastTick;
    unsigned long holdUntil;
    unsigned long alertHoldUntil;
};

// ── Sketch globals (defined in sketch_wrapper.cpp via sketch_lib) ─────────────
extern uint8_t  cfg_num_leds;
extern uint8_t  cfg_brightness;
extern uint16_t cfg_poll_min;
extern float    cfg_cold_temp;
extern float    cfg_hot_temp;
extern char     cfg_latitude[16];
extern char     cfg_longitude[16];
extern float    cfg_freeze_thr;
extern float    cfg_heat_thr;
extern float    cfg_precip_thr;
extern char     cfg_wifi_ssid[64];
extern char     cfg_wifi_pass[64];
extern float    cfg_hold_sec;
extern float    cfg_alert_hold_sec;
extern float    cfg_attack_sec;
extern float    cfg_decay_sec;
extern uint32_t cfg_freeze_color;
extern uint32_t cfg_heat_color;
extern uint32_t cfg_rain_color;
extern CRGB     leds[MAX_LEDS];
extern LEDState ledStates[MAX_LEDS];
extern bool     g_forceRepoll;
extern bool     g_ap_mode;
extern bool     g_pendingConnect;

// ── Sketch functions (defined in sketch_wrapper.cpp via sketch_lib) ───────────
CRGB tempToColor(float tempF);
bool fetchForecast(DayForecast* outDays);
bool pollWeather();
void tickAnimations();
void applyHostname();
void startAPMode();
void handleRoot();
void handleSave();
void handlePollNow();
void handleScan();
void handleOtaUpdate();
void handleOtaUpload();
