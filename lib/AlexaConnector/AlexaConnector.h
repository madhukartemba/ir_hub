#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Espalexa.h>
#include <functional>
#include <vector>
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"

class AlexaConnector {
   private:
    DeviceManager& deviceManager;
    IRManager& irManager;
    Espalexa espalexa;
    bool wifiEnabled;
    std::function<void(const Device& device, bool state)> onStateChangeCallback;

    void handleDeviceCallback(const String& deviceName, uint8_t value) {
        bool state = value > 0;
        LOG_DEBUG("[Alexa] Set state for device %s to %s with value %d", deviceName.c_str(),
                  state ? "ON" : "OFF", value);

        // Look up the device from device manager
        Device* device = deviceManager.getDeviceByName(deviceName);
        if (device) {
            onStateChangeCallback(*device, state);
            if (state) {
                irManager.sendProtocol(device->onCommand);
                LOG_INFO("[Alexa] Turning ON device %s", deviceName.c_str());
            } else {
                irManager.sendProtocol(device->offCommand);
                LOG_INFO("[Alexa] Turning OFF device %s", deviceName.c_str());
            }
        } else {
            LOG_ERROR("[Alexa] Device %s not found in device manager", deviceName.c_str());
        }
    }

   public:
    AlexaConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager), irManager(irManager), wifiEnabled(false) {};

    ~AlexaConnector() {};

    void begin() {
        // Check if WiFi is available and connected
        if (WiFi.status() != WL_CONNECTED) {
            LOG_INFO("[Alexa] WiFi not connected, Alexa functionality disabled");
            wifiEnabled = false;
            return;
        }

        wifiEnabled = true;
        WiFi.mode(WIFI_STA);

        // Register all existing devices
        for (Device& device : deviceManager.getDevices()) {
            registerDevice(device);
        }

        // Set up device callbacks
        deviceManager.setOnDeviceAdded([this](const Device& device) {
            LOG_DEBUG("[Alexa] Device added: %s (ID: %d)", device.name.c_str(), device.id);
            registerDevice(device);
        });

        deviceManager.setOnDeviceRemoved([this](const Device& device) {
            LOG_DEBUG("[Alexa] Device removed: %s (ID: %d)", device.name.c_str(), device.id);
            unregisterDevice(device);
        });

        espalexa.begin();
        LOG_INFO("[Alexa] Alexa functionality enabled");
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void registerDevice(const Device& device) {
        if (wifiEnabled) {
            espalexa.addDevice(
                device.name.c_str(),
                [this, deviceName = device.name](EspalexaDevice* d) {
                    handleDeviceCallback(deviceName, d->getValue());
                },
                EspalexaDeviceType::onoff);
        }
    }

    void unregisterDevice(const Device& device) {
        if (wifiEnabled) {
            // Espalexa doesn't have a direct removeDevice method
            // We'll need to handle this differently - devices will remain registered
            // but won't be accessible through the device manager
            LOG_DEBUG("[Alexa] Device %s unregistered (note: Espalexa keeps devices registered)",
                      device.name.c_str());
        }
    }

    void update() {
        if (wifiEnabled) {
            espalexa.loop();
        }
    }

    bool isEnabled() const { return wifiEnabled; }
};