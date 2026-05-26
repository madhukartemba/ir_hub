#include "App.h"

#include "BootSafety.h"
#include "NeoRing.h"
#include "OtaDownloaderMode.h"
#include "PendingOta.h"
#include "UserPrefs.h"
#include "config.h"
#include "Global/Global.h"
#include "led/ClickSweepOnceAnimation.h"
#include "preferences.h"
#include "secrets.h"
#include "UI/HomeScreen.h"
#include "UI/setup/SetupOnboardingScreen.h"

#ifndef FIRMWARE_VERSION
#    define FIRMWARE_VERSION "0.0.0"
#endif
#ifndef OTA_HW_VARIANT
#    define OTA_HW_VARIANT ""
#endif

namespace app {

namespace {

// 1-hour gap between manifest checks for faster OTA pickup.
constexpr unsigned long kOtaCheckIntervalMs = 1UL * 60UL * 60UL * 1000UL;

// Heap supervisor: logs trend + proactively restarts before fragmentation
// causes a mid-MQTT/OTA crash. ESP8266 has ~30 KB usable heap.
constexpr unsigned long kHeapLogIntervalMs = 60UL * 1000UL;
constexpr uint32_t kHeapFreePanicBytes = 4096;
constexpr uint16_t kHeapBlockPanicBytes = 2048;
constexpr uint8_t kHeapFragPanicPct = 80;

unsigned long lastHeapLog = 0;
bool wasWifiConnected = false;

// --- Wi-Fi outage recovery -------------------------------------------------
//
// Espalexa joins the SSDP multicast group inside Espalexa::begin() and never
// re-joins. After a real Wi-Fi outage (the AP rebooted, DHCP renewed with a
// different IP, mesh roaming hopped APs) the IGMP membership is gone and the
// device becomes silently invisible to Alexa — Espalexa::loop() keeps polling
// a socket that will never receive another M-SEARCH. The library does not
// expose a teardown API, so the only durable fix is to restart the device
// when we detect a meaningful outage. Sub-second blips are tolerated.
constexpr unsigned long kWifiOutageRebootThresholdMs = 5UL * 1000UL;
constexpr unsigned long kPostReconnectRebootDelayMs = 2UL * 1000UL;
unsigned long wifiLostAtMs = 0;
bool wifiRebootPending = false;
unsigned long wifiRebootAtMs = 0;
// Limit our patience for DHCP after the link comes up: if WiFi.localIP()
// hasn't become non-zero within this window we just reboot — same recovery,
// no need to try to differentiate "slow DHCP" from "stuck DHCP".
constexpr unsigned long kPostLinkUpDhcpWaitMs = 10UL * 1000UL;
unsigned long linkUpAtMs = 0;

void performWifiRecoveryReboot() {
    LOG_WARN("[WiFi] Wi-Fi outage exceeded recovery threshold — restarting to "
             "refresh Espalexa multicast socket");
    if (!display.isDisplayOn()) {
        display.turnOn();
    }
    ledRing.solid(COLOR_INFO);
    ledRing.finishTransition();
    display.clear();
    display.printCentered("Wi-Fi recovered", 16);
    display.printCentered("Restarting to", 36);
    display.printCentered("re-link Alexa...", 50);
    display.update();

    // Tidy up the network sockets we own so we don't ship half-baked UDP/TCP
    // frames into the lwIP queues on the way out.
    alexaConnector.shutdown();
    mqttConnector.shutdown();

    delay(250);
    ESP.restart();
}

void superviseHeap() {
    unsigned long now = millis();
    if (now - lastHeapLog < kHeapLogIntervalMs) {
        return;
    }
    lastHeapLog = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint16_t maxBlock = ESP.getMaxFreeBlockSize();
    uint8_t frag = ESP.getHeapFragmentation();
    LOG_INFO("[Heap] free=%u max_block=%u frag=%u%%", (unsigned)freeHeap, (unsigned)maxBlock,
             (unsigned)frag);

    if (freeHeap < kHeapFreePanicBytes || maxBlock < kHeapBlockPanicBytes ||
        frag > kHeapFragPanicPct) {
        LOG_ERROR("[Heap] Below safe limits (free=%u, max_block=%u, frag=%u%%) — restarting",
                  (unsigned)freeHeap, (unsigned)maxBlock, (unsigned)frag);
        delay(50);
        ESP.restart();
    }
}

void configureRuntimeCallbacks() {
    // Device add/remove: both Alexa and MQTT must stay in sync (single callback on DeviceManager)
    deviceManager.setOnDeviceAdded([](const Device& device) {
        LOG_DEBUG("[Hub] Device added: %s (uuid: %s)", device.name.c_str(), device.uuid.c_str());
        alexaConnector.registerDevice(device);
        mqttConnector.registerDevice(device);
    });
    deviceManager.setOnDeviceRemoved([](const Device& device) {
        LOG_DEBUG("[Hub] Device removed: %s (uuid: %s)", device.name.c_str(), device.uuid.c_str());
        alexaConnector.unregisterDevice(device);
        mqttConnector.unregisterDevice(device);
    });

    auto onIrRemoteStateChange = [](const Device& device, bool state) {
        LOG_DEBUG("[Hub] Remote state: %s %s", device.name.c_str(), state ? "ON" : "OFF");
        speaker.beep();
        ledRing.addAnimation(std::make_unique<ClickSweepOnceAnimation>(
            NUM_LEDS, state ? SEND_ON_COMMAND_COLOR : SEND_OFF_COMMAND_COLOR));
    };
    alexaConnector.setOnStateChangeCallback(onIrRemoteStateChange);
    mqttConnector.setOnStateChangeCallback(onIrRemoteStateChange);

    otaUpdater.begin(OTA_MANIFEST_URL, FIRMWARE_VERSION, OTA_HW_VARIANT, kOtaCheckIntervalMs);
    otaUpdater.setOnUpdatePending([](const char* version, const char* /*url*/) {
        // Flash a brief message before the reboot trampoline picks the OTA up
        // in downloader mode. The actual download UI lives in runDownloaderMode().
        if (!display.isDisplayOn()) {
            display.turnOn();
        }
        ledRing.solid(COLOR_INFO);
        ledRing.finishTransition();
        display.clear();
        display.printCentered("Update found", 10);
        char line[24];
        snprintf(line, sizeof(line), "v%s", version);
        display.printCentered(line, 26);
        display.printCentered("Restarting to", 42);
        display.printCentered("install...", 54);
        display.update();
        // Best-effort tidy-up of network sockets before reboot.
        mqttConnector.shutdown();
    });
    mqttConnector.setOnOtaCheckCallback([]() { otaUpdater.checkNow(); });
}

void showReadyScreen(bool wifiConnected) {
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    if (wifiConnected) {
        display.printCentered("Ready!", 40);
        speaker.successBeep();
        ledRing.solid(COLOR_SUCCESS);
    } else {
        display.printCentered("Ready! (Offline)", 40);
        speaker.errorBeep();
        ledRing.solid(COLOR_ERROR);
    }
    ledRing.finishTransition();
    display.update();
    delay(500);
}

void configureRouter() {
    router.setTimeoutDuration(TIMEOUT_DURATION);
    router.enableTimeout(true);
    // Set up activity callback to reset timeout on button interactions
    router.setActivityCallback([]() -> unsigned long { return button.getLastInteractionTime(); });
}

void attachHomeAsDefaultScreen() { router.setDefaultScreen(new HomeScreen()); }

}  // namespace

void setup() {
    // OTA downloader-mode trampoline: if the previous boot's normal-mode
    // firmware armed a pending OTA, hand the entire heap to the TLS download
    // by skipping all non-essential subsystem init. We clear the slot first so
    // a crash during download falls back to normal boot on the next restart
    // (preventing an OTA-induced boot loop).
    pending_ota::Slot pending{};
    bool hasPending = pending_ota::peek(pending);
    if (hasPending) {
        pending_ota::clear();
        ota_downloader::runDownloaderMode(pending);
    }

    Serial.begin(115200);

    // Bump boot-loop counter; cleared at end of setup() when system ready.
    uint16_t bootFailures = boot_safety::registerBootAttempt();
    if (bootFailures > 0) {
        LOG_WARN("[Boot] Recovering from previous failed boot (count=%u)", (unsigned)bootFailures);
    }

    // Initialize display first so subsequent failures can be shown on-screen.
    bool displayReady = display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED);
    boot_safety::setDisplayReady(displayReady);
    if (!displayReady) {
        LOG_ERROR("[Boot] Failed to initialize display");
        boot_safety::criticalFailure("Display", "init failed");
    }

