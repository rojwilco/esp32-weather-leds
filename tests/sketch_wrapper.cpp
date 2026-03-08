// sketch_wrapper.cpp — compiles esp32-weather-leds.ino under the host test harness.
//
// Include order matters:
//   1. Mock headers must come before the .ino so angle-bracket includes
//      (e.g. <FastLED.h>) resolve to our stubs from the CMake include path.
//   2. UNIT_TESTING is defined by CMake (-DUNIT_TESTING) so the secrets guard
//      in the .ino fires without needing a local #define here.
//   3. WIFI_SSID / WIFI_PASSWORD are defined in mocks/Arduino.h as defaults.

#include "mocks/Arduino.h"
#include "mocks/FastLED.h"
#include "mocks/WiFi.h"
#include "mocks/HTTPClient.h"
#include "mocks/WiFiClientSecure.h"
#include "mocks/Preferences.h"
#include "mocks/WebServer.h"
#include "mocks/Update.h"

// Pull the entire sketch into this translation unit.
// Angle-bracket includes inside the .ino (<FastLED.h> etc.) will resolve to
// the mock headers above because CMake puts tests/mocks/ first in the search path.
#include "../esp32-weather-leds.ino"
