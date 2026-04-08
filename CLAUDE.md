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
starts in AP mode: it broadcasts an open network named **ESP32-WeatherLED**.
Connect to that network, then visit `http://192.168.4.1/` to enter your
SSID (use the Scan button) and password. After saving, the device reboots
into normal station mode. Credentials persist in NVS across reboots.
Latitude/longitude are also set via the web UI.

## Architecture

- `setup()` — loads config, initializes FastLED, connects WiFi (dim-orange on failure), starts `WebServer`, prints IP, clears LEDs
- `loop()` — calls `server.handleClient()` every iteration; polls immediately on boot, then every `cfg_poll_min` minutes; `g_forceRepoll` flag triggers an out-of-schedule poll; dispatches to `pollDemoMode()` instead of `pollWeather()` when `g_demo_mode` is true
- `loadConfig()` / `saveConfig()` — reads/writes all runtime settings to NVS via `Preferences` (namespace `"wxleds"`); includes `g_demo_mode` (key `"demo_mode"`)
- `handleRoot()` — serves the HTML config page (`config_html.h`) with current config values injected via `snprintf`; page adapts based on device mode: AP mode shows only WiFi fields, station mode shows full config. **The order of arguments in the `snprintf` call must exactly match the order of `%` placeholders in `CONFIG_HTML`** — positional format strings give no compile-time protection against mismatches.
- `handleSave()` — processes POST from the config form, validates inputs, persists settings via `saveConfig()`, applies `FastLED.setBrightness()` immediately; sets `g_forceRepoll` if location or `cfg_num_leds` changed; `cfg_num_leds` is clamped to 1–16
- `handlePollNow()` — sets `g_forceRepoll = true`, triggering a poll on the next `loop()` iteration
- `handleDemo()` — toggles `g_demo_mode`, saves to NVS, sets `g_forceRepoll = true`, redirects to `/`
- `applyForecast()` — shared helper called by both `pollWeather()` (on success) and `pollDemoMode()`; initialises `ledStates[]` and `leds[]` from a `DayForecast` array using the alert priority logic (rain > freeze > heat); does not call `FastLED.show()`
- `pollWeather()` — fetches `cfg_num_leds` days in one request, calls `applyForecast()`, calls `FastLED.show()` once
- `pollDemoMode()` — builds a synthetic `DayForecast` array without any network access and calls `applyForecast()` + `FastLED.show()`. Each LED gets a `tempAvg` interpolated linearly from `cfg_cold_temp` (LED 0) to `cfg_hot_temp` (LED n−1), with `tempMax = tAvg + 5` and `tempMin = tAvg − 5`. Alert LEDs are seeded as: freeze at index 0 (`tempMin = cfg_freeze_thr − 1`), rain at index `n/2` (`precipProb = cfg_precip_thr + 10`), heat at index `n−1` (`tempMax = cfg_heat_thr + 1`). Because the gradient's `tempMin` values can also fall below `cfg_freeze_thr` on the cold end (and `tempMax` above `cfg_heat_thr` on the hot end), **multiple LEDs may animate an alert in demo mode** — this is intentional: it lets users see exactly how threshold and range changes affect the display without fetching live data.
- `fetchForecast()` — HTTPS GET to Open-Meteo using `cfg_latitude`/`cfg_longitude`, filters JSON for max/min/precip arrays, computes daily averages
- `tempToColor()` — maps °F to CHSV using runtime bounds: `cfg_cold_temp`→hue 160 (blue, default 20°F), `cfg_hot_temp`→hue 0 (red, default 90°F)

### Animation timing math

`tickAnimations()` advances each alert LED by **1 blend unit per tick** (`FADE_STEP = 1`). Single-unit steps are required for perceptual smoothness: human vision is logarithmic (Weber-Fechner), so a step of 3/255 that looks smooth at full brightness becomes a visible staircase at low brightness. With step size 1, the fade is always smooth regardless of `cfg_brightness`.

The attack (base→alert) and decay (alert→base) phases each have an independent tick interval derived from their duration setting:

