#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "NeoRing.h"
#include "UserPrefs.h"
#include "config.h"
#include "global/Global.h"
#include "led/ClickSweepOnceAnimation.h"
#include "preferences.h"
#include "secrets.h"
#include "ui/HomeScreen.h"

#ifndef FIRMWARE_VERSION
#  define FIRMWARE_VERSION "0.0.0"
#endif
#ifndef OTA_HW_VARIANT
#  define OTA_HW_VARIANT ""
#endif

// 6-hour gap between manifest checks. Long enough to be polite to GitHub's
// edge cache, short enough that a critical patch reaches the friend overnight.
static constexpr unsigned long kOtaCheckIntervalMs = 6UL * 60UL * 60UL * 1000UL;

unsigned long lastAnimSwitch = 0;
const unsigned long animSwitchInterval = 5000;  // 5 seconds

int currentAnim = 0;  // index to track which animation is active

// Critical-failure handling: graceful recovery + boot-loop guard so init
// failures don't brick the device until a reflash.

static bool g_displayReady = false;
static bool g_ledReady = false;
static bool g_speakerReady = false;

// RTC user memory survives soft restarts but not power loss.
// Must NOT be packed: rtcUserMemoryRead/Write needs 4-byte alignment.
struct BootGuard {
    uint32_t magic;
    uint16_t failures;
    uint16_t reserved;
};
static_assert(sizeof(BootGuard) == 8, "BootGuard layout changed unexpectedly");
static_assert(alignof(BootGuard) >= 4, "BootGuard must be uint32-aligned for RTC API");
static constexpr uint32_t kBootGuardMagic = 0xCAFEF00D;
static constexpr uint32_t kBootGuardRtcOffset = 64;  // away from OTA region
static constexpr uint16_t kBootGuardSoftLimit = 3;
static constexpr unsigned long kCritDisplayMs = 5000UL;
static constexpr unsigned long kCritPersistentDisplayMs = 30000UL;

static uint16_t bootGuardReadFailures() {
    BootGuard g{};
    if (!ESP.rtcUserMemoryRead(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g))) {
        return 0;
    }
    if (g.magic != kBootGuardMagic) {
        return 0;
    }
    return g.failures;
}

static void bootGuardWriteFailures(uint16_t failures) {
    BootGuard g{kBootGuardMagic, failures, 0};
    ESP.rtcUserMemoryWrite(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g));
}

[[noreturn]] static void criticalFailure(const char* line1, const char* line2 = nullptr) {
    LOG_ERROR("[CRITICAL] %s%s%s", line1 ? line1 : "(unknown)", line2 ? " — " : "",
              line2 ? line2 : "");

    // setup() bumped this counter on entry; we never decremented because we're failing here.
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
    while (true) {  // unreachable; ESP.restart() isn't marked noreturn
        delay(1000);
    }
}

