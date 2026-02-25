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

CRGB leds[NUM_LEDS];

CRGB tempToColor(float tempF) {
  float fraction = constrain((tempF - COLD_TEMP_F) / (HOT_TEMP_F - COLD_TEMP_F), 0.0f, 1.0f);
  uint8_t hue = 160 - (uint8_t)(fraction * 160.0f);
  return CHSV(hue, 255, 255);
}

bool fetchForecast(float* outTemps) {
  char url[256];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=%s&longitude=%s"
    "&daily=temperature_2m_max,temperature_2m_min"
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

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

  if (err) {
    Serial.printf("fetchForecast: JSON parse error: %s\n", err.c_str());
    return false;
  }

  JsonArray maxArr = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray minArr = doc["daily"]["temperature_2m_min"].as<JsonArray>();

  if (maxArr.isNull() || minArr.isNull()) {
    Serial.println("fetchForecast: missing temperature arrays in response");
    return false;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    float tMax = maxArr[i].as<float>();
    float tMin = minArr[i].as<float>();
    float tAvg = (tMax + tMin) / 2.0f;
    outTemps[i] = tAvg;
    Serial.printf("  day %d: max=%.1f  min=%.1f  avg=%.1f\n", i, tMax, tMin, tAvg);
  }

  return true;
}

void pollWeather() {
  float temps[NUM_LEDS];

  Serial.println("pollWeather: fetching forecast...");
  bool ok = fetchForecast(temps);

  if (ok) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = tempToColor(temps[i]);
    }
  } else {
    Serial.println("pollWeather: fetch failed, showing dim white");
    fill_solid(leds, NUM_LEDS, CRGB(10, 10, 10));
  }

  FastLED.show();
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
}
