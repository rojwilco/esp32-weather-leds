#pragma once

#include "Arduino.h"

// ── IPAddress ─────────────────────────────────────────────────────────────────
struct IPAddress {
    String toString() const { return String("192.168.1.1"); }
};

// ── WiFi connection status constants ─────────────────────────────────────────
static constexpr int WL_CONNECTED    = 3;
static constexpr int WL_DISCONNECTED = 6;

// ── WiFiClass stub ────────────────────────────────────────────────────────────
// Tests can set g_mock_wifi_status to control connection state.
extern int g_mock_wifi_status;

struct WiFiClass {
    void begin(const char*, const char*) {}
    int  status() const { return g_mock_wifi_status; }
    void reconnect() {}
    IPAddress localIP() const { return IPAddress{}; }
};

extern WiFiClass WiFi;
