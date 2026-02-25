# ESP32 Weather LED Indicator

Displays the next 6 days of average temperature forecast on 6 WS2812B LEDs.
Each LED represents one day, color-coded from blue (cold) to red (hot).

## Hardware

- LOLIN D32 (ESP32)
- 6x WS2812B addressable LEDs on GPIO 23 (GRB order)

## Color Scale

| Temperature | Color  |
|-------------|--------|
| ≤ 20°F      | Blue   |
| ~60°F       | Green  |
| ≥ 100°F     | Red    |

## Setup

1. Install the Arduino IDE or `arduino-cli`
2. Install board package: **esp32 by Espressif Systems**
3. Install libraries: **FastLED 3.10.3**, **ArduinoJson v7.x**
4. Copy `secrets.h.example` → `secrets.h` and fill in your WiFi credentials and location
5. Compile and flash (see CLAUDE.md)

## Data Source

[Open-Meteo](https://open-meteo.com/) — free, no API key required.
