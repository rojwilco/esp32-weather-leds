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

## First-boot WiFi setup

WiFi credentials are configured at runtime — there is no `secrets.h`.
On first boot (or whenever no credentials are saved in NVS), the device
starts in AP mode: it broadcasts an open network named **ESP32-Weather**.
Connect to that network, then visit `http://192.168.4.1/` to enter your
SSID (use the Scan button) and password. After saving, the device reboots
into normal station mode. Credentials persist in NVS across reboots.
Latitude/longitude are also set via the web UI.

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
- `test_handle_root` — `handleRoot()` rendering: HTTP 200, buffer not truncated, country select present with US default, config values injected, firmware version and build timestamp present in footer
- `test_handle_save` — `handleSave()` logic: location-change repoll flag, brightness/poll_min clamping, cold/hot temp correction, 303 redirect, NVS persistence
- `test_handle_scan` — WiFi scan JSON output: sorting by RSSI, deduplication

To add a new test file, create `tests/test_<name>.cpp` and add `add_sketch_test(test_<name>.cpp)` to `tests/CMakeLists.txt`.

### Test style

Every `TEST_F` must include a 1–2 sentence description in two places:

1. A `// Description:` comment immediately above the `TEST_F` line.
2. A `RecordProperty("description", "...")` call as the **first statement** in the test body. This embeds the description as a `<property>` element in the JUnit XML output consumed by CI.

```cpp
// Description: Temperatures far below the cold endpoint clamp to hue 160 (blue)
// rather than wrapping or going negative.
TEST_F(TempToColorTest, BelowColdClampsToBlue) {
    RecordProperty("description",
        "Temperatures far below the cold endpoint clamp to hue 160 (blue) "
        "rather than wrapping or going negative.");
    // ... assertions ...
}
```

The description should state *what property is being verified and why it matters*, not restate the test name. `RecordProperty` is a static method inherited from `::testing::Test`; call it unqualified (not `::testing::RecordProperty`).

**VS Code (optional):** To enable inline test running and results in the IDE, install the **C++ TestMate** extension (`matepek.vscode-catch2-test-adapter`) and add the following to your `.vscode/` config files:

`.vscode/tasks.json` — defines a build task that runs `make build` in `tests/`:
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build tests",
      "type": "shell",
      "command": "make build",
      "options": { "cwd": "${workspaceFolder}/tests" },
      "group": { "kind": "build", "isDefault": true },
      "presentation": { "reveal": "silent", "revealProblems": "onProblem" },
      "problemMatcher": "$gcc"
    }
  ]
}
```

`.vscode/settings.json` — points TestMate at the test binaries and wires up the build task so tests are always rebuilt before running:
```json
{
  "testMate.cpp.test.advancedExecutables": [
    {
      "pattern": "tests/build/test_*",
      "runTask": { "before": ["Build tests"] }
    }
  ],
  "C_Cpp.default.compileCommands": "tests/build/compile_commands.json"
}
```

With this in place, clicking **Run tests** in TestMate automatically runs `make build` first, and `Cmd+Shift+B` triggers the build directly. The `compile_commands.json` setting gives the C++ extension accurate IntelliSense for the test files.

**GitHub Actions:** Two workflows run in CI:
- `.github/workflows/tests.yml` — runs on every push and PR; configures, builds, and runs the host tests on `ubuntu-latest`; publishes per-test results via `dorny/test-reporter`. The CMake build directory is cached keyed on `tests/CMakeLists.txt`.
- `.github/workflows/build.yml` — compiles the sketch on every push, PR, and tag; on `v*.*.*` tags the `release` job additionally publishes a GitHub Release (see [Publishing a release](#publishing-a-release) below).

## Commit conventions

When a commit closes a GitHub issue, the `Closes #N` keyword **must appear on its own line** in the commit body (not embedded in the subject line). GitHub only auto-closes issues when the keyword is on a standalone line.

```
fix: short subject line describing what changed

Longer explanation of why, if needed.

Closes #42
```

Do **not** write `fix: short subject (closes #42)` — GitHub will not pick that up.

## Versioning

Firmware version is defined in `version.h` using three separate macros:

```cpp
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
```

**When to bump** (semver rules):
- **PATCH** — bug fixes, test additions, doc changes, no behaviour change
- **MINOR** — new features that are backwards-compatible (new web UI field, new config option, new LED behaviour)
- **MAJOR** — breaking changes (NVS key renames that wipe saved config, wire-protocol changes, pin reassignments)

**How to bump:** edit only the relevant line(s) in `version.h`. The string (`FIRMWARE_VERSION`), packed integer (`FW_VERSION_INT`), and build timestamp (`FIRMWARE_BUILD_TIMESTAMP`) all derive from those three defines automatically — no other files need touching.

`FIRMWARE_VERSION` appends a `"-dev"` suffix in all local and CI branch builds (via `FW_VERSION_SUFFIX`). The release CI job overrides this to `""` via a compiler flag. Do not modify `FW_VERSION_SUFFIX` in `version.h` itself.

**When to bump:** bump the version as part of the same commit that completes a feature or fix, before pushing. Every PR that changes sketch behaviour should include a version increment. Do **not** add an additional PATCH bump for bug fixes made within the same PR that introduced the feature — the existing version increment already covers them.

The version appears:
- On the serial monitor at boot: `v1.0.0 (built Mar  7 2026 22:19:15)`
- In the config web UI footer: `Firmware: 1.0.0 (built ...) | Device IP: ...`

### Publishing a release

1. Bump the version in `version.h` and commit it (per the rules above).
2. Create and push an annotated tag:
   ```bash
   git tag -a v1.2.3 -m "Release v1.2.3"
   git push origin v1.2.3
   ```
3. The `release` job in `.github/workflows/build.yml` triggers automatically. It:
   - Compiles with `-DFW_VERSION_SUFFIX=""` (no `-dev` suffix).
   - Names the binary `esp32-weather-leds-v<version>.bin`.
   - Creates a GitHub Release titled `v<version>` with GitHub-generated notes and the binary attached.

No manual upload is needed — pushing the tag is the complete release action.

## Critical constraints

- **ArduinoJson v7 only.** Filter arrays with `filter["daily"]["temperature_2m_max"][0] = true` (the `[0]` is required to retain the whole array). Access via `.as<JsonArray>()`.
- **Single `FastLED.show()` per poll cycle** — per-LED calls cause flicker.
- **`forecast_days` equals `NUM_LEDS`** — changing LED count automatically adjusts the API request.
- **`handleRoot()` page buffer is `static`** — the buffer must remain `static char page[...]` (currently 10 KB) to avoid a stack overflow in the WebServer callback. The ESP32 loop task stack is ~8 KB total; a plain local allocation of that size crashes the device. The `static` keyword is what matters; the buffer size may be grown as needed.
