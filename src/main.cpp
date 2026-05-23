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

    // Initialize display first so we can show error messages
    if (!display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED)) {
        LOG_ERROR("Failed to initialize display");
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
    LOG_DEBUG("LED ring initialized on pin");

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        LOG_ERROR("Failed to mount LittleFS");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("LittleFS failed", 25);
        display.printCentered("Check wiring", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }

    // Initialize IdGen
    if (!idGen.begin()) {
        LOG_ERROR("Failed to initialize IdGen");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("IdGen failed", 25);
        display.printCentered("Check storage", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }

    // Initialize IRManager
    if (!irManager.begin(IR_RX_PIN, IR_TX_PIN)) {
        LOG_ERROR("Failed to initialize IRManager");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("IR Manager failed", 25);
        display.printCentered("Check IR pins", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }

    // Initialize DeviceManager
    if (!deviceManager.begin()) {
        LOG_ERROR("Failed to initialize DeviceManager");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("Device Manager failed", 25);
        display.printCentered("Check storage", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }

    // Initialize speaker
    LOG_DEBUG("Starting speaker setup");
    if (!speaker.begin(SPEAKER_PIN)) {
        LOG_ERROR("Failed to initialize speaker");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("Speaker failed", 25);
        display.printCentered("Check speaker pin", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }
    LOG_DEBUG("Speaker initialized");

    // Initialize button
    LOG_DEBUG("Starting button setup");
    if (!button.begin(TOUCH_BUTTON_PIN, INPUT)) {
        LOG_ERROR("Failed to initialize button");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("Button failed", 25);
        display.printCentered("Check button pin", 40);
        display.update();
        while (1) {
            delay(100);
        }
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
