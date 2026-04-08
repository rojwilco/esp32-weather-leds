// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#ifndef UNIT_TESTING
#include <DNSServer.h>
#include <Update.h>
#endif

#include <Preferences.h>
#include <WebServer.h>
#include "config_html.h"
#include "version.h"

#define LED_PIN           23
#define MAX_LEDS          16   // Open-Meteo API maximum for forecast_days
#define AP_SSID           "ESP32-WeatherLED"

// Compile-time defaults — overridden at runtime via web UI + NVS
#define DEFAULT_NUM_LEDS        6
#define DEFAULT_BRIGHTNESS      50
#define DEFAULT_POLL_MIN        30        // minutes
#define DEFAULT_COLD_TEMP_F     20.0f
#define DEFAULT_HOT_TEMP_F      90.0f
#define DEFAULT_LATITUDE        "38.9947"
#define DEFAULT_LONGITUDE       "-77.0284"
#define DEFAULT_FREEZE_THR_F    32.0f
#define DEFAULT_HEAT_THR_F      95.0f
#define DEFAULT_PRECIP_THR_PCT  50.0f
#define DEFAULT_WIFI_SSID       ""
#define DEFAULT_WIFI_PASS       ""
#define DEFAULT_HOLD_SEC        3.0f     // seconds to hold on temperature color between flashes
#define DEFAULT_ALERT_HOLD_SEC  0.0f     // seconds to hold at full alert color before fading back (0 = no hold)
#define DEFAULT_ATTACK_SEC      0.5f     // rise time: base → alert color
#define DEFAULT_DECAY_SEC       0.5f     // fall time: alert → base color
#define DEFAULT_FREEZE_COLOR    0xC8C8FFU  // icy white-blue, distinct from cold blue base
#define DEFAULT_HEAT_COLOR      0xFF8C00U  // orange, distinct from hot red base
#define DEFAULT_RAIN_COLOR      0x00C8C8U  // cyan

// Runtime config (loaded from NVS, used everywhere)
uint8_t  cfg_num_leds   = DEFAULT_NUM_LEDS;
uint8_t  cfg_brightness = DEFAULT_BRIGHTNESS;
uint16_t cfg_poll_min   = DEFAULT_POLL_MIN;
float    cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
float    cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
char     cfg_latitude[16]  = DEFAULT_LATITUDE;
char     cfg_longitude[16] = DEFAULT_LONGITUDE;
float    cfg_freeze_thr = DEFAULT_FREEZE_THR_F;
float    cfg_heat_thr   = DEFAULT_HEAT_THR_F;
float    cfg_precip_thr = DEFAULT_PRECIP_THR_PCT;
char     cfg_wifi_ssid[64] = DEFAULT_WIFI_SSID;
char     cfg_wifi_pass[64] = DEFAULT_WIFI_PASS;
float    cfg_hold_sec        = DEFAULT_HOLD_SEC;
float    cfg_alert_hold_sec  = DEFAULT_ALERT_HOLD_SEC;
float    cfg_attack_sec      = DEFAULT_ATTACK_SEC;
float    cfg_decay_sec       = DEFAULT_DECAY_SEC;
uint32_t cfg_freeze_color = DEFAULT_FREEZE_COLOR;
uint32_t cfg_heat_color   = DEFAULT_HEAT_COLOR;
uint32_t cfg_rain_color   = DEFAULT_RAIN_COLOR;

// Animation: single-unit blend steps for perceptual smoothness at all brightness levels.
// Tick interval is derived from cfg_attack_sec / cfg_decay_sec so phase durations stay user-controlled.
#define FADE_STEP  1

Preferences prefs;
WebServer   server(80);

bool g_forceRepoll    = false;
bool g_ap_mode        = false;
bool g_pendingConnect = false;

#ifndef UNIT_TESTING
static DNSServer dnsServer;
#endif

struct DayForecast {
  float tempMax;
  float tempMin;
  float tempAvg;
  float precipProb;  // 0–100
};

enum AlertType { ALERT_NONE, ALERT_HEAT, ALERT_FREEZE, ALERT_RAIN };

struct LEDState {
  CRGB      baseColor;
  CRGB      alertColor;
  AlertType alert;
  int       blendAmt;         // 0–255
  int       fadeDir;          // +1 or -1
  unsigned long lastTick;
  unsigned long holdUntil;      // millis() when hold-on-base phase ends
  unsigned long alertHoldUntil; // millis() when hold-on-alert phase ends
};

