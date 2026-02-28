#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include "secrets.h"

#include <Preferences.h>
#include <WebServer.h>

#define LED_PIN           23
#define NUM_LEDS          6

// Compile-time defaults — overridden at runtime via web UI + NVS
#define DEFAULT_BRIGHTNESS      50
#define DEFAULT_POLL_MIN        30        // minutes
#define DEFAULT_COLD_TEMP_F     20.0f
#define DEFAULT_HOT_TEMP_F      90.0f
#define DEFAULT_LATITUDE        "33.7490"
#define DEFAULT_LONGITUDE       "-84.3880"
#define DEFAULT_FREEZE_THR_F    32.0f
#define DEFAULT_HEAT_THR_F      95.0f
#define DEFAULT_PRECIP_THR_PCT  50.0f

// Runtime config (loaded from NVS, used everywhere)
uint8_t  cfg_brightness = DEFAULT_BRIGHTNESS;
uint16_t cfg_poll_min   = DEFAULT_POLL_MIN;
float    cfg_cold_temp  = DEFAULT_COLD_TEMP_F;
float    cfg_hot_temp   = DEFAULT_HOT_TEMP_F;
char     cfg_latitude[16]  = DEFAULT_LATITUDE;
char     cfg_longitude[16] = DEFAULT_LONGITUDE;
float    cfg_freeze_thr = DEFAULT_FREEZE_THR_F;
float    cfg_heat_thr   = DEFAULT_HEAT_THR_F;
float    cfg_precip_thr = DEFAULT_PRECIP_THR_PCT;

// Alert colors
#define COLOR_FREEZE  CRGB(0,   0,   200)
#define COLOR_HEAT    CRGB(255, 0,   0)
#define COLOR_RAIN    CRGB(0,   200, 200)

// Animation timing: ~500ms fade each way (255/3 steps × 6ms), 2s hold on base temperature color
#define FADE_STEP         3
#define FADE_INTERVAL_MS  6
#define HOLD_MS           3000UL

Preferences prefs;
WebServer   server(80);

static bool g_forceRepoll = false;

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
  int       blendAmt;    // 0–255
  int       fadeDir;     // +1 or -1
  unsigned long lastTick;
  unsigned long holdUntil; // millis() when hold-on-base phase ends
};

CRGB leds[NUM_LEDS];
LEDState ledStates[NUM_LEDS];

void loadConfig() {
  prefs.begin("wxleds", /*readOnly=*/true);
  cfg_brightness = prefs.getUChar("brightness",  DEFAULT_BRIGHTNESS);
  cfg_poll_min   = prefs.getUShort("poll_min",   DEFAULT_POLL_MIN);
  cfg_cold_temp  = prefs.getFloat("cold_temp",   DEFAULT_COLD_TEMP_F);
  cfg_hot_temp   = prefs.getFloat("hot_temp",    DEFAULT_HOT_TEMP_F);
  cfg_freeze_thr = prefs.getFloat("freeze_thr",  DEFAULT_FREEZE_THR_F);
  cfg_heat_thr   = prefs.getFloat("heat_thr",    DEFAULT_HEAT_THR_F);
  cfg_precip_thr = prefs.getFloat("precip_thr",  DEFAULT_PRECIP_THR_PCT);
  prefs.getString("latitude",  DEFAULT_LATITUDE).toCharArray(cfg_latitude,  sizeof(cfg_latitude));
  prefs.getString("longitude", DEFAULT_LONGITUDE).toCharArray(cfg_longitude, sizeof(cfg_longitude));
  prefs.end();
  Serial.printf("Config: brt=%d poll=%dmin cold=%.1f hot=%.1f lat=%s lon=%s\n",
                cfg_brightness, cfg_poll_min, cfg_cold_temp, cfg_hot_temp,
                cfg_latitude, cfg_longitude);
}

