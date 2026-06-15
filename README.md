# ESP32 Weather LED Indicator

Displays up to 16 days of weather forecast on up to 16 WS2812B LEDs (default 6, configurable via web UI).
Each LED represents one day, color-coded blue (cold) to red (hot). LEDs pulse to alert on freeze days
(daily low ≤ 32°F), heat days (daily high ≥ 95°F), and rainy days (precipitation probability ≥ 50%) —
all thresholds configurable.

## Hardware

![Assembled weather LED bar showing a rain-alert day](docs/demo.gif)

### Components

- **ESP32 controller** — [D1 Mini NodeMCU ESP32-WROOM-32](https://www.amazon.com/dp/B08L5XFWN6) (sold in 3-packs)
- **LED strip** — [BTF-LIGHTING WS2812B ECO 100 LED/m, IP30](https://www.amazon.com/dp/B088B8G8LD) — cut to the number of LEDs you need (default 6, max 16); wired to GPIO 23 (GRB order, include a load resistor on the data line)
- **LED channel/case** — [LED bar graph enclosure (MakerWorld, 3D-printable)](https://makerworld.com/en/models/2452077-led-bar-graph)
- **D1 Mini case** — many options exist on MakerWorld/Printables; choice depends on your board variant and header configuration

## Color Scale

| Temperature | Color  |
|-------------|--------|
| ≤ 20°F      | Blue   |
| ~55°F       | Green  |
| ≥ 90°F      | Red    |

Bounds default to 20°F (cold/blue) and 90°F (hot/red) and are configurable via the web UI.

## Setup

1. Install the Arduino IDE or `arduino-cli`
2. Install board package: **esp32 by Espressif Systems**
3. Install libraries: **FastLED 3.10.3**, **ArduinoJson v7.x**
4. Set board `esp32:esp32:d1_mini32`
5. Compile and flash (see CLAUDE.md) — no `secrets.h` needed
6. On first boot the device starts in AP mode. Connect to the open Wi-Fi
   network **ESP32-WeatherLED** and visit `http://192.168.4.1/` to enter your
   WiFi credentials (use the **Scan** button to see nearby networks). The AP
   mode page shows only WiFi fields; full config is available after connecting
   to your network. After saving, the device reboots and connects normally.

## Configuration

**First boot:** the device broadcasts an open AP named **ESP32-WeatherLED**; connect and visit `http://192.168.4.1/` to set credentials. The AP mode page shows only WiFi fields; full config is available after connecting to your network. After saving, the device reboots into station mode and prints its IP to the serial monitor (`Web UI: http://<ip>/`). Visit that address to change any setting:

| Setting | Default | Description |
|---|---|---|
| WiFi SSID | — | Network name; use the **Scan** button to list visible networks |
| WiFi Password | — | Saved in NVS; leave blank on the settings page to keep the current password |
| Number of LEDs/days | 6 | Number of forecast days (1–16); LED 1 = today, LED N = N-1 days ahead |
| Brightness | 50 | LED brightness (0–255) |
| Poll interval | 30 min | How often to fetch a new forecast |
| Cold color bound | 20°F | Temperature mapped to blue |
| Hot color bound | 90°F | Temperature mapped to red |
| Latitude / Longitude | — | The page includes a ZIP code lookup that resolves to lat/lon in the browser (via zippopotam.us) — the ESP32 never makes this request |
| Freeze alert threshold | 32°F | Alert when daily low ≤ this value |
| Freeze alert color | `#c8c8ff` (icy blue) | Color pulsed by freeze-alert LEDs |
| Heat alert threshold | 95°F | Alert when daily high ≥ this value |
| Heat alert color | `#ff00ff` (magenta) | Color pulsed by heat-alert LEDs |
| Precipitation alert threshold | 50% | Alert when precipitation probability ≥ this value |
| Rain alert color | `#00c8c8` (cyan) | Color pulsed by rain-alert LEDs |

Settings are saved to flash (NVS) and persist across reboots. The **Basic Settings** section has a **Reset to defaults** button that restores num\_leds, brightness, poll interval, and the cold/hot color bounds to their factory values without touching location or alert thresholds. The **Alert Thresholds** section has a matching button that restores all three thresholds and their alert colors. The **Poll Now** button forces an immediate weather fetch.

## Demo Mode

The **Demo Mode** button on the config page switches the device into a mode that drives LEDs without any WiFi or weather API access. Useful for:

- Testing LED wiring and verifying the full color spectrum
- Tuning alert thresholds and color bounds without waiting for a real forecast
- Retail or exhibition display

In demo mode all LEDs show a smooth cold→hot gradient (blue to red) spanning `cfg_cold_temp` to `cfg_hot_temp`. Alert animations fire wherever the gradient naturally crosses a configured threshold — each LED uses a ±5°F daily spread around its gradient midpoint, so:

- **Freeze** — LEDs at the cold end whose gradient temperature minus 5°F falls at or below `cfg_freeze_thr` pulse the freeze alert color. With defaults (cold=20°F, freeze threshold=32°F) this affects the coldest two or three LEDs depending on how many are configured.
- **Rain** — the middle LED (`n/2`) always pulses the rain alert color.
- **Heat** — LEDs at the hot end whose gradient temperature plus 5°F meets or exceeds `cfg_heat_thr` pulse the heat alert color.

Because alerts fire based on the live threshold settings, adjusting **Cold/Hot color bounds** or any **Alert threshold** in the config page and saving will immediately update which LEDs blink on the next poll cycle — letting you preview exactly how real weather data will behave before pointing the device at a live forecast.

Demo mode is saved to NVS and persists across reboots. Pressing **Demo Mode** again returns to normal weather polling.

## Factory Reset

If the device loses network access and you need to reconfigure it without a USB connection, use the **triple-reset** gesture:

1. Press **RST** once. The LED strip and the onboard LED will flash **light blue once** to confirm the first press, then boot normally.
2. Press **RST** again (any time during boot, or within 30 seconds of the LEDs showing weather). They will flash **light blue twice**, then boot normally.
3. Press **RST** a third time. The strip turns solid red and the onboard LED stays on, all settings are wiped, and the device reboots into AP mode.
4. Connect to the **ESP32-WeatherLED** network and reconfigure as on first boot.

> **Note:** The counter resets to zero 30 seconds after the device finishes booting, so stray presses during normal operation will never trigger a factory reset. Powering the device off also clears the counter.

## Data Source

[Open-Meteo](https://open-meteo.com/) — free, no API key required.
