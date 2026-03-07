#pragma once
#include <stdint.h>

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

#define FW_VERSION_INT \
    ((uint32_t)(FW_VERSION_MAJOR) << 16 | \
     (uint32_t)(FW_VERSION_MINOR) <<  8 | \
     (uint32_t)(FW_VERSION_PATCH))

// Stringify helpers
#define _FW_STR(x)  #x
#define _FW_XSTR(x) _FW_STR(x)
#define FIRMWARE_VERSION \
    _FW_XSTR(FW_VERSION_MAJOR) "." \
    _FW_XSTR(FW_VERSION_MINOR) "." \
    _FW_XSTR(FW_VERSION_PATCH)

#define FIRMWARE_BUILD_TIMESTAMP __DATE__ " " __TIME__
