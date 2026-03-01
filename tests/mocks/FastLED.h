#pragma once

#include <cstdint>
#include <cstring>

// ── CHSV ─────────────────────────────────────────────────────────────────────
struct CHSV {
    uint8_t h, s, v;
    CHSV() : h(0), s(0), v(0) {}
    CHSV(uint8_t h, uint8_t s, uint8_t v) : h(h), s(s), v(v) {}
};

// ── CRGB ─────────────────────────────────────────────────────────────────────
struct CRGB {
    uint8_t r, g, b;

    // Track the HSV origin when converted from CHSV (useful in tests).
    uint8_t _hue = 0;
    bool    _fromHSV = false;

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    // HSV → RGB conversion (standard algorithm matching FastLED's behaviour
    // closely enough for unit tests).
    CRGB(const CHSV& hsv);

    bool operator==(const CRGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const CRGB& o) const { return !(*this == o); }

    static const CRGB Black;
};

// ── Free functions ────────────────────────────────────────────────────────────

// Linear blend from a (amount=0) to b (amount=255).
inline CRGB blend(const CRGB& a, const CRGB& b, uint8_t amount) {
    if (amount == 0)   return a;
    if (amount == 255) return b;
    return CRGB(
        static_cast<uint8_t>(a.r + (static_cast<int>(b.r - a.r) * amount) / 255),
        static_cast<uint8_t>(a.g + (static_cast<int>(b.g - a.g) * amount) / 255),
        static_cast<uint8_t>(a.b + (static_cast<int>(b.b - a.b) * amount) / 255)
    );
}

inline void fill_solid(CRGB* leds, int num, const CRGB& color) {
    for (int i = 0; i < num; i++) leds[i] = color;
}

// ── FastLED object ────────────────────────────────────────────────────────────
// Enum values so the template parameters compile; values don't matter.
enum FastLEDLedType  { WS2812B = 1, WS2812 = 2, NEOPIXEL = 3 };
enum FastLEDColorOrder { GRB = 0, RGB = 1, BGR = 2 };

struct CFastLED {
    int showCount  = 0;
    int brightness = 255;

    template<int TYPE, int PIN, int ORDER>
    void addLeds(CRGB*, int) {}

    void setBrightness(int b) { brightness = b; }
    void show()  { ++showCount; }
    void clear() {}
    void resetShowCount() { showCount = 0; }
};

extern CFastLED FastLED;

// ── CHSV → CRGB implementation ────────────────────────────────────────────────
inline CRGB::CRGB(const CHSV& hsv) : _hue(hsv.h), _fromHSV(true) {
    uint8_t h = hsv.h, s = hsv.s, v = hsv.v;
    if (s == 0) { r = g = b = v; return; }

    uint8_t region    = h / 43;
    uint8_t remainder = static_cast<uint8_t>((h - region * 43) * 6);

    uint8_t p = static_cast<uint8_t>((static_cast<int>(v) * (255 - s)) >> 8);
    uint8_t q = static_cast<uint8_t>((static_cast<int>(v) * (255 - ((static_cast<int>(s) * remainder) >> 8))) >> 8);
    uint8_t t = static_cast<uint8_t>((static_cast<int>(v) * (255 - ((static_cast<int>(s) * (255 - remainder)) >> 8))) >> 8);

    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}
