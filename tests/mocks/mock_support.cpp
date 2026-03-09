// Definitions for all extern globals declared in the mock headers.
// This file is compiled into every test executable via sketch_lib.

#include "Arduino.h"
#include "FastLED.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "Preferences.h"
#include "WebServer.h"
#include "Update.h"

#include <map>
#include <string>

// ── Arduino.h globals ─────────────────────────────────────────────────────────
unsigned long g_mock_millis = 0;
SerialClass   Serial;

// ── FastLED.h globals ─────────────────────────────────────────────────────────
CFastLED         FastLED;
const CRGB CRGB::Black = CRGB(0, 0, 0);

// ── WiFi.h globals ────────────────────────────────────────────────────────────
int         g_mock_wifi_status   = WL_CONNECTED;
std::string g_mock_wifi_hostname;
WiFiClass   WiFi;
std::vector<MockNetwork> g_mock_scan_results;

// ── HTTPClient.h globals ──────────────────────────────────────────────────────
int    g_mock_http_code     = 200;
String g_mock_http_payload  = "";
bool   g_mock_http_begin_ok = true;

// ── Preferences.h globals ─────────────────────────────────────────────────────
std::map<std::string, std::string> g_mock_prefs_store;

// ── WebServer.h globals ───────────────────────────────────────────────────────
std::map<std::string, std::string> g_mock_server_args;
int    g_mock_last_send_code = 0;
String g_mock_last_send_body;
String g_mock_last_redirect;
HTTPUpload g_mock_upload;

// ── Update.h globals ──────────────────────────────────────────────────────────
bool        g_mock_update_begin_ok   = true;
bool        g_mock_update_write_fail = false;
bool        g_mock_update_end_ok     = true;
bool        g_mock_update_has_error  = false;
const char* g_mock_update_error_str  = "";
size_t      g_mock_update_written    = 0;
UpdateClass Update;