CRGB leds[MAX_LEDS];
LEDState ledStates[MAX_LEDS];

void loadConfig() {
  prefs.begin("wxleds", /*readOnly=*/true);
  cfg_num_leds   = prefs.getUChar("num_leds",    DEFAULT_NUM_LEDS);
  cfg_brightness = prefs.getUChar("brightness",  DEFAULT_BRIGHTNESS);
  cfg_poll_min   = prefs.getUShort("poll_min",   DEFAULT_POLL_MIN);
  cfg_cold_temp  = prefs.getFloat("cold_temp",   DEFAULT_COLD_TEMP_F);
  cfg_hot_temp   = prefs.getFloat("hot_temp",    DEFAULT_HOT_TEMP_F);
  cfg_freeze_thr = prefs.getFloat("freeze_thr",  DEFAULT_FREEZE_THR_F);
  cfg_heat_thr   = prefs.getFloat("heat_thr",    DEFAULT_HEAT_THR_F);
  cfg_precip_thr = prefs.getFloat("precip_thr",  DEFAULT_PRECIP_THR_PCT);
  prefs.getString("latitude",  DEFAULT_LATITUDE).toCharArray(cfg_latitude,  sizeof(cfg_latitude));
  prefs.getString("longitude", DEFAULT_LONGITUDE).toCharArray(cfg_longitude, sizeof(cfg_longitude));
  prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID).toCharArray(cfg_wifi_ssid, sizeof(cfg_wifi_ssid));
  prefs.getString("wifi_pass", DEFAULT_WIFI_PASS).toCharArray(cfg_wifi_pass, sizeof(cfg_wifi_pass));
  cfg_hold_sec        = prefs.getFloat("hold_sec",       DEFAULT_HOLD_SEC);
  cfg_alert_hold_sec  = prefs.getFloat("alert_hold_sec", DEFAULT_ALERT_HOLD_SEC);
  cfg_attack_sec      = prefs.getFloat("attack_sec",     DEFAULT_ATTACK_SEC);
  cfg_decay_sec       = prefs.getFloat("decay_sec",      DEFAULT_DECAY_SEC);
  cfg_freeze_color    = prefs.getUInt("freeze_clr",  DEFAULT_FREEZE_COLOR);
  cfg_heat_color      = prefs.getUInt("heat_clr",    DEFAULT_HEAT_COLOR);
  cfg_rain_color      = prefs.getUInt("rain_clr",    DEFAULT_RAIN_COLOR);
  prefs.end();
  Serial.printf("Config: leds=%d brt=%d poll=%dmin cold=%.1f hot=%.1f lat=%s lon=%s ssid=%s hold=%.2fs ahld=%.2fs atk=%.2fs dcy=%.2fs\n",
                cfg_num_leds, cfg_brightness, cfg_poll_min, cfg_cold_temp, cfg_hot_temp,
                cfg_latitude, cfg_longitude, cfg_wifi_ssid, cfg_hold_sec, cfg_alert_hold_sec, cfg_attack_sec, cfg_decay_sec);
}

void saveConfig() {
  prefs.begin("wxleds", /*readOnly=*/false);
  prefs.putUChar("num_leds",    cfg_num_leds);
  prefs.putUChar("brightness",  cfg_brightness);
  prefs.putUShort("poll_min",   cfg_poll_min);
  prefs.putFloat("cold_temp",   cfg_cold_temp);
  prefs.putFloat("hot_temp",    cfg_hot_temp);
  prefs.putFloat("freeze_thr",  cfg_freeze_thr);
  prefs.putFloat("heat_thr",    cfg_heat_thr);
  prefs.putFloat("precip_thr",  cfg_precip_thr);
  prefs.putString("latitude",   cfg_latitude);
  prefs.putString("longitude",  cfg_longitude);
  prefs.putString("wifi_ssid",  cfg_wifi_ssid);
  prefs.putString("wifi_pass",  cfg_wifi_pass);
  prefs.putFloat("hold_sec",       cfg_hold_sec);
  prefs.putFloat("alert_hold_sec", cfg_alert_hold_sec);
  prefs.putFloat("attack_sec",     cfg_attack_sec);
  prefs.putFloat("decay_sec",      cfg_decay_sec);
  prefs.putUInt("freeze_clr",  cfg_freeze_color);
  prefs.putUInt("heat_clr",    cfg_heat_color);
  prefs.putUInt("rain_clr",    cfg_rain_color);
  prefs.end();
}

