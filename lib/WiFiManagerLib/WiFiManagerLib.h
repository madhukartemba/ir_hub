#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "Display.h"
#include "Log.h"
#include "NeoRing.h"
#include "Speaker.h"

class WiFiManagerLib {
   private:
    WiFiManager wifiManager;
    Display& display;
    NeoRing& ledRing;
    Speaker& speaker;
    bool wifiConnected;
    bool isOtaSetup = false;
    uint32_t otaColor = 0x0000FF;
    uint32_t otaSuccessColor = 0x00FF00;
    uint32_t otaErrorColor = 0xFF0000;

    void setupWiFiManager(const char* apName, int apTimeout, int connectTimeout) {
        // Configure timeout (in seconds)
        wifiManager.setConfigPortalTimeout(apTimeout);
        wifiManager.setConnectTimeout(connectTimeout);

        // Disable OTA update options and other advanced features
        wifiManager.setBreakAfterConfig(true);    // Exit after configuration
        wifiManager.setRemoveDuplicateAPs(true);  // Remove duplicate APs
        wifiManager.setMinimumSignalQuality(30);  // Minimum signal quality

        // Customize the portal appearance
        wifiManager.setTitle("IR Hub Wi-Fi Setup");
        wifiManager.setClass("invert");

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

        // Set custom menu items to hide OTA and other advanced options
        std::vector<const char*> menu = {"wifi", "info"};
        wifiManager.setMenu(menu);

        // Add custom parameters for better user experience
        WiFiManagerParameter custom_text(
            "<p style='text-align:center;color:white;font-size:16px;margin:20px 0;'>"
            "IR Hub Wi-Fi Setup</p>");
        wifiManager.addParameter(&custom_text);
    }

   public:
    WiFiManagerLib(Display& display, NeoRing& ledRing, Speaker& speaker)
        : display(display), ledRing(ledRing), speaker(speaker), wifiConnected(false) {}

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

    void setupOTA(uint32_t otaColor = 0x0000FF, uint32_t otaSuccessColor = 0x00FF00,
                  uint32_t otaErrorColor = 0xFF0000) {
        this->otaColor = otaColor;
        this->otaSuccessColor = otaSuccessColor;
        this->otaErrorColor = otaErrorColor;

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
            ledRing.spinner(this->otaColor);
            display.clear();
            display.printCentered("OTA Update", 10);
            display.printCentered("Starting...", 30);
            display.update();
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            ledRing.update();
            display.clear();
            display.printCentered("OTA Update", 10);

            display.drawProgressBar(10, 32, 108, 12, progress, total, true);

            display.update();
        });

        ArduinoOTA.onEnd([this]() {
            LOG_INFO("OTA Update Complete");
            ledRing.solid(this->otaSuccessColor);
            ledRing.finishTransition();
            display.clear();
            display.printCentered("OTA Complete", 20);
            display.printCentered("Restarting...", 40);
            display.update();
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            ledRing.solid(this->otaErrorColor);
            ledRing.finishTransition();
            LOG_ERROR("OTA Error: " + String(error));
            display.clear();
            display.printCentered("OTA Error", 10);
            display.printCentered("Error: " + String(error), 30);
            display.update();
        });

        ArduinoOTA.begin();
        isOtaSetup = true;
        LOG_INFO("OTA Ready");

        // Display OTA info
        display.clear();
        display.printCentered("IR Hub", 10);
        display.printCentered("IP: " + WiFi.localIP().toString(), 25);
        display.printCentered("OTA: Ready", 40);
        display.update();
        delay(2000);
    }

    bool isConnected() const { return wifiConnected && WiFi.status() == WL_CONNECTED; }

    void update() {
        if (isConnected()) {
            if (isOtaSetup) {
                ArduinoOTA.handle();  // Handle OTA updates only if WiFi is connected
            }
        }
    }

    void resetWiFi() {
        wifiManager.resetSettings();
        LOG_INFO("WiFi settings reset");
    }
};

#endif  // WIFI_MANAGER_H