void saveConfig() {
  prefs.begin("wxleds", /*readOnly=*/false);
  prefs.putUChar("brightness",  cfg_brightness);
  prefs.putUShort("poll_min",   cfg_poll_min);
  prefs.putFloat("cold_temp",   cfg_cold_temp);
  prefs.putFloat("hot_temp",    cfg_hot_temp);
  prefs.putFloat("freeze_thr",  cfg_freeze_thr);
  prefs.putFloat("heat_thr",    cfg_heat_thr);
  prefs.putFloat("precip_thr",  cfg_precip_thr);
  prefs.putString("latitude",   cfg_latitude);
  prefs.putString("longitude",  cfg_longitude);
  prefs.end();
}

const char CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html>
<head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weather LEDs Config</title>
<style>
body{font-family:sans-serif;max-width:500px;margin:2em auto;padding:0 1em;background:#1a1a1a;color:#e0e0e0}
h1{font-size:1.3em}
label{display:block;margin-top:1em;font-size:.9em;color:#aaa}
input[type=number],input[type=text]{width:100%%;box-sizing:border-box;padding:.4em;font-size:1em;background:#2a2a2a;color:#e0e0e0;border:1px solid #444;border-radius:4px}
.btn{margin-top:1.2em;padding:.5em 1.2em;font-size:1em;cursor:pointer}
.save{background:#1e7a4a;color:#fff;border:none;border-radius:4px}
.poll{background:#4a3a7a;color:#fff;border:none;border-radius:4px}
.footer{margin-top:2em;font-size:.8em;color:#555}
</style>
</head><body>
<h1>Weather LEDs Config</h1>
<form method="POST" action="/save">
<label>Brightness (0-255)</label>
<input type="number" name="brightness" min="0" max="255" value="%d">
<label>Poll interval (minutes)</label>
<input type="number" name="poll_min" min="1" max="1440" value="%d">
<label>Cold temperature &deg;F (blue end)</label>
<input type="number" name="cold_temp" step="0.1" value="%.1f">
<label>Hot temperature &deg;F (red end)</label>
<input type="number" name="hot_temp" step="0.1" value="%.1f">
<label>Latitude</label>
<input type="text" name="latitude" maxlength="12" value="%s">
<label>Longitude</label>
<input type="text" name="longitude" maxlength="12" value="%s">
<label>Freeze threshold &deg;F</label>
<input type="number" name="freeze_thr" step="0.1" value="%.1f">
<label>Heat threshold &deg;F</label>
<input type="number" name="heat_thr" step="0.1" value="%.1f">
<label>Precipitation threshold %%</label>
<input type="number" name="precip_thr" step="0.1" min="0" max="100" value="%.1f">
<br>
<button class="btn save" type="submit">Save</button>
</form>
<form method="POST" action="/poll">
<button class="btn poll" type="submit">Poll Now</button>
</form>
<div class="footer">Device IP: %s</div>
</body></html>)rawhtml";

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
    cfg_latitude, cfg_longitude, NUM_LEDS);

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

  for (int i = 0; i < NUM_LEDS; i++) {
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

void pollWeather() {
  DayForecast forecast[NUM_LEDS];

  Serial.println("pollWeather: fetching forecast...");
  bool ok = fetchForecast(forecast);

  if (ok) {
    unsigned long now = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      CRGB base = tempToColor(forecast[i].tempAvg);

      AlertType alert;
      CRGB alertColor;
      if (forecast[i].precipProb >= cfg_precip_thr) {
        alert      = ALERT_RAIN;
        alertColor = COLOR_RAIN;
      } else if (forecast[i].tempMin <= cfg_freeze_thr) {
        alert      = ALERT_FREEZE;
        alertColor = COLOR_FREEZE;
      } else if (forecast[i].tempMax >= cfg_heat_thr) {
        alert      = ALERT_HEAT;
        alertColor = COLOR_HEAT;
      } else {
        alert      = ALERT_NONE;
        alertColor = CRGB::Black;
      }

      ledStates[i].baseColor  = base;
      ledStates[i].alertColor = alertColor;
      ledStates[i].alert      = alert;
      ledStates[i].blendAmt   = 0;
      ledStates[i].fadeDir    = 1;
      ledStates[i].lastTick   = now;
      ledStates[i].holdUntil  = now + HOLD_MS;

      leds[i] = base;
    }
  } else {
    Serial.println("pollWeather: fetch failed, showing dim white");
    fill_solid(leds, NUM_LEDS, CRGB(10, 10, 10));
    for (int i = 0; i < NUM_LEDS; i++) {
      ledStates[i].alert = ALERT_NONE;
    }
  }

  FastLED.show();
}

void tickAnimations() {
  bool changed = false;
  unsigned long now = millis();

  for (int i = 0; i < NUM_LEDS; i++) {
    if (ledStates[i].alert == ALERT_NONE) continue;
    if (now < ledStates[i].holdUntil) continue;          // holding on base color
    if (now - ledStates[i].lastTick < FADE_INTERVAL_MS) continue;

    ledStates[i].lastTick = now;
    ledStates[i].blendAmt += ledStates[i].fadeDir * FADE_STEP;

    if (ledStates[i].blendAmt >= 255) {
      ledStates[i].blendAmt = 255;
      ledStates[i].fadeDir  = -1;
    } else if (ledStates[i].blendAmt <= 0) {
      ledStates[i].blendAmt  = 0;
      ledStates[i].fadeDir   = 1;
      ledStates[i].holdUntil = now + HOLD_MS;  // restart 3s hold on base
    }

    leds[i] = blend(ledStates[i].baseColor, ledStates[i].alertColor,
                    (uint8_t)ledStates[i].blendAmt);
    changed = true;
  }
  if (changed) FastLED.show();
}

void handleRoot() {
  char page[3000];
  String ip = WiFi.localIP().toString();
  snprintf(page, sizeof(page), CONFIG_HTML,
           cfg_brightness, cfg_poll_min,
           cfg_cold_temp, cfg_hot_temp,
           cfg_latitude, cfg_longitude,
           cfg_freeze_thr, cfg_heat_thr, cfg_precip_thr,
           ip.c_str());
  server.send(200, "text/html", page);
}

void handleSave() {
  if (server.hasArg("brightness"))
    cfg_brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
  if (server.hasArg("poll_min"))
    cfg_poll_min = (uint16_t)constrain(server.arg("poll_min").toInt(), 1, 1440);
  if (server.hasArg("cold_temp"))
    cfg_cold_temp = server.arg("cold_temp").toFloat();
  if (server.hasArg("hot_temp"))
    cfg_hot_temp  = server.arg("hot_temp").toFloat();
  if (cfg_cold_temp >= cfg_hot_temp)
    cfg_hot_temp = cfg_cold_temp + 1.0f;
  if (server.hasArg("freeze_thr"))
    cfg_freeze_thr = server.arg("freeze_thr").toFloat();
  if (server.hasArg("heat_thr"))
    cfg_heat_thr   = server.arg("heat_thr").toFloat();
  if (server.hasArg("precip_thr"))
    cfg_precip_thr = server.arg("precip_thr").toFloat();

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

  FastLED.setBrightness(cfg_brightness);
  saveConfig();
  if (locationChanged) g_forceRepoll = true;

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePollNow() {
  g_forceRepoll = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP32 Temp Forecast LED Indicator — starting up");

  loadConfig();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(cfg_brightness);

  // Dim white boot indicator
  fill_solid(leds, NUM_LEDS, CRGB(20, 20, 20));
  FastLED.show();

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed — showing dim orange");
    fill_solid(leds, NUM_LEDS, CRGB(20, 10, 0));
    FastLED.show();
    return;
  }

  Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/",     HTTP_GET,  handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/poll", HTTP_POST, handlePollNow);
  server.begin();
  Serial.printf("Web UI: http://%s/\n", WiFi.localIP().toString().c_str());

  // Clear LEDs after successful connect
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();
}

void loop() {
  static bool firstRun = true;
  static unsigned long lastPollTime = 0;

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
    pollWeather();
    lastPollTime = millis();
  }

  tickAnimations();
}
