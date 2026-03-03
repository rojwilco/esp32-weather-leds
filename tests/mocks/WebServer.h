#pragma once

#include "Arduino.h"
#include <map>
#include <string>
#include <functional>

// HTTP method tokens expected by the sketch
static constexpr int HTTP_GET  = 0;
static constexpr int HTTP_POST = 1;

// ── Injectable mock state ─────────────────────────────────────────────────────
extern std::map<std::string, std::string> g_mock_server_args;

// Capture last server.send() call so tests can assert on it.
extern int    g_mock_last_send_code;
extern String g_mock_last_send_body;
extern String g_mock_last_redirect;

// ── WebServer mock ────────────────────────────────────────────────────────────
struct WebServer {
    explicit WebServer(int /*port*/) {}

    void begin() {}
    void handleClient() {}

    void on(const char* /*uri*/, int /*method*/, std::function<void()> /*handler*/) {}
    void onNotFound(std::function<void()> /*handler*/) {}

    bool   hasArg(const String& key) const {
        return g_mock_server_args.count(std::string(key.c_str())) > 0;
    }
    String arg(const String& key) const {
        auto it = g_mock_server_args.find(std::string(key.c_str()));
        return it != g_mock_server_args.end() ? String(it->second) : String("");
    }

    void send(int code, const char* = "", const char* body = "") {
        g_mock_last_send_code = code;
        g_mock_last_send_body = String(body);
    }
    void send(int code, const char* ct, const String& body) {
        g_mock_last_send_code = code;
        g_mock_last_send_body = body;
    }

    void sendHeader(const char* name, const String& value) {
        if (std::string(name) == "Location") g_mock_last_redirect = value;
    }
};
