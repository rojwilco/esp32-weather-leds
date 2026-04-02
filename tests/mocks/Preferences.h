#pragma once

#include "Arduino.h"
#include <map>
#include <string>

// ── In-memory Preferences (replaces ESP32 NVS) ────────────────────────────────
// The same backing store is reused across begin()/end() calls within one test.
// Call g_mock_prefs_store.clear() in test SetUp() if isolation is needed.
extern std::map<std::string, std::string> g_mock_prefs_store;

struct Preferences {
    void begin(const char* /*ns*/, bool /*readOnly*/ = false) {}
    void end() {}

    // ── getters ───────────────────────────────────────────────────────────────
    uint8_t  getUChar(const char* key, uint8_t  def = 0) const      { return get_or(key, def); }
    uint16_t getUShort(const char* key, uint16_t def = 0) const     { return get_or(key, def); }
    uint32_t getUInt(const char* key, uint32_t def = 0) const       { return get_or(key, def); }
    float    getFloat(const char* key, float    def = 0.0f) const   { return get_or(key, def); }
    String   getString(const char* key, const char* def = "") const {
        auto it = g_mock_prefs_store.find(key);
        return it != g_mock_prefs_store.end() ? String(it->second) : String(def);
    }

    // ── putters ───────────────────────────────────────────────────────────────
    void putUChar(const char* key,  uint8_t  v) { g_mock_prefs_store[key] = std::to_string(v); }
    void putUShort(const char* key, uint16_t v) { g_mock_prefs_store[key] = std::to_string(v); }
    void putUInt(const char* key,   uint32_t v) { g_mock_prefs_store[key] = std::to_string(v); }
    void putFloat(const char* key,  float    v) { g_mock_prefs_store[key] = std::to_string(v); }
    void putString(const char* key, const char* v) { g_mock_prefs_store[key] = v; }
    void putString(const char* key, const String& v) { g_mock_prefs_store[key] = v.c_str(); }

private:
    template<typename T>
    T get_or(const char* key, T def) const {
        auto it = g_mock_prefs_store.find(key);
        if (it == g_mock_prefs_store.end()) return def;
        try {
            if constexpr (std::is_same_v<T, float>)    return std::stof(it->second);
            if constexpr (std::is_same_v<T, uint8_t>)  return static_cast<uint8_t>(std::stoul(it->second));
            if constexpr (std::is_same_v<T, uint16_t>) return static_cast<uint16_t>(std::stoul(it->second));
            if constexpr (std::is_same_v<T, uint32_t>) return static_cast<uint32_t>(std::stoul(it->second));
            return static_cast<T>(std::stod(it->second));
        } catch (...) { return def; }
    }
};
