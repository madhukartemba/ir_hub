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

constexpr unsigned long kOtaCheckIntervalMs = 1UL * 60UL * 60UL * 1000UL;

constexpr unsigned long kHeapLogIntervalMs = 60UL * 1000UL;
constexpr uint32_t kHeapFreePanicBytes = 4096;
constexpr uint16_t kHeapBlockPanicBytes = 2048;
constexpr uint8_t kHeapFragPanicPct = 80;

unsigned long lastHeapLog = 0;
bool wasWifiConnected = false;

constexpr unsigned long kWifiOutageRebootThresholdMs = 5UL * 1000UL;
constexpr unsigned long kPostReconnectRebootDelayMs = 2UL * 1000UL;
unsigned long wifiLostAtMs = 0;
bool wifiRebootPending = false;
unsigned long wifiRebootAtMs = 0;
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
        mqttConnector.syncDeviceState(device.uuid, state);
        alexaConnector.syncDeviceState(device.uuid, state);
        speaker.beep();
        ledRing.addAnimation(std::make_unique<ClickSweepOnceAnimation>(
            NUM_LEDS, state ? SEND_ON_COMMAND_COLOR : SEND_OFF_COMMAND_COLOR));
    };
    alexaConnector.setOnStateChangeCallback(onIrRemoteStateChange);
    mqttConnector.setOnStateChangeCallback(onIrRemoteStateChange);

    otaUpdater.begin(OTA_MANIFEST_URL, FIRMWARE_VERSION, OTA_HW_VARIANT, kOtaCheckIntervalMs);
    otaUpdater.setOnUpdatePending([](const char* version, const char* /*url*/) {
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
        mqttConnector.shutdown();
    });
    mqttConnector.setOnOtaCheckCallback([]() { otaUpdater.checkNow(); });
}

void showReadyScreen(bool wifiConnected) {
    delay(1000);
    display.drawBrandStatus(wifiConnected ? "Ready!" : "Ready! (Offline)");
    if (wifiConnected) {
        speaker.successBeep();
        ledRing.solid(COLOR_SUCCESS);
    } else {
        speaker.errorBeep();
        ledRing.solid(COLOR_ERROR);
    }
    ledRing.finishTransition();
    display.update();
    delay(500);

    if (wifiManager.isConnected()) {
        ledRing.wave(COLOR_HOME_SCREEN_WIFI_CONNECTED);
    } else {
        ledRing.wave(COLOR_HOME_SCREEN_WIFI_DISCONNECTED);
    }
}

void configureRouter() {
    router.setTimeoutDuration(TIMEOUT_DURATION);
    router.enableTimeout(true);
    router.setActivityCallback([]() -> unsigned long { return button.getLastInteractionTime(); });
}

void attachHomeAsDefaultScreen() { router.setDefaultScreen(new HomeScreen()); }

}  // namespace

void setup() {
    pending_ota::Slot pending{};
    bool hasPending = pending_ota::peek(pending);
    if (hasPending) {
        pending_ota::clear();
        ota_downloader::runDownloaderMode(pending);
    }

    Serial.begin(115200);

    uint16_t bootFailures = boot_safety::registerBootAttempt();
    if (bootFailures > 0) {
        LOG_WARN("[Boot] Recovering from previous failed boot (count=%u)", (unsigned)bootFailures);
    }

    bool displayReady = display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED);
    boot_safety::setDisplayReady(displayReady);
    if (!displayReady) {
        LOG_ERROR("[Boot] Failed to initialize display");
        boot_safety::criticalFailure("Display", "init failed");
    }

    display.drawBrandSplash();
    display.update();

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
    bool hasSavedWiFiCredentials = false;
    if (skipWiFiSetup) {
        wifiManager.skipSetupFlow();
        LOG_INFO("[WiFi] Setup/connect skipped by user preference");
        attachHomeAsDefaultScreen();
    } else {
        wifiConnected = wifiManager.begin(WIFI_AP_NAME, WIFI_AP_TIMEOUT);
        hasSavedWiFiCredentials = wifiManager.hasSavedWiFiCredentials();
        if (!wifiConnected) {
            if (!hasSavedWiFiCredentials) {
                router.push(new SetupOnboardingScreen());
            } else {
                LOG_INFO("[WiFi] Saved credentials found but network unavailable; booting offline");
            }
            attachHomeAsDefaultScreen();
        } else {
            attachHomeAsDefaultScreen();
            wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);
        }
    }

    if (userPrefsAlexaEnabled()) {
        alexaConnector.begin();
    } else {
        LOG_INFO("[Alexa] Disabled in settings; skipping startup");
    }
    mqttConnector.begin();

    configureRuntimeCallbacks();
    if (wifiConnected) {
        showReadyScreen(true);
    } else if (skipWiFiSetup || hasSavedWiFiCredentials) {
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
    ledRing.update();
    wifiManager.update();

    bool wifiConnectedNow = wifiManager.isConnected();
    unsigned long nowMs = millis();

    if (!wifiConnectedNow && wasWifiConnected) {
        wifiLostAtMs = nowMs;
        LOG_WARN("[WiFi] Link dropped");
    }

    if (wifiConnectedNow && !wasWifiConnected) {
        linkUpAtMs = nowMs;
        unsigned long outageMs = (wifiLostAtMs > 0) ? (nowMs - wifiLostAtMs) : 0;
        LOG_INFO("[WiFi] Link is up (outage=%lums)", outageMs);

        wifiManager.reapplyNoSleep();

        if (outageMs > kWifiOutageRebootThresholdMs && !wifiRebootPending) {
            wifiRebootPending = true;
            wifiRebootAtMs = nowMs + kPostReconnectRebootDelayMs;
        } else {
            if (!wifiManager.isOtaReady()) {
                wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);
            }
            if (userPrefsAlexaEnabled() && !alexaConnector.isEnabled() &&
                WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
                alexaConnector.begin();
            }
            if (!mqttConnector.isEnabled()) {
                mqttConnector.begin();
            }
        }
    }

    if (userPrefsAlexaEnabled() && wifiConnectedNow && !alexaConnector.isEnabled() &&
        !wifiRebootPending) {
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

    if (wifiRebootPending && nowMs >= wifiRebootAtMs) {
        performWifiRecoveryReboot();
        return;
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