/// Mount LittleFS with one-shot format-and-retry recovery.
static bool mountLittleFsWithRecovery() {
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

// Heap supervisor: logs trend + proactively restarts before fragmentation
// causes a mid-MQTT/OTA crash. ESP8266 has ~30 KB usable heap.
static constexpr unsigned long kHeapLogIntervalMs = 60UL * 1000UL;
static constexpr uint32_t kHeapFreePanicBytes = 4096;
static constexpr uint16_t kHeapBlockPanicBytes = 2048;
static constexpr uint8_t kHeapFragPanicPct = 80;
static unsigned long lastHeapLog = 0;

static void superviseHeap() {
    unsigned long now = millis();
    if (now - lastHeapLog < kHeapLogIntervalMs) {
        return;
    }
    lastHeapLog = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint16_t maxBlock = ESP.getMaxFreeBlockSize();
    uint8_t frag = ESP.getHeapFragmentation();
    LOG_INFO("[Heap] free=%u max_block=%u frag=%u%%", (unsigned)freeHeap,
             (unsigned)maxBlock, (unsigned)frag);

    if (freeHeap < kHeapFreePanicBytes || maxBlock < kHeapBlockPanicBytes ||
        frag > kHeapFragPanicPct) {
        LOG_ERROR("[Heap] Below safe limits (free=%u, max_block=%u, frag=%u%%) — restarting",
                  (unsigned)freeHeap, (unsigned)maxBlock, (unsigned)frag);
        delay(50);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);

    // Bump boot-loop counter; cleared at end of setup() when system ready.
    uint16_t bootFailures = bootGuardReadFailures();
    bootGuardWriteFailures(bootFailures + 1);
    if (bootFailures > 0) {
        LOG_WARN("[Boot] Recovering from previous failed boot (count=%u)",
                 (unsigned)bootFailures);
    }

    // Initialize display first so subsequent failures can be shown on-screen.
    g_displayReady = display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED);
    if (!g_displayReady) {
        LOG_ERROR("[Boot] Failed to initialize display");
        criticalFailure("Display", "init failed");
    }

    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Initializing...", 40);
    display.update();

    // Probe only; defer calibration until after we know the user's haptics
    // preference (so a muted boot doesn't buzz the LRA).
    if (!haptics.probe()) {
        LOG_WARN("[Haptics] DRV2605 not found — tactile feedback disabled");
    }

    // Initialize NeoRing
    LOG_DEBUG("[Boot] Starting LED ring setup");
    ledRing.begin(NUM_LEDS, NEOPIXEL_PIN, DISPLAY_DRIVER);
    ledRing.solid(Color::RoyalBlue);
    ledRing.finishTransition();
    g_ledReady = true;
    LOG_DEBUG("[Boot] LED ring initialized");

    // Initialize LittleFS, with a one-shot format-and-retry recovery.
    if (!mountLittleFsWithRecovery()) {
        criticalFailure("LittleFS", "mount failed");
    }

    // Load prefs before any subsystem that reacts to them.
    userPrefsLoad();
    if (userPrefsHapticsEnabled() && haptics.isPresent()) {
        if (!haptics.begin()) {
            LOG_WARN("[Haptics] DRV2605 calibration failed — tactile feedback disabled");
        }
    }
    haptics.setMuted(!userPrefsHapticsEnabled());

    // Initialize IdGen
    if (!idGen.begin()) {
        criticalFailure("IdGen", "init failed");
    }

    // Initialize IRManager
    if (!irManager.begin(IR_RX_PIN, IR_TX_PIN)) {
        criticalFailure("IR Manager", "check IR pins");
    }

    // Initialize DeviceManager
    if (!deviceManager.begin()) {
        criticalFailure("Device Manager", "storage error");
    }

    // Initialize speaker
    LOG_DEBUG("[Boot] Starting speaker setup");
    if (!speaker.begin(SPEAKER_PIN)) {
        criticalFailure("Speaker", "check speaker pin");
    }
    speaker.setMuted(!userPrefsSoundEnabled());
    g_speakerReady = true;
    LOG_DEBUG("[Speaker] Initialized (muted=%s)", speaker.isMuted() ? "yes" : "no");

    // Initialize button
    LOG_DEBUG("[Boot] Starting button setup");
    if (!button.begin(TOUCH_BUTTON_PIN, INPUT)) {
        criticalFailure("Button", "check button pin");
    }
    button.setSpeaker(speaker);
    button.setHaptics(haptics);
    LOG_DEBUG("[Button] Initialized");

    // Successful core initialization now setup wireless connection
    speaker.playStartupSound();

    // Initialize WiFi Manager
    bool wifiConnected = wifiManager.begin(WIFI_AP_NAME, WIFI_AP_TIMEOUT, WIFI_CONNECT_TIMEOUT);
    wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);

    // Device add/remove: both Alexa and MQTT must stay in sync (single callback on DeviceManager)
    deviceManager.setOnDeviceAdded([](const Device& device) {
        LOG_DEBUG("[Hub] Device added: %s (ID: %d)", device.name.c_str(), device.id);
        alexaConnector.registerDevice(device);
        mqttConnector.registerDevice(device);
    });
    deviceManager.setOnDeviceRemoved([](const Device& device) {
        LOG_DEBUG("[Hub] Device removed: %s (ID: %d)", device.name.c_str(), device.id);
        alexaConnector.unregisterDevice(device);
        mqttConnector.unregisterDevice(device);
    });

    // Initialize AlexaConnector (will handle WiFi status internally)
    alexaConnector.begin();
    // Initialize MQTT (Home Assistant discovery + commands)
    mqttConnector.begin();

    auto onIrRemoteStateChange = [](const Device& device, bool state) {
        LOG_DEBUG("[Hub] Remote state: %s %s", device.name.c_str(), state ? "ON" : "OFF");
        if (state) {
            speaker.beep();
            ledRing.addAnimation(
                std::make_unique<ClickSweepOnceAnimation>(NUM_LEDS, SEND_ON_COMMAND_COLOR));
        } else {
            speaker.beep();
            ledRing.addAnimation(
                std::make_unique<ClickSweepOnceAnimation>(NUM_LEDS, SEND_OFF_COMMAND_COLOR));
        }
    };
    alexaConnector.setOnStateChangeCallback(onIrRemoteStateChange);
    mqttConnector.setOnStateChangeCallback(onIrRemoteStateChange);

    otaUpdater.begin(OTA_MANIFEST_URL, FIRMWARE_VERSION, OTA_HW_VARIANT, kOtaCheckIntervalMs);
    otaUpdater.setOnUpdateStart([]() {
        LOG_INFO("[OTA-HTTP] Starting download — disconnecting MQTT to free heap");
        mqttConnector.shutdown();
        if (display.isDisplayOn() == false) display.turnOn();
        display.clear();
        display.printCentered("OTA Update", 10);
        display.printCentered("Downloading...", 30);
        display.update();
        ledRing.spinner(COLOR_INFO);
    });
    otaUpdater.setOnUpdateProgress([](unsigned cur, unsigned total) {
        if (!display.isDisplayOn()) display.turnOn();
        ledRing.update();
        display.clear();
        display.printCentered("OTA Update", 10);
        display.drawProgressBar(10, 32, 108, 12, cur, total, true);
        display.update();
    });
    otaUpdater.setOnUpdateError([](const char* msg) {
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
        display.clear();
        display.printCentered("OTA Failed", 10);
        if (msg && *msg) display.printCentered(msg, 30);
        display.printCentered("Will retry later", 50);
        display.update();
        delay(3000);
    });
    mqttConnector.setOnOtaCheckCallback([]() { otaUpdater.checkNow(); });

    // Show ready message on display
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    if (wifiConnected) {
        display.printCentered("Ready!", 40);
        speaker.successBeep();
        ledRing.solid(COLOR_SUCCESS);
        ledRing.finishTransition();
    } else {
        display.printCentered("Ready! (Offline)", 40);
        speaker.errorBeep();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
    }
    display.update();
    delay(500);

    // Initialize the global router
    router.setDefaultScreen(new HomeScreen());  // Status screen is now the default
    router.setTimeoutDuration(TIMEOUT_DURATION);
    router.enableTimeout(true);

    // Set up activity callback to reset timeout on button interactions
    router.setActivityCallback([]() -> unsigned long { return button.getLastInteractionTime(); });

    bootGuardWriteFailures(0);  // system ready, clear boot-loop counter

    LOG_INFO("[Boot] IR Hub: System Ready");
    LOG_INFO("[Heap] startup free=%u max_block=%u frag=%u%%",
             (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize(),
             (unsigned)ESP.getHeapFragmentation());
    lastHeapLog = millis();
}

void loop() {
    // ledRing is serviced multiple times to absorb the OLED transfer stall.
    // NeoRing's internal 60 fps gate makes extra calls free.
    ledRing.update();
    wifiManager.update();
    router.update();
    ledRing.update();
    button.update();
    alexaConnector.update();
    mqttConnector.update();
    otaUpdater.update();
    ledRing.update();
    superviseHeap();
}
