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
    DeviceManager& deviceManager;
    IRManager& irManager;
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

        for (Device& device : deviceManager.getDevices()) {
            registerDevice(device);
        }

        fauxmo.onSetState([this](unsigned char device_id, const char* device_name, bool state,
                                 unsigned char value) {
            LOG_DEBUG("[Alexa] Set state for device %s (ID: %d) to %s with value %d", device_name,
                      device_id, state ? "ON" : "OFF", value);

            Device* device = deviceManager.getDeviceByName(device_name);
            if (device) {
                if (state) {
                    irManager.sendProtocol(device->onCommand);
                    LOG_INFO("[Alexa] Turning ON device %s (ID: %d)", device_name, device_id);
                } else {
                    irManager.sendProtocol(device->offCommand);
                    LOG_INFO("[Alexa] Turning OFF device %s (ID: %d)", device_name, device_id);
                }
            } else {
                LOG_ERROR("[Alexa] Device %s (ID: %d) not found", device_name, device_id);
            }
        });

        deviceManager.setOnDeviceAdded([this](const Device& device) {
            LOG_DEBUG("[Alexa] Device added: %s (ID: %d)", device.name.c_str(), device.id);
            registerDevice(device);
        });

        deviceManager.setOnDeviceRemoved([this](const Device& device) {
            LOG_DEBUG("[Alexa] Device removed: %s (ID: %d)", device.name.c_str(), device.id);
            unregisterDevice(device);
        });
    }

    void registerDevice(const Device& device) { fauxmo.addDevice(device.name.c_str()); }
    void unregisterDevice(const Device& device) { fauxmo.removeDevice(device.name.c_str()); }

    void update() { fauxmo.handle(); }
};