    display.clear();
    if (U8G2* raw = display.getDisplay()) {
        // Startup splash: bold brand title + subtle author credit.
        raw->setFont(u8g2_font_helvB10_tr);
        const char* title = "IR Hub";
        int titleW = raw->getStrWidth(title);
        raw->drawStr((display.getWidth() - titleW) / 2, 28, title);

        raw->setFont(u8g2_font_helvR08_tr);
        const char* credit = "By Madhukar";
        int creditW = raw->getStrWidth(credit);
        raw->drawStr((display.getWidth() - creditW) / 2, 60, credit);
    } else {
        // Defensive fallback; should not happen after successful display.begin().
        display.printCentered("IR Hub", 20);
        display.printCentered("By Madhukar", 44);
    }
    display.update();

    // Probe only; defer calibration until after we know the user's haptics
    // preference (so a muted boot doesn't buzz the LRA).
    if (!haptics.probe()) {
        LOG_WARN("[Haptics] DRV2605 not found — tactile feedback disabled");
    }

    LOG_DEBUG("[Boot] Starting LED ring setup");
    ledRing.begin(NUM_LEDS, NEOPIXEL_PIN, DISPLAY_DRIVER);
    ledRing.solid(Color::RoyalBlue);
    ledRing.finishTransition();
    boot_safety::setLedReady(true);
    LOG_DEBUG("[Boot] LED ring initialized");

