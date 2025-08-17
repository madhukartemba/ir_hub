#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <WiFiManager.h>
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

    // WiFiManager setup
    WiFiManager wifiManager;

    // Configure timeout (in seconds)
    wifiManager.setConfigPortalTimeout(WIFI_AP_TIMEOUT);
    wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT);

    // Check if WiFi credentials exist using WiFiManager
    bool hasCredentials = wifiManager.getWiFiIsSaved();

    if (hasCredentials) {
        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("Connecting...", 40);
        display.update();

        LOG_INFO("Found saved WiFi credentials, attempting connection...");
        LOG_DEBUG("Saved SSID: %s", WiFi.SSID().c_str());
    } else {
        display.clear();
        display.printCentered("IR Hub", 15);
        display.printCentered("Connect to AP", 30);
        display.printCentered(WIFI_AP_NAME, 45);
        display.update();

        LOG_INFO("No saved WiFi credentials found");
        LOG_DEBUG("Will start AP with name: %s", WIFI_AP_NAME);
    }

    LOG_INFO("Starting WiFi connection attempt...");
    LOG_DEBUG("WiFi timeout set to 60 seconds");

    // Try to connect to WiFi, but don't restart if it fails
    bool wifiConnected = wifiManager.autoConnect(WIFI_AP_NAME);

    if (wifiConnected) {
        LOG_INFO("WiFi connection successful!");
        LOG_DEBUG("IP Address: %s", WiFi.localIP().toString().c_str());
        LOG_DEBUG("SSID: %s", WiFi.SSID().c_str());
        LOG_DEBUG("RSSI: %d dBm", WiFi.RSSI());

        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("WiFi Connected!", 40);
        display.update();
        delay(1000);

        // Setup OTA
        LOG_DEBUG("Setting up OTA...");
        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("Setting up OTA...", 40);
        display.update();

        ArduinoOTA.setHostname("ir-hub");

        ArduinoOTA.onStart([]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            LOG_INFO("Start updating " + type);
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            display.clear();
            display.printCentered("OTA Update", 10);
            display.printCentered("Starting...", 30);
            display.update();
        });

        ArduinoOTA.onEnd([]() {
            LOG_INFO("OTA Update Complete");
            display.clear();
            display.printCentered("OTA Complete", 20);
            display.printCentered("Restarting...", 40);
            display.update();
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            display.clear();
            display.printCentered("OTA Update", 10);

            display.drawProgressBar(10, 32, 108, 12, progress, total, true);

            display.update();
        });

        ArduinoOTA.onError([](ota_error_t error) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            LOG_ERROR("OTA Error: " + String(error));
            display.clear();
            display.printCentered("OTA Error", 10);
            display.printCentered("Error: " + String(error), 30);
            display.update();
        });

        ArduinoOTA.begin();
        LOG_INFO("OTA Ready");

        // Display OTA info
        display.clear();
        display.printCentered("IR Hub", 10);
        display.printCentered("IP: " + WiFi.localIP().toString(), 25);
        display.printCentered("OTA: Ready", 40);
        display.update();
        delay(2000);
    } else {
        LOG_WARN("WiFi connection failed - continuing in offline mode");
        LOG_DEBUG("WiFi status: %d", WiFi.status());
        LOG_DEBUG("No saved credentials or connection timeout");

        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("WiFi Failed", 30);
        display.printCentered("Offline Mode", 40);
        display.update();
        delay(2000);
    }

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
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();  // Handle OTA updates only if WiFi is connected
    }
    router.update();  // Main app logic now handled by router
    button.update();
    ring.update();
    alexaConnector.update();
}
