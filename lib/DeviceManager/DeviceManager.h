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
   public:
    using DeviceCallback = std::function<void(const Device&)>;

   private:
    const char* storageDir = "/devices";
    std::map<String, Device> deviceCacheByName;
    std::map<int, Device> deviceCacheById;
    bool cacheLoaded = false;
    IdGen& idGen;
    DeviceCallback onDeviceAdded;
    DeviceCallback onDeviceRemoved;

   public:
    DeviceManager(IdGen& idGen) : idGen(idGen) {}
    ~DeviceManager() {}

    void setOnDeviceAdded(DeviceCallback cb) {
        LOG_DEBUG("[DeviceManager] Setting onDeviceAdded callback");
        onDeviceAdded = cb;
    }

    void setOnDeviceRemoved(DeviceCallback cb) {
        LOG_DEBUG("[DeviceManager] Setting onDeviceRemoved callback");
        onDeviceRemoved = cb;
    }

    bool begin() {
        if (!LittleFS.begin()) {
            LOG_ERROR("[DeviceManager] Failed to mount LittleFS");
            return false;
        }
        LOG_INFO("[DeviceManager] LittleFS mounted");

        if (!LittleFS.exists(storageDir)) {
            LittleFS.mkdir(storageDir);
            LOG_INFO("[DeviceManager] Storage directory created");
        } else {
            LOG_INFO("[DeviceManager] Storage directory exists");
        }
        return true;
    }

    int addSingleCommandDevice(IRCode command) {
        if (!command.isValid()) {
            LOG_ERROR("[DeviceManager] Invalid command");
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
                LOG_ERROR("[DeviceManager] Invalid on command");
            }
            if (!offCommand.isValid()) {
                LOG_ERROR("[DeviceManager] Invalid off command");
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
            LOG_ERROR("[DeviceManager] Failed to open file");
            return;
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

        if (onDeviceAdded) {
            LOG_DEBUG("[DeviceManager] Triggering onDeviceAdded callback for device %d", device.id);
            onDeviceAdded(device);
        }

        LOG_INFO("[DeviceManager] Device saved to %s", filename.c_str());
    }

    bool removeDeviceById(int id) {
        Device* device = getDeviceById(id);
        if (device) {
            return removeDevice(*device);
        }
        return false;
    }

    bool removeDevice(const Device& device) {
        // Snapshot before mutating: `device` is often a reference into the
        // cache we're about to erase from.
        Device snapshot = device;

        String filename = String(snapshot.id) + ".json";
        bool success = LittleFS.remove(String(storageDir) + "/" + filename);
        if (success) {
            LOG_INFO("[DeviceManager] Device removed from %s", filename.c_str());
            deviceCacheByName.erase(snapshot.name);
            deviceCacheById.erase(snapshot.id);

            if (onDeviceRemoved) {
                LOG_DEBUG("[DeviceManager] Triggering onDeviceRemoved callback for device %d",
                          snapshot.id);
                onDeviceRemoved(snapshot);
            }
        } else {
            LOG_ERROR("[DeviceManager] Failed to remove device from %s", filename.c_str());
        }
        return success;
    }

    Device* getDeviceById(int id) {
        auto it = deviceCacheById.find(id);
        if (it != deviceCacheById.end()) {
            LOG_DEBUG("[DeviceManager] Found device %d in cache", id);
            return &(it->second);
        }

        LOG_DEBUG("[DeviceManager] Device %d not found in cache, loading from storage", id);

        String filename = String(id) + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "r");
        if (!file) {
            LOG_ERROR("[DeviceManager] Failed to open file for device %d", id);
            return nullptr;
        }
        String json = file.readString();
        file.close();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err) {
            LOG_ERROR("[DeviceManager] Failed to parse JSON for device %d: %s", id, err.c_str());
            return nullptr;
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

        return &(itById->second);
    }

    Device* getDeviceByName(const String& name) {
        auto it = deviceCacheByName.find(name);
        if (it != deviceCacheByName.end()) {
            LOG_DEBUG("[DeviceManager] Found device %s in cache", name.c_str());
            return &(it->second);
        }

        LOG_DEBUG("[DeviceManager] Device %s not found in cache, loading from storage",
                  name.c_str());

        for (Device& device : getDevices()) {
            if (device.name == name) {
                LOG_DEBUG("[DeviceManager] Loaded device %s from storage", name.c_str());
                return &device;
            }
        }
        LOG_ERROR("[DeviceManager] Device %s not found", name.c_str());
        return nullptr;
    }

    std::vector<Device> getDevices() {
        loadAll();
        std::vector<Device> devices;
        devices.reserve(deviceCacheById.size());
        for (const auto& kv : deviceCacheById) {
            devices.push_back(kv.second);
        }
        LOG_INFO("[DeviceManager] Loaded %d devices", devices.size());
        return devices;
    }

    /// Iterate every device without allocating a vector copy.
    template <typename Fn>
    void forEachDevice(Fn fn) {
        loadAll();
        for (auto& kv : deviceCacheById) {
            fn(kv.second);
        }
    }

    /// Number of known devices (cheap; uses the cache).
    size_t deviceCount() {
        loadAll();
        return deviceCacheById.size();
    }

   private:
    /// Lazy one-shot scan; cache stays in sync via saveDevice/removeDevice.
    void loadAll() {
        if (cacheLoaded) {
            return;
        }
        cacheLoaded = true;  // set first so any failure isn't retried each call

        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) {
                continue;
            }
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) {
                continue;
            }
            int id = filename.substring(0, filename.lastIndexOf('.')).toInt();
            if (!getDeviceById(id)) {
                LOG_ERROR("[DeviceManager] Failed to load device from %s", filename.c_str());
            }
        }
        LOG_INFO("[DeviceManager] Cache primed with %u devices",
                 (unsigned)deviceCacheById.size());
    }
};