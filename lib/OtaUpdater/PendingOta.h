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

// Bump this when the Slot layout changes so old slots from a previous
// firmware version are rejected on read (rather than mis-parsed).
constexpr uint32_t kMagic = 0x07A0FAD2;
// BootGuard occupies dwords 64-65 (see main.cpp). Sit at 66 onward.
constexpr uint32_t kRtcDwordOffset = 66;
constexpr size_t kUrlMax = 160;
constexpr size_t kVersionMax = 16;
constexpr size_t kMd5HexMax = 36;  // 32 hex chars + nul + slop

struct Slot {
    uint32_t magic;
    uint32_t expected_size;     // bytes; required so we don't have to trust Content-Length
    char url[kUrlMax];
    char version[kVersionMax];
    char md5_hex[kMd5HexMax];   // empty string = no integrity check
    uint32_t checksum;
};
static_assert(sizeof(Slot) % 4 == 0, "Slot must be a multiple of 4 bytes for RTC API");
static_assert(alignof(Slot) >= 4, "Slot must be uint32-aligned");

inline uint32_t computeChecksum(const Slot& s) {
    uint32_t c = s.magic;
    c = c * 31u + s.expected_size;
    for (size_t i = 0; i < kUrlMax; i++) c = c * 31u + (uint8_t)s.url[i];
    for (size_t i = 0; i < kVersionMax; i++) c = c * 31u + (uint8_t)s.version[i];
    for (size_t i = 0; i < kMd5HexMax; i++) c = c * 31u + (uint8_t)s.md5_hex[i];
    return c;
}

/// Returns false if URL/version/md5 is too long, or if the write itself fails.
/// `md5_hex` may be nullptr or empty to skip integrity verification.
inline bool arm(const char* url, const char* version, uint32_t expected_size,
                const char* md5_hex = nullptr) {
    if (!url || !version) return false;
    if (strlen(url) >= kUrlMax) return false;
    if (strlen(version) >= kVersionMax) return false;
    if (md5_hex && strlen(md5_hex) >= kMd5HexMax) return false;
    Slot s{};
    s.magic = kMagic;
    s.expected_size = expected_size;
    strncpy(s.url, url, kUrlMax - 1);
    strncpy(s.version, version, kVersionMax - 1);
    if (md5_hex && *md5_hex) {
        strncpy(s.md5_hex, md5_hex, kMd5HexMax - 1);
    }
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
    out.md5_hex[kMd5HexMax - 1] = '\0';
    return true;
}

inline void clear() {
    Slot s{};
    ESP.rtcUserMemoryWrite(kRtcDwordOffset,
                           reinterpret_cast<uint32_t*>(&s),
                           sizeof(s));
}

}  // namespace pending_ota
