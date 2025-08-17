#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "global/Global.h"
#include "ui/HomeScreen.h"

void setup() {
    Serial.begin(9600);

    // Initialize display first so we can show error messages
    if (!display.begin(OLED_SDA_PIN, OLED_SCL_PIN)) {
        LOG_ERROR("Failed to initialize display");
        while (1) {
            delay(100);
        }
    }

    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Initializing...", 40);
    display.update();

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
    LOG_DEBUG("Button initialized on pin");

    // Initialize LED ring
    LOG_DEBUG("Starting LED ring setup");
    if (!ring.begin(NEOPIXEL_PIN, NUM_LEDS, CENTER_LED)) {
        LOG_ERROR("Failed to initialize LED ring");
        display.clear();
        display.printCentered("ERROR", 10);
        display.printCentered("LED Ring failed", 25);
        display.printCentered("Check LED pin", 40);
        display.update();
        while (1) {
            delay(100);
        }
    }
    LOG_DEBUG("LED ring initialized on pin");

    // Initialize WiFi Manager
    bool wifiConnected = wifiManager.begin(WIFI_AP_NAME, WIFI_AP_TIMEOUT, WIFI_CONNECT_TIMEOUT);

    // Initialize AlexaConnector (will handle WiFi status internally)
    alexaConnector.begin();

    // Initialize the global router
    router.setDefaultScreen(new HomeScreen());  // Status screen is now the default

    // Show ready message on display
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    if (wifiConnected) {
        display.printCentered("Ready!", 40);
    } else {
        display.printCentered("Ready! (Offline)", 40);
    }
    display.update();
    delay(500);

    // Play startup sound
    speaker.playStartupSound();
    LOG_DEBUG("Startup sound played");

    LOG_INFO("IR Hub: System Ready");
}

void loop() {
    wifiManager.update();  // Handle WiFi and OTA updates
    router.update();       // Main app logic now handled by router
    button.update();
    ring.update();
    alexaConnector.update();
}
