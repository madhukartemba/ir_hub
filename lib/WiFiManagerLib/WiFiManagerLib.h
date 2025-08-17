#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "Display.h"
#include "Speaker.h"

class WiFiManagerLib {
   private:
    WiFiManager wifiManager;
    Display& display;
    Speaker& speaker;
    bool wifiConnected;

    void setupWiFiManager(const char* apName, int apTimeout, int connectTimeout);
    void setupOTA();
    void displayWiFiStatus();

   public:
    WiFiManagerLib(Display& display, Speaker& speaker);

    bool begin(const char* apName = "IRHub Setup", int apTimeout = 180, int connectTimeout = 60);
    bool isConnected() const;
    void update();
    void resetWiFi();
};

#endif  // WIFI_MANAGER_H