CRGB tempToColor(float tempF) {
  float fraction = constrain((tempF - cfg_cold_temp) / (cfg_hot_temp - cfg_cold_temp), 0.0f, 1.0f);
  uint8_t hue = 160 - (uint8_t)(fraction * 160.0f);
  return CHSV(hue, 255, 255);
}

bool fetchForecast(DayForecast* outDays) {
  char url[300];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=%s&longitude=%s"
    "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&temperature_unit=fahrenheit"
    "&timezone=auto"
    "&forecast_days=%d",
    cfg_latitude, cfg_longitude, cfg_num_leds);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("fetchForecast: http.begin() failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("fetchForecast: HTTP error %d\n", httpCode);
    http.end();
    return false;
  }

  // Buffer full payload before http.end() — WiFiClientSecure streams can
  // deliver incomplete data to deserializeJson when read directly.
  String payload = http.getString();
  http.end();

  JsonDocument filter;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["daily"]["precipitation_probability_max"][0] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

  if (err) {
    Serial.printf("fetchForecast: JSON parse error: %s\n", err.c_str());
    return false;
  }

  JsonArray maxArr    = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray minArr    = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray precipArr = doc["daily"]["precipitation_probability_max"].as<JsonArray>();

  if (maxArr.isNull() || minArr.isNull()) {
    Serial.println("fetchForecast: missing temperature arrays in response");
    return false;
  }

  for (int i = 0; i < cfg_num_leds; i++) {
    outDays[i].tempMax    = maxArr[i].as<float>();
    outDays[i].tempMin    = minArr[i].as<float>();
    outDays[i].tempAvg    = (outDays[i].tempMax + outDays[i].tempMin) / 2.0f;
    outDays[i].precipProb = precipArr.isNull() ? 0.0f : precipArr[i].as<float>();
    Serial.printf("  day %d: max=%.1f  min=%.1f  avg=%.1f  precip=%.0f%%\n",
                  i, outDays[i].tempMax, outDays[i].tempMin,
                  outDays[i].tempAvg, outDays[i].precipProb);
  }

  return true;
}

bool pollWeather() {
  DayForecast forecast[MAX_LEDS];

  Serial.println("pollWeather: fetching forecast...");
  bool ok = fetchForecast(forecast);

  if (ok) {
    unsigned long now = millis();
    for (int i = 0; i < cfg_num_leds; i++) {
      CRGB base = tempToColor(forecast[i].tempAvg);

      AlertType alert;
      CRGB alertColor;
      if (forecast[i].precipProb >= cfg_precip_thr) {
        alert      = ALERT_RAIN;
        alertColor = CRGB((cfg_rain_color >> 16) & 0xFF, (cfg_rain_color >> 8) & 0xFF, cfg_rain_color & 0xFF);
      } else if (forecast[i].tempMin <= cfg_freeze_thr) {
        alert      = ALERT_FREEZE;
        alertColor = CRGB((cfg_freeze_color >> 16) & 0xFF, (cfg_freeze_color >> 8) & 0xFF, cfg_freeze_color & 0xFF);
      } else if (forecast[i].tempMax >= cfg_heat_thr) {
        alert      = ALERT_HEAT;
        alertColor = CRGB((cfg_heat_color >> 16) & 0xFF, (cfg_heat_color >> 8) & 0xFF, cfg_heat_color & 0xFF);
      } else {
        alert      = ALERT_NONE;
        alertColor = CRGB::Black;
      }

      ledStates[i].baseColor  = base;
      ledStates[i].alertColor = alertColor;
      ledStates[i].alert      = alert;
      ledStates[i].blendAmt   = 0;
      ledStates[i].fadeDir    = 1;
      ledStates[i].lastTick        = now;
      ledStates[i].holdUntil       = now + (unsigned long)(cfg_hold_sec * 1000.0f);
      ledStates[i].alertHoldUntil  = 0;

      leds[i] = base;
    }
    for (int i = cfg_num_leds; i < MAX_LEDS; i++) {
      leds[i] = CRGB::Black;
      ledStates[i].alert = ALERT_NONE;
    }
  } else {
    Serial.println("pollWeather: fetch failed, showing dim white");
    fill_solid(leds, cfg_num_leds, CRGB(10, 10, 10));
    for (int i = 0; i < cfg_num_leds; i++) {
      ledStates[i].alert = ALERT_NONE;
    }
    for (int i = cfg_num_leds; i < MAX_LEDS; i++) {
      leds[i] = CRGB::Black;
      ledStates[i].alert = ALERT_NONE;
    }
  }

  FastLED.show();
  return ok;
}

