
#pragma once

#include <Arduino.h>
#include <WiFiManager.h>
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"
#include "fauxmoESP.h"

class AlexaConnector {
   private:
    fauxmoESP fauxmo;
    WiFiManager wifiManager;
    DeviceManager& deviceManager;
    bool isInitialized;
    std::vector<int> registeredDeviceIds;

   public:
    AlexaConnector(DeviceManager& deviceManager)
        : deviceManager(deviceManager), isInitialized(false) {}

    void setup() {
        if (isInitialized) {
            LOG_WARN("AlexaConnector already initialized");
            return;
        }

        // Initialize fauxmoESP
        fauxmo.createServer(80);
        fauxmo.setPort(80);
        fauxmo.enable(true);

        // Register callback for Alexa commands
        fauxmo.addDevice("ir_hub_controller");
        fauxmo.onSetState(
            [this](unsigned char device_id, const char* device_name, bool state,
                   unsigned char value) { this->handleAlexaCommand(device_name, state); });

        // Register all existing devices
        registerAllDevices();

        isInitialized = true;
        LOG_INFO("AlexaConnector initialized successfully");
    }

    void loop() {
        if (isInitialized) {
            fauxmo.handle();
        }
    }

    // Register all devices from DeviceManager with Alexa
    void registerAllDevices() {
        try {
            std::vector<Device> devices = deviceManager.getDevices();
            LOG_INFO("Registering %d devices with Alexa", devices.size());

            for (const Device& device : devices) {
                registerDevice(device);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to register devices with Alexa: %s", e.what());
        }
    }

    // Register a single device with Alexa
    void registerDevice(const Device& device) {
        if (!isInitialized) {
            LOG_WARN("AlexaConnector not initialized, cannot register device");
            return;
        }

        // Check if device is already registered
        if (isDeviceRegistered(device.id)) {
            LOG_INFO("Device %d already registered with Alexa", device.id);
            return;
        }

        // Create a unique device name for Alexa
        String alexaDeviceName = "ir_device_" + String(device.id);

        // Add device to fauxmoESP
        fauxmo.addDevice(alexaDeviceName.c_str());

        // Store the registered device ID
        registeredDeviceIds.push_back(device.id);

        LOG_INFO("Registered device %d (%s) with Alexa as '%s'", device.id, device.name.c_str(),
                 alexaDeviceName.c_str());
    }

    // Unregister a device from Alexa
    void unregisterDevice(int deviceId) {
        if (!isInitialized) {
            LOG_WARN("AlexaConnector not initialized, cannot unregister device");
            return;
        }

        String alexaDeviceName = "ir_device_" + String(deviceId);

        // Remove device from fauxmoESP (if supported)
        // Note: fauxmoESP doesn't have a direct remove method,
        // but we can track it in our registeredDeviceIds

        // Remove from our tracking list
        registeredDeviceIds.erase(
            std::remove(registeredDeviceIds.begin(), registeredDeviceIds.end(), deviceId),
            registeredDeviceIds.end());

        LOG_INFO("Unregistered device %d from Alexa", deviceId);
    }

    // Handle Alexa command for a specific device
    void handleAlexaCommand(const char* device_name, bool state) {
        LOG_INFO("Alexa command received: %s for device %s", state ? "ON" : "OFF", device_name);

        // Extract device ID from device name (format: "ir_device_<id>")
        String deviceNameStr = String(device_name);
        if (deviceNameStr.startsWith("ir_device_")) {
            String idStr = deviceNameStr.substring(10);  // Remove "ir_device_" prefix
            int deviceId = idStr.toInt();

            try {
                Device device = deviceManager.getDevice(deviceId);
                executeDeviceCommand(device, state);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to execute Alexa command for device %d: %s", deviceId, e.what());
            }
        } else {
            LOG_WARN("Unknown device name format: %s", device_name);
        }
    }

    // Execute the actual IR command for a device
    void executeDeviceCommand(const Device& device, bool state) {
        IRCode commandToSend;

        if (state) {
            // Turn ON - use onCommand
            commandToSend = device.onCommand;
            LOG_INFO("Executing ON command for device %d (%s)", device.id, device.name.c_str());
        } else {
            // Turn OFF - use offCommand
            commandToSend = device.offCommand;
            LOG_INFO("Executing OFF command for device %d (%s)", device.id, device.name.c_str());
        }

        // Send the IR command using IRManager
        if (commandToSend.isValid()) {
            // Access the global IRManager instance
            extern IRManager irManager;
            irManager.sendProtocol(commandToSend);
            LOG_INFO("IR command sent for device %d (protocol: %d, value: 0x%llX)", device.id,
                     (int)commandToSend.getProtocol(), commandToSend.getValue());
        } else {
            LOG_ERROR("Invalid IR command for device %d", device.id);
        }
    }

    // Check if a device is registered with Alexa
    bool isDeviceRegistered(int deviceId) {
        return std::find(registeredDeviceIds.begin(), registeredDeviceIds.end(), deviceId) !=
               registeredDeviceIds.end();
    }

    // Get list of registered device IDs
    std::vector<int> getRegisteredDeviceIds() { return registeredDeviceIds; }

    // Refresh device registrations (useful after device changes)
    void refreshDevices() {
        if (!isInitialized) {
            LOG_WARN("AlexaConnector not initialized, cannot refresh devices");
            return;
        }

        LOG_INFO("Refreshing Alexa device registrations");
        registeredDeviceIds.clear();
        registerAllDevices();
    }

    // Callback methods for DeviceManager notifications
    void onDeviceAdded(const Device& device) {
        LOG_INFO("AlexaConnector notified of new device: %d (%s)", device.id, device.name.c_str());
        if (isInitialized) {
            registerDevice(device);
        }
    }

    void onDeviceRemoved(int deviceId) {
        LOG_INFO("AlexaConnector notified of removed device: %d", deviceId);
        if (isInitialized) {
            unregisterDevice(deviceId);
        }
    }
};