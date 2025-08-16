#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <functional>
#include <vector>
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"
#include "fauxmoESP.h"

class AlexaConnector {
   private:
    IRManager& irManager;
    DeviceManager& deviceManager;
    fauxmoESP fauxmo;

   public:
    AlexaConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager), irManager(irManager) {};

    ~AlexaConnector() {};

    void begin() {
        WiFi.mode(WIFI_STA);

        if (WiFi.status() != WL_CONNECTED) {
            LOG_ERROR("[Alexa] Not connected to WiFi, skipping alexa setup");
            return;
        }

        fauxmo.createServer(true);
        fauxmo.setPort(80);
        fauxmo.enable(true);

        // Now we register devices
    }

    void registerDevice(Device device) {}
};