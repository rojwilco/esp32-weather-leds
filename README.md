# ESP32 Weather LED Indicator

Displays the next 6 days of average temperature forecast on 6 WS2812B LEDs.
Each LED represents one day, color-coded from blue (cold) to red (hot).

## Hardware

- WeMos D1 Mini (ESP32)
- 6x WS2812B addressable LEDs on GPIO 23 (GRB order, ensure load resistor on data is present or place your own)

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
5. Copy `secrets.h.example` → `secrets.h` and fill in your WiFi credentials (latitude/longitude can be set via the web UI after flashing)
6. Compile and flash (see CLAUDE.md)

## Configuration

After flashing, the device IP is printed to the serial monitor (`Web UI: http://<ip>/`). Visit `http://<ip>/` from any device on the same network to configure:

| Setting | Default | Description |
|---|---|---|
| Brightness | 50 | LED brightness (0–255) |
| Freeze alert threshold | 32°F | Alert when daily low ≤ this value |
| Heat alert threshold | 95°F | Alert when daily high ≥ this value |
| Precipitation alert threshold | 50% | Alert when precipitation probability ≥ this value |
| Cold color bound | 20°F | Temperature mapped to blue |
| Hot color bound | 90°F | Temperature mapped to red |
| Latitude / Longitude | — | The page includes a ZIP code lookup that resolves to lat/lon in the browser (via zippopotam.us) — the ESP32 never makes this request |
| Poll interval | 30 min | How often to fetch a new forecast |

Settings are saved to flash (NVS) and persist across reboots. The "Poll Now" button forces an immediate weather fetch.

## Data Source

[Open-Meteo](https://open-meteo.com/) — free, no API key required.
