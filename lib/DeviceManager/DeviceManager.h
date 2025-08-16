#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
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
    std::map<String, Device> deviceCacheByName;
    std::map<int, Device> deviceCacheById;
    IdGen& idGen;

   public:
    DeviceManager(IdGen& idGen) : idGen(idGen) {}
    ~DeviceManager() {}

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

    int addSingleCommandDevice(IRCode command) {
        if (!command.isValid()) {
            LOG_ERROR("Invalid command");
            return -1;
        }

        Device device;
        device.id = idGen.generateId();
        device.protocolName = String(typeToString(command.getProtocol(), false));
        device.name = device.protocolName + " " + String(device.id);
        device.type = SINGLE_COMMAND;
        device.onCommand = command;
        device.offCommand = command;
        saveDevice(device);

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
        device.name = device.protocolName + " " + String(device.id);
        device.type = DUAL_COMMAND;
        device.onCommand = onCommand;
        device.offCommand = offCommand;
        saveDevice(device);
        return device.id;
    }

    void saveDevice(const Device& device) {
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
        serializeJson(doc, file);
        file.close();

        deviceCacheByName.emplace(device.name, device);
        deviceCacheById.emplace(device.id, device);

        LOG_INFO("Device saved to %s", filename.c_str());
    }

    bool removeDeviceById(int id) {
        Device device = getDeviceById(id);
        return removeDevice(device);
    }

    bool removeDevice(const Device& device) {
        String filename = String(device.id) + ".json";
        bool success = LittleFS.remove(String(storageDir) + "/" + filename);
        if (success) {
            LOG_INFO("Device removed from %s", filename.c_str());
            deviceCacheByName.erase(device.name);
            deviceCacheById.erase(device.id);
        } else {
            LOG_ERROR("Failed to remove device from %s", filename.c_str());
        }
        return success;
    }

    Device& getDeviceById(int id) {
        auto it = deviceCacheById.find(id);
        if (it != deviceCacheById.end()) {
            LOG_DEBUG("[DeviceManager] Found device %d in cache", id);
            return it->second;
        }

        LOG_DEBUG("[DeviceManager] Device %d not found in cache, loading from storage", id);

        String filename = String(id) + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "r");
        if (!file) {
            LOG_ERROR("Failed to open file");
            throw std::runtime_error("Device not found");
        }
        String json = file.readString();
        file.close();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err) {
            LOG_ERROR("Failed to parse JSON for device %d: %s", id, err.c_str());
            throw std::runtime_error("Invalid JSON");
        }

        Device device;
        device.id = id;
        device.type = (DeviceType)doc["type"];
        device.name = doc["name"] | "";
        device.protocolName = doc["protocolName"] | "";

        device.onCommand = IRCode::fromJson(doc["onCommand"]);
        device.offCommand = IRCode::fromJson(doc["offCommand"]);

        // Insert into cache
        auto [itById, inserted] = deviceCacheById.emplace(device.id, std::move(device));
        deviceCacheByName.emplace(itById->second.name, itById->second);

        return itById->second;  // ✅ return reference to cached object
    }

    Device& getDeviceByName(const String& name) {
        auto it = deviceCacheByName.find(name);
        if (it != deviceCacheByName.end()) {
            LOG_DEBUG("[DeviceManager] Found device %s in cache", name.c_str());
            return it->second;
        }

        LOG_DEBUG("[DeviceManager] Device %s not found in cache, loading from storage",
                  name.c_str());

        for (Device& device : getDevices()) {
            if (device.name == name) {
                LOG_DEBUG("[DeviceManager] Loaded device %s from storage", name.c_str());
                return device;
            }
        }
        LOG_ERROR("[DeviceManager] Device %s not found", name.c_str());
        throw std::runtime_error("Device not found");
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

                    Device device = getDeviceById(id);
                    devices.push_back(device);
                } catch (const std::runtime_error& e) {
                    LOG_ERROR("Failed to load device from %s: %s", dir.fileName(), e.what());
                }
            }
        }

        LOG_INFO("Loaded %d devices", devices.size());
        return devices;
    }
};