void tickAnimations() {
  bool changed = false;
  unsigned long now = millis();
  unsigned long holdMs       = (unsigned long)(cfg_hold_sec * 1000.0f);
  unsigned long alertHoldMs  = (unsigned long)(cfg_alert_hold_sec * 1000.0f);
  unsigned long attackIntervalMs = (unsigned long)(cfg_attack_sec * 1000.0f * FADE_STEP / 255.0f + 0.5f);
  if (attackIntervalMs < 1) attackIntervalMs = 1;
  unsigned long decayIntervalMs  = (unsigned long)(cfg_decay_sec  * 1000.0f * FADE_STEP / 255.0f + 0.5f);
  if (decayIntervalMs < 1) decayIntervalMs = 1;

  for (int i = 0; i < MAX_LEDS; i++) {
    if (ledStates[i].alert == ALERT_NONE) continue;
    if (now < ledStates[i].holdUntil) continue;          // holding on base color
    if (now < ledStates[i].alertHoldUntil) continue;     // holding on alert color
    unsigned long intervalMs = (ledStates[i].fadeDir == 1) ? attackIntervalMs : decayIntervalMs;
    if (now - ledStates[i].lastTick < intervalMs) continue;

    ledStates[i].lastTick = now;
    ledStates[i].blendAmt += ledStates[i].fadeDir * FADE_STEP;

    if (ledStates[i].blendAmt >= 255) {
      ledStates[i].blendAmt       = 255;
      ledStates[i].fadeDir        = -1;
      ledStates[i].alertHoldUntil = now + alertHoldMs;  // pause at full alert color
    } else if (ledStates[i].blendAmt <= 0) {
      ledStates[i].blendAmt  = 0;
      ledStates[i].fadeDir   = 1;
      ledStates[i].holdUntil = now + holdMs;  // restart hold on base
    }

    leds[i] = blend(ledStates[i].baseColor, ledStates[i].alertColor,
                    (uint8_t)ledStates[i].blendAmt);
    changed = true;
  }
  if (changed) FastLED.show();
}

// Forward declarations for handlers used by startAPMode() and setup()
void handleRoot();
void handleSave();
void handleScan();
void handleOtaUpdate();
void handleOtaUpload();

// Sets the DHCP hostname to "weather-leds-XXXXXX" where XXXXXX is the last
// 3 bytes of the MAC address in lowercase hex. Must be called before WiFi.begin()
// for the name to be registered via DHCP.
void applyHostname() {
  static char hostname[20];
#ifndef UNIT_TESTING
  // WiFi.macAddress() returns all-zeroes before WiFi.begin(), so read the
  // base MAC via ESP.getEfuseMac() which is available at any time.
  // getEfuseMac() packs mac[0..5] into a uint64 with mac[0] at the LSB.
  uint64_t mac64 = ESP.getEfuseMac();
  snprintf(hostname, sizeof(hostname), "weather-leds-%02x%02x%02x",
           (uint8_t)(mac64 >> 24),   // mac[3]
           (uint8_t)(mac64 >> 32),   // mac[4]
           (uint8_t)(mac64 >> 40));  // mac[5]
#else
  snprintf(hostname, sizeof(hostname), "weather-leds-112233");
#endif
  WiFi.setHostname(hostname);
}

void startAPMode() {
  g_ap_mode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);

#ifndef UNIT_TESTING
  dnsServer.start(53, "*", WiFi.softAPIP());
