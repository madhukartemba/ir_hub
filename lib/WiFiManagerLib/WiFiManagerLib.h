#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "Display.h"
#include "Log.h"
#include "Speaker.h"

class WiFiManagerLib {
   private:
    WiFiManager wifiManager;
    Display& display;
    LedRing& ring;
    Speaker& speaker;
    bool wifiConnected;
    bool apTimeout;

    void setupWiFiManager(const char* apName, int apTimeout, int connectTimeout) {
        this->apTimeout = apTimeout;

        // Configure timeout (in seconds)
        wifiManager.setConfigPortalTimeout(apTimeout);
        wifiManager.setConnectTimeout(connectTimeout);

        // Non blocking connection
        wifiManager.setConfigPortalBlocking(false);

        // Disable OTA update options and other advanced features
        wifiManager.setBreakAfterConfig(true);    // Exit after configuration
        wifiManager.setRemoveDuplicateAPs(true);  // Remove duplicate APs
        wifiManager.setMinimumSignalQuality(30);  // Minimum signal quality

        // Customize the portal appearance
        wifiManager.setTitle("IR Hub Wi-Fi Setup");

        // Set custom callbacks for better user experience
        wifiManager.setAPCallback([this, apName](WiFiManager* myWiFiManager) {
            display.clear();
            display.printCentered("WiFi Setup Mode", 6);
            display.printCentered("Connect to AP", 20);
            display.printCentered(apName, 35);
            display.printCentered("IP: 192.168.4.1", 50);
            display.update();
        });

        wifiManager.setSaveConfigCallback([this]() {
            display.clear();
            display.printCentered("WiFi Saved!", 20);
            display.printCentered("Connecting...", 40);
            display.update();
        });

        wifiManager.setCustomHeadElement(
            "<style>"
            "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea "
            "0%,#764ba2 "
            "100%);"
            "margin:0;padding:20px;color:white;}"
            ".form-group{background:rgba(255,255,255,0.95);padding:20px;border-radius:10px;"
            "box-shadow:0 8px 32px rgba(0,0,0,0.1);margin-bottom:20px;}"
            "input[type='text'],input[type='password']{width:100%;padding:12px;border:2px solid "
            "#ddd;"
            "border-radius:6px;font-size:16px;box-sizing:border-box;transition:border-color 0.3s;"
            "color:#333;}"
            "input[type='text']:focus,input[type='password']:focus{border-color:#667eea;outline:"
            "none;}"
            "button{background:linear-gradient(135deg,#667eea 0%,#764ba2 "
            "100%);color:white;border:none;"
            "padding:12px "
            "24px;border-radius:6px;font-size:16px;cursor:pointer;transition:transform "
            "0.2s;}"
            "button:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.2);}"
            "h1{text-align:center;color:white;margin-bottom:30px;font-size:28px;text-shadow:0 2px "
            "4px "
            "rgba(0,0,0,0.3);}"
            "label{display:block;margin-bottom:8px;font-weight:bold;color:white;}"
            ".btn-container{text-align:center;margin-top:20px;}"
            ".btn-container button{margin:0 10px;}"
            "</style>");

        // Set custom menu items to hide OTA and other advanced options
        std::vector<const char*> menu = {"wifi", "info"};
        wifiManager.setMenu(menu);

        // Add custom parameters for better user experience
        WiFiManagerParameter custom_text(
            "<p style='text-align:center;color:white;font-size:16px;margin:20px 0;'>"
            "Welcome to IR Hub WiFi Setup</p>");
        wifiManager.addParameter(&custom_text);
    }

    void setupOTA() {
        LOG_DEBUG("Setting up OTA...");
        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("Setting up OTA...", 40);
        display.update();

        ArduinoOTA.setHostname("ir-hub");

        ArduinoOTA.onStart([this]() {
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

        ArduinoOTA.onEnd([this]() {
            LOG_INFO("OTA Update Complete");
            display.clear();
            display.printCentered("OTA Complete", 20);
            display.printCentered("Restarting...", 40);
            display.update();
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            display.clear();
            display.printCentered("OTA Update", 10);

            display.drawProgressBar(10, 32, 108, 12, progress, total, true);

            display.update();
        });

        ArduinoOTA.onError([this](ota_error_t error) {
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
    }

   public:
    WiFiManagerLib(Display& display, LedRing& ring, Speaker& speaker)
        : display(display), ring(ring), speaker(speaker), wifiConnected(false) {}

    bool begin(const char* apName = "IRHub Setup", int apTimeout = 180, int connectTimeout = 60) {
        setupWiFiManager(apName, apTimeout, connectTimeout);

        // Check if WiFi credentials exist using WiFiManager
        bool hasCredentials = wifiManager.getWiFiIsSaved();

        if (hasCredentials) {
            display.clear();
            display.printCentered("IR Hub", 20);
            display.printCentered("Connecting...", 40);
            display.update();

            LOG_INFO("Found saved WiFi credentials, attempting connection...");
            LOG_DEBUG("Saved SSID: %s", WiFi.SSID().c_str());
        }

        LOG_INFO("Starting WiFi connection attempt...");
        LOG_DEBUG("WiFi timeout set to %d seconds", connectTimeout);

        // Try to connect to WiFi
        wifiConnected = wifiManager.autoConnect(apName);

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
            setupOTA();

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

        return wifiConnected;
    }

    void process() { wifiManager.process(); }

    bool isConnected() const { return wifiConnected && WiFi.status() == WL_CONNECTED; }

    void update() {
        if (isConnected()) {
            ArduinoOTA.handle();  // Handle OTA updates only if WiFi is connected
        }
    }

    void resetWiFi() {
        wifiManager.resetSettings();
        LOG_INFO("WiFi settings reset");
    }
};

#endif  // WIFI_MANAGER_H
