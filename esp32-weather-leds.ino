#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define LED_PIN           23
#define NUM_LEDS          6
#define LED_BRIGHTNESS    80      // 0–255
#define POLL_INTERVAL_MS  (30UL * 60UL * 1000UL)  // 30 minutes
#define COLD_TEMP_F       20.0f   // hue 160 = blue
#define HOT_TEMP_F        90.0f   // hue 0   = red

// Open-Meteo location — find yours at https://open-meteo.com/
#define LATITUDE   "38.9947"
#define LONGITUDE  "-77.0284"

// Alert thresholds
#define FREEZE_THRESHOLD_F    32.0f
#define HEAT_THRESHOLD_F      95.0f
#define PRECIP_THRESHOLD_PCT  50.0f

// Alert colors
#define COLOR_FREEZE  CRGB(0,   0,   200)
#define COLOR_HEAT    CRGB(255, 0,   0)
#define COLOR_RAIN    CRGB(0,   200, 200)

// Animation timing: ~500ms fade each way (255/3 steps × 6ms), 2s hold on base temperature color
#define FADE_STEP         3
#define FADE_INTERVAL_MS  6
#define HOLD_MS           2000UL

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

CRGB tempToColor(float tempF) {
  float fraction = constrain((tempF - COLD_TEMP_F) / (HOT_TEMP_F - COLD_TEMP_F), 0.0f, 1.0f);
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
    LATITUDE, LONGITUDE, NUM_LEDS);

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
      if (forecast[i].precipProb >= PRECIP_THRESHOLD_PCT) {
        alert      = ALERT_RAIN;
        alertColor = COLOR_RAIN;
      } else if (forecast[i].tempMin <= FREEZE_THRESHOLD_F) {
        alert      = ALERT_FREEZE;
        alertColor = COLOR_FREEZE;
      } else if (forecast[i].tempMax >= HEAT_THRESHOLD_F) {
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
      ledStates[i].holdUntil = now + HOLD_MS;  // restart 2s hold on base
    }

    leds[i] = blend(ledStates[i].baseColor, ledStates[i].alertColor,
                    (uint8_t)ledStates[i].blendAmt);
    changed = true;
  }
  if (changed) FastLED.show();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP32 Temp Forecast LED Indicator — starting up");


  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);

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

  if (firstRun || (millis() - lastPollTime >= POLL_INTERVAL_MS)) {
    firstRun = false;
    pollWeather();
    lastPollTime = millis();
  }

  tickAnimations();
}