#endif

  server.on("/",     HTTP_GET,  handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", HTTP_GET,  handleScan);
  // Captive portal: redirect all other paths to the config page
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.printf("AP mode: connect to \"%s\", then open http://192.168.4.1/\n", AP_SSID);
  fill_solid(leds, cfg_num_leds, CRGB(0, 0, 40));
  FastLED.show();
}

void handleRoot() {
  static char page[17408];
  String ip = g_ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  const char* stationDisplay = g_ap_mode ? "none" : "block";
  // Format 24-bit RGB as #rrggbb (always 7 chars + NUL = 8 bytes).
  // Mask to 24 bits before formatting to keep the value in range.
  char freezeColorHex[8], heatColorHex[8], rainColorHex[8];
  snprintf(freezeColorHex, sizeof(freezeColorHex), "#%06x", (unsigned)(cfg_freeze_color & 0xFFFFFFU));
  snprintf(heatColorHex,   sizeof(heatColorHex),   "#%06x", (unsigned)(cfg_heat_color   & 0xFFFFFFU));
  snprintf(rainColorHex,   sizeof(rainColorHex),   "#%06x", (unsigned)(cfg_rain_color   & 0xFFFFFFU));
  char dfltFreezeHex[8], dfltHeatHex[8], dfltRainHex[8];
  snprintf(dfltFreezeHex, sizeof(dfltFreezeHex), "#%06x", (unsigned)(DEFAULT_FREEZE_COLOR & 0xFFFFFFU));
  snprintf(dfltHeatHex,   sizeof(dfltHeatHex),   "#%06x", (unsigned)(DEFAULT_HEAT_COLOR   & 0xFFFFFFU));
  snprintf(dfltRainHex,   sizeof(dfltRainHex),   "#%06x", (unsigned)(DEFAULT_RAIN_COLOR   & 0xFFFFFFU));
  snprintf(page, sizeof(page), CONFIG_HTML,
           g_ap_mode ? "block" : "none",  // AP setup banner
           stationDisplay,                 // main-cfg section (hidden in AP mode)
           cfg_brightness, cfg_poll_min, cfg_num_leds,
           cfg_cold_temp, cfg_hot_temp,
           cfg_latitude, cfg_longitude,
           cfg_freeze_thr, freezeColorHex,     // nerdy: freeze threshold + color
           cfg_heat_thr,   heatColorHex,        // nerdy: heat threshold + color
           cfg_precip_thr, rainColorHex,        // nerdy: precip threshold + rain color
           cfg_hold_sec, cfg_alert_hold_sec, cfg_attack_sec, cfg_decay_sec,  // nerdy: timing
           cfg_wifi_ssid,                  // SSID field
           stationDisplay,                 // station-only section (poll + OTA)
           FIRMWARE_VERSION, FIRMWARE_BUILD_TIMESTAMP,
           ip.c_str(),
           DEFAULT_HOLD_SEC, DEFAULT_ALERT_HOLD_SEC, DEFAULT_ATTACK_SEC, DEFAULT_DECAY_SEC,  // timing defaults for reset JS
           DEFAULT_FREEZE_THR_F, dfltFreezeHex,   // threshold/color defaults for reset JS
           DEFAULT_HEAT_THR_F,   dfltHeatHex,
           DEFAULT_PRECIP_THR_PCT, dfltRainHex);
  server.send(200, "text/html", page);
}