    if (!boot_safety::mountLittleFsWithRecovery()) {
        boot_safety::criticalFailure("LittleFS", "mount failed");
    }

    userPrefsLoad();
    if (userPrefsHapticsEnabled() && haptics.isPresent()) {
        if (!haptics.begin()) {
            LOG_WARN("[Haptics] DRV2605 calibration failed — tactile feedback disabled");
        }
    }
    haptics.setMuted(!userPrefsHapticsEnabled());

    if (!irManager.begin(IR_RX_PIN, IR_TX_PIN)) {
        boot_safety::criticalFailure("IR Manager", "check IR pins");
    }
    if (!deviceManager.begin()) {
        boot_safety::criticalFailure("Device Manager", "storage error");
    }

    LOG_DEBUG("[Boot] Starting speaker setup");
    if (!speaker.begin(SPEAKER_PIN)) {
        boot_safety::criticalFailure("Speaker", "check speaker pin");
    }
    speaker.setMuted(!userPrefsSoundEnabled());
    boot_safety::setSpeakerReady(true);
    LOG_DEBUG("[Speaker] Initialized (muted=%s)", speaker.isMuted() ? "yes" : "no");

    LOG_DEBUG("[Boot] Starting button setup");
    if (!button.begin(TOUCH_BUTTON_PIN, INPUT)) {
        boot_safety::criticalFailure("Button", "check button pin");
    }
    button.setSpeaker(speaker);
    button.setHaptics(haptics);
    LOG_DEBUG("[Button] Initialized");

    speaker.playStartupSound();

