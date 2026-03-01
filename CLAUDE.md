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

`secrets.h` is gitignored. Copy `secrets.h.example` → `secrets.h` and fill in `WIFI_SSID` and `WIFI_PASSWORD`. Latitude/longitude are configured at runtime via the web UI.

## Architecture

- `setup()` — loads config, initializes FastLED, connects WiFi (dim-orange on failure), starts `WebServer`, prints IP, clears LEDs
- `loop()` — calls `server.handleClient()` every iteration; polls immediately on boot, then every `cfg_poll_min` minutes; `g_forceRepoll` flag triggers an out-of-schedule poll
- `loadConfig()` / `saveConfig()` — reads/writes all runtime settings to NVS via `Preferences` (namespace `"wxleds"`)
- `handleRoot()` — serves the HTML config page (`config_html.h`) with current config values injected via `snprintf`
- `handleSave()` — processes POST from the config form, validates inputs, persists settings via `saveConfig()`, applies `FastLED.setBrightness()` immediately; sets `g_forceRepoll` if location changed
- `handlePollNow()` — sets `g_forceRepoll = true`, triggering a poll on the next `loop()` iteration
- `pollWeather()` — fetches all 6 days in one request, sets all LEDs, calls `FastLED.show()` once
- `fetchForecast()` — HTTPS GET to Open-Meteo using `cfg_latitude`/`cfg_longitude`, filters JSON for max/min/precip arrays, computes daily averages
- `tempToColor()` — maps °F to CHSV using runtime bounds: `cfg_cold_temp`→hue 160 (blue, default 20°F), `cfg_hot_temp`→hue 0 (red, default 90°F)

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
- `test_handle_root` — `handleRoot()` rendering: HTTP 200, buffer not truncated, country select present with US default, config values injected
- `test_handle_save` — `handleSave()` logic: location-change repoll flag, brightness/poll_min clamping, cold/hot temp correction, 303 redirect, NVS persistence

To add a new test file, create `tests/test_<name>.cpp` and add `add_sketch_test(test_<name>.cpp)` to `tests/CMakeLists.txt`.

**VS Code:** Install the **C++ TestMate** extension (`matepek.vscode-catch2-test-adapter`). `.vscode/settings.json` is already configured to point it at `tests/build/test_*`. Build the tests first so the binaries exist for TestMate to discover.

**GitHub Actions:** `.github/workflows/tests.yml` runs on every push and PR to any branch. It configures, builds, and runs the tests on `ubuntu-latest`, then publishes per-test results via `dorny/test-reporter` as a check on the commit. The CMake build directory (including FetchContent downloads) is cached keyed on `tests/CMakeLists.txt` to speed up subsequent runs.

## Critical constraints

- **ArduinoJson v7 only.** Filter arrays with `filter["daily"]["temperature_2m_max"][0] = true` (the `[0]` is required to retain the whole array). Access via `.as<JsonArray>()`.
- **Single `FastLED.show()` per poll cycle** — per-LED calls cause flicker.
- **`forecast_days` equals `NUM_LEDS`** — changing LED count automatically adjusts the API request.
- **`handleRoot()` page buffer is `static`** — the 8 KB buffer must remain `static char page[8192]` to avoid a stack overflow in the WebServer callback. The ESP32 loop task stack is ~8 KB total; a plain local allocation of that size crashes the device.