void handleSave() {
  if (server.hasArg("brightness"))
    cfg_brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
  if (server.hasArg("poll_min"))
    cfg_poll_min = (uint16_t)constrain(server.arg("poll_min").toInt(), 1, 1440);
  if (server.hasArg("num_leds")) {
    uint8_t newVal = (uint8_t)constrain(server.arg("num_leds").toInt(), 1, MAX_LEDS);
    if (newVal != cfg_num_leds) {
      cfg_num_leds = newVal;
      g_forceRepoll = true;
    }
  }
  if (server.hasArg("cold_temp"))
    cfg_cold_temp = server.arg("cold_temp").toFloat();
  if (server.hasArg("hot_temp"))
    cfg_hot_temp  = server.arg("hot_temp").toFloat();
  if (cfg_cold_temp >= cfg_hot_temp)
    cfg_hot_temp = cfg_cold_temp + 1.0f;
  float    prev_freeze_thr   = cfg_freeze_thr;
  float    prev_heat_thr     = cfg_heat_thr;
  float    prev_precip_thr   = cfg_precip_thr;
  uint32_t prev_freeze_color = cfg_freeze_color;
  uint32_t prev_heat_color   = cfg_heat_color;
  uint32_t prev_rain_color   = cfg_rain_color;

  if (server.hasArg("freeze_thr"))
    cfg_freeze_thr = server.arg("freeze_thr").toFloat();
  if (server.hasArg("heat_thr"))
    cfg_heat_thr   = server.arg("heat_thr").toFloat();
  if (server.hasArg("precip_thr"))
    cfg_precip_thr = server.arg("precip_thr").toFloat();
  if (server.hasArg("freeze_color")) {
    String c = server.arg("freeze_color");
    if (c.length() == 7 && c.c_str()[0] == '#') {
      uint32_t v; if (sscanf(c.c_str(), "#%06x", &v) == 1) cfg_freeze_color = v;
    }
  }
  if (server.hasArg("heat_color")) {
    String c = server.arg("heat_color");
    if (c.length() == 7 && c.c_str()[0] == '#') {
      uint32_t v; if (sscanf(c.c_str(), "#%06x", &v) == 1) cfg_heat_color = v;
    }
  }
  if (server.hasArg("rain_color")) {
    String c = server.arg("rain_color");
    if (c.length() == 7 && c.c_str()[0] == '#') {
      uint32_t v; if (sscanf(c.c_str(), "#%06x", &v) == 1) cfg_rain_color = v;
    }
  }
  if (cfg_freeze_thr   != prev_freeze_thr   || cfg_heat_thr   != prev_heat_thr   ||
      cfg_precip_thr   != prev_precip_thr   || cfg_freeze_color != prev_freeze_color ||
      cfg_heat_color   != prev_heat_color   || cfg_rain_color   != prev_rain_color)
    g_forceRepoll = true;
  if (server.hasArg("hold_sec"))
    cfg_hold_sec = constrain(server.arg("hold_sec").toFloat(), 0.1f, 60.0f);
  if (server.hasArg("alert_hold_sec"))
    cfg_alert_hold_sec = constrain(server.arg("alert_hold_sec").toFloat(), 0.0f, 10.0f);
  if (server.hasArg("attack_sec"))
    cfg_attack_sec = constrain(server.arg("attack_sec").toFloat(), 0.1f, 10.0f);
  if (server.hasArg("decay_sec"))
    cfg_decay_sec = constrain(server.arg("decay_sec").toFloat(), 0.1f, 10.0f);

  bool locationChanged = false;
  if (server.hasArg("latitude")) {
    String lat = server.arg("latitude");
    if (lat != String(cfg_latitude)) locationChanged = true;
    lat.toCharArray(cfg_latitude, sizeof(cfg_latitude));
  }
  if (server.hasArg("longitude")) {
    String lon = server.arg("longitude");
    if (lon != String(cfg_longitude)) locationChanged = true;
    lon.toCharArray(cfg_longitude, sizeof(cfg_longitude));
  }

  bool wifiChanged = false;
  if (server.hasArg("wifi_ssid")) {
    String ssid = server.arg("wifi_ssid");
    if (ssid != String(cfg_wifi_ssid)) wifiChanged = true;
    ssid.toCharArray(cfg_wifi_ssid, sizeof(cfg_wifi_ssid));
  }
  if (server.hasArg("wifi_pass") && !server.arg("wifi_pass").isEmpty()) {
    wifiChanged = true;
    server.arg("wifi_pass").toCharArray(cfg_wifi_pass, sizeof(cfg_wifi_pass));
  }

  FastLED.setBrightness(cfg_brightness);
  saveConfig();
  if (locationChanged) g_forceRepoll = true;
  if (wifiChanged)     g_pendingConnect = true;

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePollNow() {
  g_forceRepoll = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;

  struct ScanEntry { String ssid; int rssi; };
  ScanEntry entries[32];
  int count = 0;

  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    int    r = WiFi.RSSI(i);
    bool dup = false;
    for (int j = 0; j < count; j++) {
      if (entries[j].ssid == s) { dup = true; break; }
    }
    if (!dup && count < 32) {
      entries[count++] = {s, r};
    }
  }

  // insertion sort descending by RSSI
  for (int i = 1; i < count; i++) {
    ScanEntry key = entries[i];
    int j = i - 1;
    while (j >= 0 && entries[j].rssi < key.rssi) {
      entries[j + 1] = entries[j--];
    }
    entries[j + 1] = key;
  }

  static char buf[4096];
  int pos = snprintf(buf, sizeof(buf), "[");
  for (int i = 0; i < count; i++) {
    if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "{\"ssid\":\"%s\",\"rssi\":%d}",
                    entries[i].ssid.c_str(), entries[i].rssi);
  }
  snprintf(buf + pos, sizeof(buf) - pos, "]");
  server.send(200, "application/json", buf);
}

void handleOtaUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA: start, file=%s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Serial.printf("OTA: begin failed: %s\n", Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.printf("OTA: write error: %s\n", Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA: success, %u bytes\n", upload.totalSize);
    } else {
      Serial.printf("OTA: end failed: %s\n", Update.errorString());
    }
  }
}

void handleOtaUpdate() {
  if (Update.hasError()) {
    server.send(500, "text/plain",
                String("Update failed: ") + Update.errorString());
  } else {
    server.sendHeader("Location", "/");
    server.send(303);
#ifndef UNIT_TESTING
    delay(200);
    ESP.restart();
#endif
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP32 Temp Forecast LED Indicator v" FIRMWARE_VERSION
                 " (built " FIRMWARE_BUILD_TIMESTAMP ") — starting up");

  loadConfig();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(cfg_brightness);

  // Dim white boot indicator on configured LEDs
  fill_solid(leds, cfg_num_leds, CRGB(20, 20, 20));
  FastLED.show();

  if (strlen(cfg_wifi_ssid) == 0) {
    Serial.println("No WiFi credentials saved — starting AP mode");
    startAPMode();
    return;
  }

  Serial.printf("Connecting to WiFi: %s\n", cfg_wifi_ssid);
  applyHostname();
  WiFi.begin(cfg_wifi_ssid, cfg_wifi_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed — starting AP mode");
    startAPMode();
    return;
  }

  Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/save",   HTTP_POST, handleSave);
  server.on("/poll",   HTTP_POST, handlePollNow);
  server.on("/scan",   HTTP_GET,  handleScan);
  server.on("/update", HTTP_POST, handleOtaUpdate, handleOtaUpload);
  server.begin();
  Serial.printf("Web UI: http://%s/\n", WiFi.localIP().toString().c_str());

  // Clear LEDs after successful connect
  fill_solid(leds, MAX_LEDS, CRGB(0, 0, 0));
  FastLED.show();
}

void loop() {
  static bool firstRun = true;
  static unsigned long lastPollTime = 0;

  if (g_ap_mode) {
#ifndef UNIT_TESTING
    dnsServer.processNextRequest();
#endif
    server.handleClient();

    if (g_pendingConnect) {
      g_pendingConnect = false;
      WiFi.softAPdisconnect(true);
#ifndef UNIT_TESTING
      dnsServer.stop();
#endif
      Serial.printf("Trying to connect to \"%s\"...\n", cfg_wifi_ssid);
      applyHostname();
      WiFi.begin(cfg_wifi_ssid, cfg_wifi_pass);

      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
        delay(500);
        Serial.print(".");
      }
      Serial.println();

      if (WiFi.status() == WL_CONNECTED) {
        g_ap_mode = false;
        Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        server.on("/poll",   HTTP_POST, handlePollNow);
        server.on("/update", HTTP_POST, handleOtaUpdate, handleOtaUpload);
        fill_solid(leds, MAX_LEDS, CRGB(0, 0, 0));
        FastLED.show();
        firstRun = true;
      } else {
        Serial.println("Connection failed — returning to AP mode");
        startAPMode();
      }
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("loop: WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  server.handleClient();

  unsigned long pollMs = (unsigned long)cfg_poll_min * 60UL * 1000UL;
  if (firstRun || g_forceRepoll || (millis() - lastPollTime >= pollMs)) {
    firstRun = false;
    g_forceRepoll = false;
    bool ok = pollWeather();
    // On success, reset the full poll interval.  On failure, retry after 1 min
    // so transient network errors don't leave the display stuck for cfg_poll_min.
    if (ok) {
      lastPollTime = millis();
    } else {
      lastPollTime = millis() - pollMs + 60UL * 1000UL;
    }
  }

  tickAnimations();
}
