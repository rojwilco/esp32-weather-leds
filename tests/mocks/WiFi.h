// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 rojwilco
// AI-assisted: generated with Claude (Anthropic); human conception, review, guidance and output approval by rojwilco.
#pragma once

#include "Arduino.h"
#include <string>
#include <vector>

// ── IPAddress ─────────────────────────────────────────────────────────────────
struct IPAddress {
    String toString() const { return String("192.168.1.1"); }
};

// ── WiFi connection status constants ─────────────────────────────────────────
static constexpr int WL_CONNECTED    = 3;
static constexpr int WL_DISCONNECTED = 6;

// ── WiFi mode constants ───────────────────────────────────────────────────────
static constexpr int WIFI_STA = 1;
static constexpr int WIFI_AP  = 2;

// ── Scan mock state ───────────────────────────────────────────────────────────
struct MockNetwork { std::string ssid; int rssi; };
extern std::vector<MockNetwork> g_mock_scan_results;

// ── WiFiClass stub ────────────────────────────────────────────────────────────
// Tests can set g_mock_wifi_status to control connection state.
extern int         g_mock_wifi_status;
extern std::string g_mock_wifi_hostname;

struct WiFiClass {
    void begin(const char*, const char*) {}
    int  status() const { return g_mock_wifi_status; }
    void reconnect() {}
    IPAddress localIP()   const { return IPAddress{}; }
    void      setHostname(const char* name) { g_mock_wifi_hostname = name; }
    String    macAddress() const { return String("AA:BB:CC:11:22:33"); }

    // AP mode
    void      mode(int) {}
    bool      softAP(const char* /*ssid*/, const char* /*pass*/ = nullptr) { return true; }
    bool      softAPdisconnect(bool = false) { return true; }
    IPAddress softAPIP() const { return IPAddress{}; }

    // Scan API
    int    scanNetworks() { return (int)g_mock_scan_results.size(); }
    String SSID(int i)    { return String(g_mock_scan_results[i].ssid); }
    int    RSSI(int i)    { return g_mock_scan_results[i].rssi; }
};

extern WiFiClass WiFi;
