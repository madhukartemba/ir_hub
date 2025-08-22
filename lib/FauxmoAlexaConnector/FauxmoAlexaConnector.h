#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <functional>
#include <vector>
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"
#include "fauxmoESP.h"

// Legacy code, not used anymore due to unsupported value errors popping up
// and the device being shown as unresponsive. Kept it here for reference.
class FauxmoAlexaConnector {
   private:
    DeviceManager& deviceManager;
    IRManager& irManager;
    fauxmoESP fauxmo;
    bool wifiEnabled;
    std::function<void(const Device& device, bool state)> onStateChangeCallback;

   public:
    FauxmoAlexaConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager), irManager(irManager), wifiEnabled(false) {};

    ~FauxmoAlexaConnector() {};

    void begin() {
        // Check if WiFi is available and connected
        if (WiFi.status() != WL_CONNECTED) {
            LOG_INFO("[Alexa] WiFi not connected, Alexa functionality disabled");
            wifiEnabled = false;
            return;
        }

        wifiEnabled = true;
        WiFi.mode(WIFI_STA);

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
                onStateChangeCallback(*device, state);
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

        LOG_INFO("[Alexa] Alexa functionality enabled");
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void registerDevice(const Device& device) {
        if (wifiEnabled) {
            fauxmo.addDevice(device.name.c_str());
        }
    }

    void unregisterDevice(const Device& device) {
        if (wifiEnabled) {
            fauxmo.removeDevice(device.name.c_str());
        }
    }

    void update() {
        if (wifiEnabled) {
            fauxmo.handle();
        }
    }

    bool isEnabled() const { return wifiEnabled; }
};