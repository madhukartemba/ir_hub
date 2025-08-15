#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <functional>
#include <vector>
#include "IRCode.h"
#include "IdGen.h"
#include "Log.h"

enum DeviceType {
    SINGLE_COMMAND,
    DUAL_COMMAND,
};

struct Device {
    int id;
    DeviceType type;
    String name;
    String protocolName;
    IRCode onCommand;
    IRCode offCommand;
};

class DeviceManager {
   private:
    const char* storageDir = "/devices";
    IdGen& idGen;
    std::function<void(const Device&)> deviceAddedCallback;
    std::function<void(int)> deviceRemovedCallback;

   public:
    DeviceManager(IdGen& idGen) : idGen(idGen) {}
    ~DeviceManager() {}

    // Callback registration methods
    void setDeviceAddedCallback(std::function<void(const Device&)> callback) {
        deviceAddedCallback = callback;
        LOG_INFO("Device added callback set");
    }

    void setDeviceRemovedCallback(std::function<void(int)> callback) {
        deviceRemovedCallback = callback;
        LOG_INFO("Device removed callback set");
    }

    void notifyDeviceAdded(const Device& device) {
        if (deviceAddedCallback) {
            deviceAddedCallback(device);
        }
    }

    void notifyDeviceRemoved(int deviceId) {
        if (deviceRemovedCallback) {
            deviceRemovedCallback(deviceId);
        }
    }

    bool begin() {
        if (!LittleFS.begin()) {
            LOG_ERROR("Failed to mount LittleFS");
            return false;
        }
        LOG_INFO("LittleFS mounted");

        if (!LittleFS.exists(storageDir)) {
            LittleFS.mkdir(storageDir);
            LOG_INFO("Storage directory created");
        } else {
            LOG_INFO("Storage directory exists");
        }
        return true;
    }

    void setStorageDir(char* dir) { storageDir = dir; }

    int addSingleCommandDevice(IRCode command) {
        if (!command.isValid()) {
            LOG_ERROR("Invalid command");
            return -1;
        }

        Device device;
        device.id = idGen.generateId();
        device.protocolName = String(typeToString(command.getProtocol(), false));
        device.name = String(device.id) + "-" + device.protocolName;
        device.type = SINGLE_COMMAND;
        device.onCommand = command;
        device.offCommand = command;
        saveDevice(device);

        // Notify AlexaConnector about the new device
        notifyDeviceAdded(device);

        return device.id;
    }

    int addDualCommandDevice(IRCode onCommand, IRCode offCommand) {
        if (!onCommand.isValid() || !offCommand.isValid()) {
            if (!onCommand.isValid()) {
                LOG_ERROR("Invalid on command");
            }
            if (!offCommand.isValid()) {
                LOG_ERROR("Invalid off command");
            }
            return -1;
        }

        Device device;
        device.id = idGen.generateId();
        if (onCommand.getProtocol() == offCommand.getProtocol()) {
            device.protocolName = String(typeToString(onCommand.getProtocol(), false));
        } else {
            device.protocolName = String(typeToString(onCommand.getProtocol(), false)) + " & " +
                                  String(typeToString(offCommand.getProtocol(), false));
        }
        device.name = String(device.id) + "-" + device.protocolName;
        device.type = DUAL_COMMAND;
        device.onCommand = onCommand;
        device.offCommand = offCommand;
        saveDevice(device);

        // Notify AlexaConnector about the new device
        notifyDeviceAdded(device);

        return device.id;
    }

    void saveDevice(Device device) {
        String filename = String(device.id) + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "w");
        if (!file) {
            LOG_ERROR("Failed to open file");
            throw std::runtime_error("Failed to open file");
        }
        JsonDocument doc;
        doc["id"] = device.id;
        doc["type"] = device.type;
        doc["name"] = device.name;
        doc["protocolName"] = device.protocolName;
        doc["onCommand"] = device.onCommand.toJson();
        doc["offCommand"] = device.offCommand.toJson();
        file.print(doc.as<String>());
        file.close();
        LOG_INFO("Device saved to %s", filename.c_str());
    }

    bool removeDevice(int id) {
        Device device = getDevice(id);
        return removeDevice(device);
    }

    bool removeDevice(Device device) {
        String filename = String(device.id) + ".json";
        bool success = LittleFS.remove(String(storageDir) + "/" + filename);
        if (success) {
            LOG_INFO("Device removed from %s", filename.c_str());
            // Notify AlexaConnector about the removed device
            notifyDeviceRemoved(device.id);
        } else {
            LOG_ERROR("Failed to remove device from %s", filename.c_str());
        }
        return success;
    }

    Device getDevice(int id) {
        String filename = String(id) + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "r");
        if (!file) {
            LOG_ERROR("Failed to open file");
            throw std::runtime_error("Device not found");
        }
        String json = file.readString();
        file.close();
        JsonDocument doc;
        deserializeJson(doc, json);

        Device device;
        device.id = id;
        device.type = (DeviceType)doc["type"];
        device.name = doc["name"] | "";
        device.protocolName = doc["protocolName"] | "";

        device.onCommand = IRCode::fromJson(doc["onCommand"]);
        device.offCommand = IRCode::fromJson(doc["offCommand"]);
        return device;
    }

    std::vector<Device> getDevices() {
        std::vector<Device> devices;

        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (dir.isFile() && String(dir.fileName()).endsWith(".json")) {
                try {
                    // Extract device ID from filename (remove .json extension)
                    String filename = String(dir.fileName());
                    String idStr = filename.substring(0, filename.lastIndexOf('.'));
                    int id = idStr.toInt();

                    Device device = getDevice(id);
                    devices.push_back(device);
                } catch (const std::runtime_error& e) {
                    LOG_ERROR("Failed to load device from %s: %s", dir.fileName(), e.what());
                }
            }
        }

        std::sort(devices.begin(), devices.end(),
                  [](const Device& a, const Device& b) { return a.id < b.id; });

        LOG_INFO("Loaded %d devices", devices.size());
        return devices;
    }
};