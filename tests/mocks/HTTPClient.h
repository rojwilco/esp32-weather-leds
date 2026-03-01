#pragma once

#include "Arduino.h"
#include "WiFiClientSecure.h"

// ── Injectable mock state ─────────────────────────────────────────────────────
// Tests set these before calling the code under test.
extern int    g_mock_http_code;
extern String g_mock_http_payload;
extern bool   g_mock_http_begin_ok;   // controls whether begin() succeeds

static constexpr int HTTP_CODE_OK = 200;

// ── HTTPClient mock ───────────────────────────────────────────────────────────
struct HTTPClient {
    bool   begin(WiFiClientSecure&, const char*) { return g_mock_http_begin_ok; }
    int    GET()       { return g_mock_http_code; }
    String getString() { return g_mock_http_payload; }
    void   end()       {}
};
