#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>

// ── String ───────────────────────────────────────────────────────────────────
// Standalone class (NOT inheriting std::string) so ArduinoJson's template
// specialisations don't conflict (inheriting std::string triggers both the
// ArduinoStringReader and the IteratorReader specialisations simultaneously).
//
// Satisfies:
//   • ArduinoJson ArduinoStringReader: is_base_of<::String, String> → true,
//     c_str(), length()
//   • ArduinoJson ArduinoStringWriter: concat(const char*)
//   • Sketch usage: +=, +, ==, !=, toInt(), toFloat(), toCharArray(), isEmpty()
class String {
public:
    String() = default;
    String(const char* s)        : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(std::string&& s)      : _s(std::move(s)) {}

    explicit String(int n)           : _s(std::to_string(n)) {}
    explicit String(unsigned int n)  : _s(std::to_string(n)) {}
    explicit String(long n)          : _s(std::to_string(n)) {}
    explicit String(unsigned long n) : _s(std::to_string(n)) {}
    explicit String(float f)         : _s(std::to_string(f)) {}
    explicit String(double f)        : _s(std::to_string(f)) {}

    // Core interface used by ArduinoJson readers/writers
    const char* c_str()  const { return _s.c_str(); }
    size_t      length() const { return _s.length(); }
    size_t      size()   const { return _s.size(); }

    // ArduinoJson ArduinoStringWriter calls concat() when serialising into a String
    bool concat(const char* s)    { _s += s; return true; }
    bool concat(const String& s)  { _s += s._s; return true; }

    // Arduino API
    void  toCharArray(char* buf, size_t bufSize) const {
        strncpy(buf, _s.c_str(), bufSize - 1);
        buf[bufSize - 1] = '\0';
    }
    long  toInt()   const { try { return std::stol(_s); } catch (...) { return 0; } }
    float toFloat() const { try { return std::stof(_s); } catch (...) { return 0.0f; } }
    bool  isEmpty() const { return _s.empty(); }

    // Operators
    String& operator+=(const String& rhs) { _s += rhs._s; return *this; }
    String& operator+=(const char* rhs)   { _s += rhs;    return *this; }
    String  operator+(const String& rhs)  const { return String(_s + rhs._s); }
    String  operator+(const char* rhs)    const { return String(_s + rhs); }

    bool operator==(const String& rhs) const { return _s == rhs._s; }
    bool operator==(const char* rhs)   const { return _s == rhs; }
    bool operator!=(const String& rhs) const { return _s != rhs._s; }
    bool operator!=(const char* rhs)   const { return _s != rhs; }
    bool operator< (const String& rhs) const { return _s < rhs._s; }

    // Allow explicit conversion to std::string where needed (e.g. map keys)
    explicit operator std::string() const { return _s; }

private:
    std::string _s;
};

// ── Timing ───────────────────────────────────────────────────────────────────
// Tests control time by writing g_mock_millis directly.
extern unsigned long g_mock_millis;
inline unsigned long millis() { return g_mock_millis; }
inline void delay(unsigned long) {}

// ── Math ─────────────────────────────────────────────────────────────────────
#ifndef constrain
#define constrain(amt, lo, hi) \
    ((amt) < (lo) ? (lo) : ((amt) > (hi) ? (hi) : (amt)))
#endif

// ── Serial stub ──────────────────────────────────────────────────────────────
struct SerialClass {
    void begin(int) {}
    void println()  {}
    template<typename T> void print(T)   {}
    template<typename T> void println(T) {}
    void printf(const char*, ...) {}
};
extern SerialClass Serial;

// ── PROGMEM (no-op on host) ───────────────────────────────────────────────────
#ifndef PROGMEM
#define PROGMEM
#endif
#define F(x) (x)

// ── Stub WiFi credentials (used by setup() when secrets.h is not included) ──
#ifndef WIFI_SSID
#  define WIFI_SSID     "test-ssid"
#  define WIFI_PASSWORD "test-pass"
#endif
