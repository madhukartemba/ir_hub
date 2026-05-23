#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

/// Tiny RTC-memory slot that survives a soft reboot. The normal-mode firmware
/// writes a pending firmware URL + version here and then calls ESP.restart().
/// On the next boot, setup() reads (and immediately clears) this slot and, if
/// it's valid, enters a stripped-down "downloader mode" with ~32 KB free heap
/// to do the TLS download — far above BearSSL's transient handshake budget.
///
/// The slot is checksum-protected so RTC-RAM noise after a brown-out can't
/// trick the device into a fake OTA URL.
namespace pending_ota {

constexpr uint32_t kMagic = 0x07A0FACE;
// BootGuard occupies dwords 64-65 (see main.cpp). Sit at 66 onward.
constexpr uint32_t kRtcDwordOffset = 66;
constexpr size_t kUrlMax = 160;
constexpr size_t kVersionMax = 16;

struct Slot {
    uint32_t magic;
    char url[kUrlMax];
    char version[kVersionMax];
    uint32_t checksum;
};
static_assert(sizeof(Slot) % 4 == 0, "Slot must be a multiple of 4 bytes for RTC API");
static_assert(alignof(Slot) >= 4, "Slot must be uint32-aligned");

inline uint32_t computeChecksum(const Slot& s) {
    uint32_t c = s.magic;
    for (size_t i = 0; i < kUrlMax; i++) c = c * 31u + (uint8_t)s.url[i];
    for (size_t i = 0; i < kVersionMax; i++) c = c * 31u + (uint8_t)s.version[i];
    return c;
}

/// Returns false if URL/version is too long, or if the write itself fails.
inline bool arm(const char* url, const char* version) {
    if (!url || !version) return false;
    if (strlen(url) >= kUrlMax) return false;
    if (strlen(version) >= kVersionMax) return false;
    Slot s{};
    s.magic = kMagic;
    strncpy(s.url, url, kUrlMax - 1);
    strncpy(s.version, version, kVersionMax - 1);
    s.checksum = computeChecksum(s);
    return ESP.rtcUserMemoryWrite(kRtcDwordOffset,
                                  reinterpret_cast<uint32_t*>(&s),
                                  sizeof(s));
}

/// Returns true if a checksum-valid pending OTA was found and `out` is filled.
inline bool peek(Slot& out) {
    if (!ESP.rtcUserMemoryRead(kRtcDwordOffset,
                               reinterpret_cast<uint32_t*>(&out),
                               sizeof(out))) {
        return false;
    }
    if (out.magic != kMagic) return false;
    if (computeChecksum(out) != out.checksum) return false;
    out.url[kUrlMax - 1] = '\0';
    out.version[kVersionMax - 1] = '\0';
    return true;
}

inline void clear() {
    Slot s{};
    ESP.rtcUserMemoryWrite(kRtcDwordOffset,
                           reinterpret_cast<uint32_t*>(&s),
                           sizeof(s));
}

}  // namespace pending_ota
