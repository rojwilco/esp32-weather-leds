# ESP32 Weather LED Indicator

Displays up to 16 days of average temperature forecast on up to 16 WS2812B LEDs (configurable, default 6).
Each LED represents one day, color-coded from blue (cold) to red (hot).

## Hardware

- WeMos D1 Mini (ESP32)
- 1–16x WS2812B addressable LEDs on GPIO 23 (default 6, configurable via web UI; GRB order, ensure load resistor on data is present or place your own)

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
| Brightness | 50 | LED brightness (0–255) |
| Freeze alert threshold | 32°F | Alert when daily low ≤ this value |
| Heat alert threshold | 95°F | Alert when daily high ≥ this value |
| Precipitation alert threshold | 50% | Alert when precipitation probability ≥ this value |
| Cold color bound | 20°F | Temperature mapped to blue |
| Hot color bound | 90°F | Temperature mapped to red |
| Latitude / Longitude | — | The page includes a ZIP code lookup that resolves to lat/lon in the browser (via zippopotam.us) — the ESP32 never makes this request |
| Poll interval | 30 min | How often to fetch a new forecast |
| Number of LEDs/days | 6 | Number of forecast days (1–16); LED 1 = today, LED N = N-1 days ahead |

Settings are saved to flash (NVS) and persist across reboots. The "Poll Now" button forces an immediate weather fetch.

## Data Source

[Open-Meteo](https://open-meteo.com/) — free, no API key required.
