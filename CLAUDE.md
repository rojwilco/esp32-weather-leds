# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Build and flash

Arduino sketch targeting the LOLIN D32 (ESP32). Compile and flash via the Arduino IDE or `arduino-cli`.

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:lolin_d32 esp32-weather-leds

# Flash (adjust port — macOS example)
arduino-cli upload --fqbn esp32:esp32:lolin_d32 --port /dev/cu.usbserial-* esp32-weather-leds

# Monitor serial output
arduino-cli monitor --port /dev/cu.usbserial-* --config baudrate=115200
```

Required board package: **esp32 by Espressif Systems** (3.3.7+)
Required libraries: **FastLED** 3.10.3, **ArduinoJson** v7.x (v6 is incompatible)

## Secrets

`secrets.h` is gitignored. Copy `secrets.h.example` → `secrets.h` and fill in `WIFI_SSID`, `WIFI_PASSWORD`, `LATITUDE`, `LONGITUDE`.

## Architecture

- `setup()` — initializes FastLED, connects WiFi (dim-orange on failure), clears LEDs
- `loop()` — polls immediately on boot, then every 30 minutes
- `pollWeather()` — fetches all 6 days in one request, sets all LEDs, calls `FastLED.show()` once
- `fetchForecast()` — HTTPS GET to Open-Meteo, filters JSON for max/min arrays, computes daily averages
- `tempToColor()` — maps °F to CHSV: 20°F→hue 160 (blue), 100°F→hue 0 (red)

## Critical constraints

- **ArduinoJson v7 only.** Filter arrays with `filter["daily"]["temperature_2m_max"][0] = true` (the `[0]` is required to retain the whole array). Access via `.as<JsonArray>()`.
- **Single `FastLED.show()` per poll cycle** — per-LED calls cause flicker.
- **`LATITUDE`/`LONGITUDE` are string `#define`s** — interpolated via `%s` in `snprintf`.
- **`forecast_days` equals `NUM_LEDS`** — changing LED count automatically adjusts the API request.
