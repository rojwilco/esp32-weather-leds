#pragma once

// Stub — the real SSL handshake is irrelevant for host-side tests.
struct WiFiClientSecure {
    void setInsecure() {}
};
