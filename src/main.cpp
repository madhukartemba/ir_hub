#include <Arduino.h>
#include <LittleFS.h>
#include "NeoRing.h"
#include "config.h"
#include "global/Global.h"
#include "led/ClickSweepOnceAnimation.h"
#include "preferences.h"
#include "ui/HomeScreen.h"

unsigned long lastAnimSwitch = 0;
const unsigned long animSwitchInterval = 5000;  // 5 seconds

int currentAnim = 0;  // index to track which animation is active

// ---------------------------------------------------------------------------
// Critical-failure handling
//
// Previously every peripheral init failure trapped the device in
// `while(1) delay(100);`. If anything went wrong in the field (e.g. LittleFS
// corruption after a power glitch) the device became a paperweight that only
// a reflash could recover. We now:
//
//   * try a one-shot recovery for LittleFS via `format()`,
//   * show the error on the OLED (+ red LED ring + error beep when those
//     subsystems are up), hold for a few seconds so the user/tester can read
//     it, then `ESP.restart()`,
//   * track consecutive failed boots in RTC user memory so a genuinely
//     broken board doesn't spin in a tight restart loop — after a few
//     consecutive failures we hold the error on screen for much longer
//     before retrying, giving the user time to power off and contact us.
// ---------------------------------------------------------------------------

static bool g_displayReady = false;
static bool g_ledReady = false;
static bool g_speakerReady = false;

// RTC user memory survives soft restarts (but not full power loss), which is
// exactly the semantics we want: a board cycling on a broken peripheral keeps
// the counter; a user unplugging-and-replugging starts fresh.
struct __attribute__((packed)) BootGuard {
    uint32_t magic;
    uint16_t failures;
    uint16_t reserved;
};
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

    // The first thing setup() did was optimistically bump this counter. We
    // never decremented it because we are failing, so the persistence check
    // here sees the now-incremented value.
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

    // delay() yields to background tasks and feeds the watchdog, so a multi-
    // second hold here is safe.
    delay(hold);

    LOG_ERROR("[CRITICAL] Restarting now");
    delay(50);  // give Serial a moment to flush
    ESP.restart();
    // Unreachable, but ESP.restart() is not marked noreturn in the headers.
    while (true) {
        delay(1000);
    }
}

/// Mount LittleFS, attempting a one-shot format-and-retry recovery if the
/// initial mount fails. Returns true on success.
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

// Heap supervision -----------------------------------------------------------
//
// On ESP8266 we have ~30 KB usable heap. Long-running firmware tends to slowly
// fragment that heap, so even when "free heap" looks healthy the largest
// contiguous block shrinks and eventually an allocation (MQTT, OTA, WiFi)
// fails and we crash. We:
//   * log free heap + fragmentation + largest block once a minute, so the
//     trend is visible over a multi-day run.
//   * proactively restart if we drop below safe thresholds. A clean restart
//     is much less disruptive than a heap-exhaustion crash mid-OTA or
//     mid-MQTT publish, and HA reconnects within seconds.
static constexpr unsigned long kHeapLogIntervalMs = 60UL * 1000UL;     // 1 min
static constexpr uint32_t kHeapFreePanicBytes = 4096;                  // 4 KB
static constexpr uint16_t kHeapBlockPanicBytes = 2048;                 // 2 KB
static constexpr uint8_t kHeapFragPanicPct = 80;                        // %
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
        // Best-effort: give Serial a moment to flush the log line.
        delay(50);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);

    // Optimistically bump the boot-loop counter on entry. The end of setup()
    // clears it when we reach "system ready"; until then any criticalFailure
    // will see the elevated count and (after a few cycles) hold the error on
    // screen for longer instead of restarting immediately.
    uint16_t bootFailures = bootGuardReadFailures();
    bootGuardWriteFailures(bootFailures + 1);
    if (bootFailures > 0) {
        LOG_WARN("[Boot] Recovering from previous failed boot (count=%u)",
                 (unsigned)bootFailures);
    }

    // Initialize display first so subsequent failures can be shown on-screen.
    g_displayReady = display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED);
    if (!g_displayReady) {
        // We can't show anything visual. Log + short delay + restart so the
        // device doesn't sit silent forever.
        LOG_ERROR("Failed to initialize display");
        criticalFailure("Display", "init failed");
    }

    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Initializing...", 40);
    display.update();

    if (!haptics.begin()) {
        LOG_WARN("DRV2605 haptics not found — tactile feedback disabled");
    }

    // Initialize NeoRing
    LOG_DEBUG("Starting LED ring setup");
    ledRing.begin(NUM_LEDS, NEOPIXEL_PIN, DISPLAY_DRIVER);
    ledRing.solid(Color::RoyalBlue);
    ledRing.finishTransition();
    g_ledReady = true;
    LOG_DEBUG("LED ring initialized on pin");

    // Initialize LittleFS, with a one-shot format-and-retry recovery.
    if (!mountLittleFsWithRecovery()) {
        criticalFailure("LittleFS", "mount failed");
    }

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
    LOG_DEBUG("Starting speaker setup");
    if (!speaker.begin(SPEAKER_PIN)) {
        criticalFailure("Speaker", "check speaker pin");
    }
    g_speakerReady = true;
    LOG_DEBUG("Speaker initialized");

    // Initialize button
    LOG_DEBUG("Starting button setup");
    if (!button.begin(TOUCH_BUTTON_PIN, INPUT)) {
        criticalFailure("Button", "check button pin");
    }
    button.setSpeaker(speaker);
    button.setHaptics(haptics);
    LOG_DEBUG("Button initialized on pin");

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
        LOG_DEBUG("Remote state: %s %s", device.name.c_str(), state ? "ON" : "OFF");
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

    // We made it all the way through setup() — clear the boot-loop counter
    // so the next failure (if any) gets the full kCritDisplayMs grace period.
    bootGuardWriteFailures(0);

    LOG_INFO("IR Hub: System Ready");
    LOG_INFO("[Heap] startup free=%u max_block=%u frag=%u%%",
             (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize(),
             (unsigned)ESP.getHeapFragmentation());
    lastHeapLog = millis();
}

void loop() {
    wifiManager.update();
    router.update();
    button.update();
    alexaConnector.update();
    mqttConnector.update();
    ledRing.update();
    superviseHeap();
}
