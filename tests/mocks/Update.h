#pragma once
#include "Arduino.h"
#include <cstddef>

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

// ── Injectable mock state ─────────────────────────────────────────────────────
extern bool        g_mock_update_begin_ok;
extern bool        g_mock_update_write_fail;
extern bool        g_mock_update_end_ok;
extern bool        g_mock_update_has_error;
extern const char* g_mock_update_error_str;
extern size_t      g_mock_update_written;

// ── UpdateClass mock ──────────────────────────────────────────────────────────
struct UpdateClass {
    bool begin(size_t /*size*/) {
        g_mock_update_written = 0;
        return g_mock_update_begin_ok;
    }
    size_t write(uint8_t* /*buf*/, size_t len) {
        if (g_mock_update_write_fail) return 0;
        g_mock_update_written += len;
        return len;
    }
    bool end(bool /*finish*/ = true) { return g_mock_update_end_ok; }
    bool hasError() const            { return g_mock_update_has_error; }
    const char* errorString() const  { return g_mock_update_error_str; }
    bool isRunning() const           { return true; }
};

extern UpdateClass Update;
