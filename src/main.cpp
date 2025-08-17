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

    // Disable OTA update options and other advanced features
    wifiManager.setBreakAfterConfig(true);    // Exit after configuration
    wifiManager.setRemoveDuplicateAPs(true);  // Remove duplicate APs
    wifiManager.setMinimumSignalQuality(30);  // Minimum signal quality

    // Customize the portal appearance
    wifiManager.setTitle("IR Hub Wi-Fi Setup");

    // Set custom callbacks for better user experience
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        display.clear();
        display.printCentered("WiFi Setup Mode", 6);
        display.printCentered("Connect to AP", 20);
        display.printCentered(WIFI_AP_NAME, 35);
        display.printCentered("IP: 192.168.4.1", 50);
        display.update();
    });

    wifiManager.setSaveConfigCallback([]() {
        display.clear();
        display.printCentered("WiFi Saved!", 20);
        display.printCentered("Connecting...", 40);
        display.update();
    });

    wifiManager.setCustomHeadElement(
        "<style>"
        "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 "
        "100%);"
        "margin:0;padding:20px;color:white;}"
        ".form-group{background:rgba(255,255,255,0.95);padding:20px;border-radius:10px;"
        "box-shadow:0 8px 32px rgba(0,0,0,0.1);margin-bottom:20px;}"
        "input[type='text'],input[type='password']{width:100%;padding:12px;border:2px solid #ddd;"
        "border-radius:6px;font-size:16px;box-sizing:border-box;transition:border-color 0.3s;"
        "color:#333;}"
        "input[type='text']:focus,input[type='password']:focus{border-color:#667eea;outline:none;}"
        "button{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;"
        "padding:12px 24px;border-radius:6px;font-size:16px;cursor:pointer;transition:transform "
        "0.2s;}"
        "button:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.2);}"
        "h1{text-align:center;color:white;margin-bottom:30px;font-size:28px;text-shadow:0 2px 4px "
        "rgba(0,0,0,0.3);}"
        "label{display:block;margin-bottom:8px;font-weight:bold;color:white;}"
        ".btn-container{text-align:center;margin-top:20px;}"
        ".btn-container button{margin:0 10px;}"
        "</style>");

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
    // Set custom menu items to hide OTA and other advanced options
    std::vector<const char *> menu = {"wifi", "info", "param"};
    wifiManager.setMenu(menu);

    // Add custom parameters for better user experience
    WiFiManagerParameter custom_text(
        "<p style='text-align:center;color:white;font-size:16px;margin:20px 0;'>"
        "Welcome to IR Hub WiFi Setup</p>");
    wifiManager.addParameter(&custom_text);

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
