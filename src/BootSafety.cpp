#include "BootSafety.h"

#include <LittleFS.h>
#include "config.h"
#include "Global/Global.h"
#include "preferences.h"

namespace boot_safety {

namespace {

struct BootGuard {  // RTC-backed; must be uint32-aligned (not packed)
    uint32_t magic;
    uint16_t failures;
    uint16_t reserved;
};

static_assert(sizeof(BootGuard) == 8, "BootGuard layout changed unexpectedly");
static_assert(alignof(BootGuard) >= 4, "BootGuard must be uint32-aligned for RTC API");

constexpr uint32_t kBootGuardMagic = 0xCAFEF00D;
constexpr uint32_t kBootGuardRtcOffset = 64;  // away from OTA region
constexpr uint16_t kBootGuardSoftLimit = 3;
constexpr unsigned long kCritDisplayMs = 5000UL;
constexpr unsigned long kCritPersistentDisplayMs = 30000UL;

bool g_displayReady = false;
bool g_ledReady = false;
bool g_speakerReady = false;

uint16_t bootGuardReadFailures() {
    BootGuard g{};
    if (!ESP.rtcUserMemoryRead(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g))) {
        return 0;
    }
    if (g.magic != kBootGuardMagic) {
        return 0;
    }
    return g.failures;
}

void bootGuardWriteFailures(uint16_t failures) {
    BootGuard g{kBootGuardMagic, failures, 0};
    ESP.rtcUserMemoryWrite(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g));
}

}  // namespace

uint16_t registerBootAttempt() {
    uint16_t failures = bootGuardReadFailures();
    bootGuardWriteFailures(failures + 1);
    return failures;
}

void clearBootFailures() {
    bootGuardWriteFailures(0);
}

void setDisplayReady(bool ready) {
    g_displayReady = ready;
}

void setLedReady(bool ready) {
    g_ledReady = ready;
}

void setSpeakerReady(bool ready) {
    g_speakerReady = ready;
}

[[noreturn]] void criticalFailure(const char* line1, const char* line2) {
    LOG_ERROR("[CRITICAL] %s%s%s", line1 ? line1 : "(unknown)", line2 ? " — " : "",
              line2 ? line2 : "");

    uint16_t failures = bootGuardReadFailures();
    bool persistent = failures >= kBootGuardSoftLimit;

    if (g_ledReady) {
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
    }

    if (g_displayReady) {
        display.clear();
        display.printCentered("ERROR", 6);
        display.drawLine(0, 18, display.getWidth(), 18);
        if (line1) {
            display.printCentered(line1, 26);
        }
        if (line2) {
            display.printCentered(line2, 38);
        }
        if (persistent) {
            display.printCentered("Boot loop detected", 50);
            display.printCentered("Power off & re-flash", 58);
        } else {
            display.printCentered("Restarting...", 54);
        }
        display.update();
    }

    if (g_speakerReady) {
        speaker.errorBeep();
    }

    unsigned long hold = persistent ? kCritPersistentDisplayMs : kCritDisplayMs;
    LOG_ERROR("[CRITICAL] Holding for %lums (failures=%u)%s", hold, (unsigned)failures,
              persistent ? " — boot loop detected" : "");
    delay(hold);

    LOG_ERROR("[CRITICAL] Restarting now");
    delay(50);
    ESP.restart();
    while (true) {
        delay(1000);
    }
}

bool mountLittleFsWithRecovery() {
    if (LittleFS.begin()) {
        return true;
    }

    LOG_WARN("[LittleFS] Mount failed — attempting recovery via format()");
    if (g_displayReady) {
        display.clear();
        display.printCentered("Storage Recovery", 6);
        display.drawLine(0, 18, display.getWidth(), 18);
        display.printCentered("Formatting...", 30);
        display.printCentered("Please wait", 44);
        display.update();
    }
    if (g_ledReady) {
        ledRing.solid(COLOR_WARNING);
        ledRing.finishTransition();
    }

    if (!LittleFS.format()) {
        LOG_ERROR("[LittleFS] Format failed");
        return false;
    }
    if (!LittleFS.begin()) {
        LOG_ERROR("[LittleFS] Mount failed even after format");
        return false;
    }

    LOG_WARN("[LittleFS] Recovered via format (all stored data wiped)");
    return true;
}

}  // namespace boot_safety