    configureRouter();
    bool wifiConnected = false;
    bool skipWiFiSetup = userPrefsSkipWiFiSetup();
    if (skipWiFiSetup) {
        wifiManager.skipSetupFlow();
        LOG_INFO("[WiFi] Setup/connect skipped by user preference");
        attachHomeAsDefaultScreen();
    } else {
        wifiConnected = wifiManager.begin(WIFI_AP_NAME, WIFI_AP_TIMEOUT, WIFI_CONNECT_TIMEOUT);
        if (!wifiConnected) {
            router.push(new SetupOnboardingScreen());
            // Set default after onboarding is on stack to avoid flashing HomeScreen first.
            attachHomeAsDefaultScreen();
        } else {
            attachHomeAsDefaultScreen();
            wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);
        }
    }

    alexaConnector.begin();
    mqttConnector.begin();

    configureRuntimeCallbacks();
    if (wifiConnected) {
        showReadyScreen(true);
    } else if (skipWiFiSetup) {
        showReadyScreen(false);
    }

    boot_safety::clearBootFailures();

    LOG_INFO("[Boot] IR Hub: System Ready");
    LOG_INFO("[Heap] startup free=%u max_block=%u frag=%u%%", (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMaxFreeBlockSize(), (unsigned)ESP.getHeapFragmentation());
    lastHeapLog = millis();
    wasWifiConnected = wifiManager.isConnected();
}

void loop() {
    // ledRing is serviced multiple times to absorb the OLED transfer stall.
    // NeoRing's internal 60 fps gate makes extra calls free.
    ledRing.update();
    wifiManager.update();

    bool wifiConnectedNow = wifiManager.isConnected();
    unsigned long nowMs = millis();

    // --- Falling edge: link just went down ---------------------------------
    if (!wifiConnectedNow && wasWifiConnected) {
        wifiLostAtMs = nowMs;
        LOG_WARN("[WiFi] Link dropped");
    }

    // --- Rising edge: link just came back --------------------------------
    if (wifiConnectedNow && !wasWifiConnected) {
        linkUpAtMs = nowMs;
        unsigned long outageMs = (wifiLostAtMs > 0) ? (nowMs - wifiLostAtMs) : 0;
        LOG_INFO("[WiFi] Link is up (outage=%lums)", outageMs);

        // The SDK occasionally resets MODEM_SLEEP back to default across a
        // reconnect, which would start dropping multicast again. Reapply.
        wifiManager.reapplyNoSleep();

        if (outageMs > kWifiOutageRebootThresholdMs && !wifiRebootPending) {
            // Real outage — Espalexa's multicast subscription is almost
            // certainly stale. Schedule a clean reboot in a moment so the
            // user sees a status flash and pending IR/MQTT work can drain.
            wifiRebootPending = true;
            wifiRebootAtMs = nowMs + kPostReconnectRebootDelayMs;
        } else {
            // Brief blip (sub-5s): the lwIP/IGMP state is usually intact, so
            // we just refresh the lazy-started services that were skipped at
            // boot because Wi-Fi wasn't up yet.
            if (!wifiManager.isOtaReady()) {
                wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);
            }
            if (!alexaConnector.isEnabled() &&
                WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
                alexaConnector.begin();
            }
            if (!mqttConnector.isEnabled()) {
                mqttConnector.begin();
            }
        }
    }

    // --- Lazy Alexa start: link is up but begin() was deferred -----------
    //
    // If alexaConnector.begin() was called too early (DHCP hadn't issued an
    // IP yet) it parked itself disabled. Retry once an IP appears. If DHCP
    // never produces one within our patience window, just reboot — same
    // outcome as the long-outage path.
    if (wifiConnectedNow && !alexaConnector.isEnabled() && !wifiRebootPending) {
        if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            alexaConnector.begin();
        } else if (linkUpAtMs > 0 && (nowMs - linkUpAtMs) > kPostLinkUpDhcpWaitMs) {
            LOG_WARN("[WiFi] DHCP did not produce an IP within %lu ms — rebooting",
                     kPostLinkUpDhcpWaitMs);
            wifiRebootPending = true;
            wifiRebootAtMs = nowMs + kPostReconnectRebootDelayMs;
        }
    }

    wasWifiConnected = wifiConnectedNow;

    // --- Execute pending reboot ---------------------------------------
    if (wifiRebootPending && nowMs >= wifiRebootAtMs) {
        performWifiRecoveryReboot();
        return;  // unreachable
    }

    router.update();
    ledRing.update();
    button.update();
    alexaConnector.update();
    mqttConnector.update();
    otaUpdater.update();
    ledRing.update();
    superviseHeap();
}

}  // namespace app
