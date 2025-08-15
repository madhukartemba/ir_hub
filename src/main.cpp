#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include "config.h"
#include "global/Global.h"
#include "ui/MainMenu.h"

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

    // Initialize the global router
    router.setDefaultScreen(new MainMenu());  // Replace with your actual default screen object

    // Initialize WiFiManager
    LOG_DEBUG("Starting WiFiManager setup");
    display.clear();
    display.printCentered("IR Hub", 10);
    display.printCentered("WiFi Setup", 25);
    display.printCentered("Connect to IR_Hub_AP", 40);
    display.update();

    // Start WiFi setup animation on LED ring
    ring.setWiFiSetupMode(true);

    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);     // 3 minutes timeout
    wifiManager.setConnectTimeout(30);           // 30 seconds to connect
    wifiManager.setConfigPortalBlocking(false);  // Enable non-blocking mode

    // Customize the portal name
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        LOG_INFO("Entered config mode");
        LOG_INFO("IP: " + WiFi.softAPIP().toString());
        LOG_INFO("SSID: " + myWiFiManager->getConfigPortalSSID());
    });

    // Set callback for when WiFi connects
    wifiManager.setSaveConfigCallback([]() { LOG_INFO("WiFi credentials saved"); });

    // Set callback for when WiFi connects successfully
    wifiManager.setSaveParamsCallback([]() {
        LOG_INFO("WiFi connected successfully");
        LOG_INFO("IP address: " + WiFi.localIP().toString());
        // Turn off WiFi setup animation
        ring.setWiFiSetupMode(false);
    });

    // Start the configuration portal in non-blocking mode
    wifiManager.startConfigPortal("IR_Hub_AP");
    LOG_INFO("WiFi config portal started");

    // Show ready message on display
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Ready!", 40);
    display.update();
    delay(500);

    LOG_INFO("IR Hub: System Ready");
}

void loop() {
    // Handle WiFiManager in non-blocking mode
    static WiFiManager wifiManager;
    static bool wifiConfigured = false;

    if (!wifiConfigured) {
        wifiManager.process();
        // Only check WiFi status occasionally, not every loop
        static unsigned long lastWiFiCheck = 0;
        if (millis() - lastWiFiCheck > 1000) {  // Check every 1 second
            if (WiFi.status() == WL_CONNECTED) {
                wifiConfigured = true;
                wifiManager.stopConfigPortal();
            }
            lastWiFiCheck = millis();
        }
    }

    router.update();  // Main app logic now handled by router
    button.update();
    ring.update();  // Now you can call ring.update() during WiFi setup!
}