```
attack_interval_ms = max(1, round(cfg_attack_sec × 1000 / 255))   // rising phase
decay_interval_ms  = max(1, round(cfg_decay_sec  × 1000 / 255))   // falling phase
```

Total phase duration ≈ 255 steps × interval_ms ≈ `cfg_attack_sec` or `cfg_decay_sec` seconds.

Example with the defaults `cfg_attack_sec = cfg_decay_sec = 0.5` (symmetric, identical to the old single `fade_sec`):
- `interval_ms = round(0.5 × 1000 / 255 + 0.5) = round(2.46) = 2 ms`
- 255 ticks × 2 ms = 510 ms ≈ 0.5 s per direction

**Radar-ping example** (`cfg_attack_sec = 0.1`, `cfg_decay_sec = 2.0`):
- Attack: `round(0.1 × 1000 / 255 + 0.5) = 1 ms` → snaps to alert color in ~255 ms
- Decay: `round(2.0 × 1000 / 255 + 0.5) = 8 ms` → fades back slowly over ~2 s

**Do not increase `FADE_STEP`** — doing so trades smoothness for a coarser interval and reintroduces visible stepping at low brightness.

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
- `test_animation_cycles` — multi-cycle animation behaviour
- `test_handle_root` — `handleRoot()` rendering: HTTP 200, buffer not truncated, country select present with US default, config values injected, firmware version and build timestamp present in footer, demo mode button present and `snprintf` arg ordering correct
- `test_handle_save` — `handleSave()` logic: location-change repoll flag, brightness/poll_min clamping, cold/hot temp correction, 303 redirect, NVS persistence
- `test_handle_scan` — WiFi scan JSON output: sorting by RSSI, deduplication
- `test_handle_ota` — OTA upload handler
- `test_hostname` — DHCP hostname generation from MAC address
- `test_demo_mode` — `pollDemoMode()` gradient endpoints, alert placement, show count, `handleDemo()` toggle behaviour

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

### Verification checklist in commit body

When a commit implements a testable feature or fix, include a **Verification** section in the commit body as a markdown checkbox list. GitHub renders these as interactive checkboxes in the PR description, giving reviewers a clear, checkable test plan.

```
feat: add configurable LED/day count via web UI

Longer explanation of why, if needed.

Verification:
- [ ] `cd tests && make` passes all tests
- [ ] Setting num_leds=4 → API requests forecast_days=4; LEDs 4–15 are black
- [ ] Values outside 1–16 are clamped; unchanged value does not trigger repoll

Closes #11
```

## Versioning

Firmware version is defined in `version.h` using three separate macros:

```cpp
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0
```

**When to bump** (semver rules):
- **PATCH** — bug fixes or test additions that change compiled firmware behaviour
- **MINOR** — new features that are backwards-compatible (new web UI field, new config option, new LED behaviour)
- **MAJOR** — breaking changes (NVS key renames that wipe saved config, wire-protocol changes, pin reassignments)
- **No bump** — README, CLAUDE.md, or other doc-only changes that do not affect the compiled binary

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
- **`forecast_days` equals `cfg_num_leds`** — the runtime config value (not a compile-time constant) controls both the LED count and the number of forecast days requested from the API.
- **`handleRoot()` page buffer is `static`** — the buffer must remain `static char page[...]` (currently 17 KB) to avoid a stack overflow in the WebServer callback. The ESP32 loop task stack is ~8 KB total; a plain local allocation of that size crashes the device. The `static` keyword is what matters; the buffer size may be grown as needed.
- **`snprintf` argument order must match template placeholder order** — `CONFIG_HTML` uses positional C format specifiers (`%s`, `%d`, `%.1f`, …). The arguments passed to `snprintf` in `handleRoot()` must appear in exactly the same order as their corresponding `%` placeholders in the template. There is no compile-time check for mismatches; adding a new placeholder in the middle of the template without inserting its argument at the matching position will silently inject wrong values into wrong fields. When adding a new placeholder, count its position in the template and insert the argument at the same position in the `snprintf` call.
