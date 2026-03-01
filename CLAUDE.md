# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Build and flash

Arduino sketch targeting the WeMos D1 Mini ESP32 (ESP32-WROOM-32). Compile and flash via the Arduino IDE or `arduino-cli`.

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:d1_mini32 esp32-weather-leds

# Flash (adjust port — macOS example)
arduino-cli upload --fqbn esp32:esp32:d1_mini32 --port /dev/cu.usbserial-* esp32-weather-leds

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

## Host tests

Hardware-free tests run on the host via Google Test + CMake. Test sources live in `tests/`, with Arduino/FastLED/WiFi stubs in `tests/mocks/`.

```bash
cd tests
make        # configure + build + run all tests
make build  # build only
make test   # run only (after building)
make clean  # remove build/
```

Dependencies (GoogleTest, ArduinoJson) are downloaded automatically by CMake via `FetchContent` on first build.

Test suites:
- `test_temp_to_color` — temperature-to-color mapping and clamping
- `test_fetch_forecast` — JSON parsing, HTTP error handling
- `test_poll_weather` — alert logic and LED state after a poll
- `test_animations` — per-tick fade/blend animation state machine

To add a new test file, create `tests/test_<name>.cpp` and add `add_sketch_test(test_<name>.cpp)` to `tests/CMakeLists.txt`.

**VS Code:** Install the **C++ TestMate** extension (`matepek.vscode-catch2-test-adapter`). `.vscode/settings.json` is already configured to point it at `tests/build/test_*`. Build the tests first so the binaries exist for TestMate to discover.

**GitHub Actions:** `.github/workflows/tests.yml` runs on every push and PR to any branch. It configures, builds, and runs the tests on `ubuntu-latest`, then publishes per-test results via `dorny/test-reporter` as a check on the commit. The CMake build directory (including FetchContent downloads) is cached keyed on `tests/CMakeLists.txt` to speed up subsequent runs.

## Critical constraints

- **ArduinoJson v7 only.** Filter arrays with `filter["daily"]["temperature_2m_max"][0] = true` (the `[0]` is required to retain the whole array). Access via `.as<JsonArray>()`.
- **Single `FastLED.show()` per poll cycle** — per-LED calls cause flicker.
- **`LATITUDE`/`LONGITUDE` are string `#define`s** — interpolated via `%s` in `snprintf`.
- **`forecast_days` equals `NUM_LEDS`** — changing LED count automatically adjusts the API request.
