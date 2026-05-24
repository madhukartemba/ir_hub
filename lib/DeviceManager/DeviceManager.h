#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <functional>
#include <optional>
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

/// Stateless device store. Every lookup hits LittleFS — there is no in-RAM
/// cache. This trades ~5–15 ms of disk + JSON-parse latency per lookup for
/// keeping the steady-state heap free of duplicated Device data, which
/// matters during the OTA manifest fetch (BearSSL needs a contiguous 6 KB
/// session block on a tight ~16 KB free-heap budget).
///
/// Lookup APIs return `std::optional<Device>` by value. Callers that need
/// to retain the Device must copy/move it; the returned object owns its
/// own strings + state vector.
class DeviceManager {
   public:
    using DeviceCallback = std::function<void(const Device&)>;

   private:
    const char* storageDir = "/devices";
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

        if (onDeviceAdded) {
            LOG_DEBUG("[DeviceManager] Triggering onDeviceAdded callback for device %d", device.id);
            onDeviceAdded(device);
        }

        LOG_INFO("[DeviceManager] Device saved to %s", filename.c_str());
    }

    bool removeDeviceById(int id) {
        auto device = getDeviceById(id);
        if (device) {
            return removeDevice(*device);
        }
        return false;
    }

    bool removeDevice(const Device& device) {
        // Snapshot so the onDeviceRemoved callback still sees valid data
        // even if `device` aliased a temporary that gets invalidated by
        // LittleFS.remove (paranoia held over from the cached impl).
        Device snapshot = device;

        String filename = String(snapshot.id) + ".json";
        bool success = LittleFS.remove(String(storageDir) + "/" + filename);
        if (success) {
            LOG_INFO("[DeviceManager] Device removed from %s", filename.c_str());
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

    /// Each call hits LittleFS — returns std::nullopt if the device doesn't
    /// exist. The returned Device owns its own data; the caller may keep it
    /// for as long as needed.
    std::optional<Device> getDeviceById(int id) {
        return loadDeviceFromDisk(id);
    }

    /// O(N) directory scan + per-file parse. Used only by the Alexa command
    /// path, which is rare enough that the linear scan doesn't matter.
    std::optional<Device> getDeviceByName(const String& name) {
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            int id = filename.substring(0, filename.lastIndexOf('.')).toInt();
            auto device = loadDeviceFromDisk(id);
            if (device && device->name == name) {
                return device;
            }
        }
        LOG_DEBUG("[DeviceManager] Device %s not found", name.c_str());
        return std::nullopt;
    }

    std::vector<Device> getDevices() {
        std::vector<Device> devices;
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            int id = filename.substring(0, filename.lastIndexOf('.')).toInt();
            if (auto device = loadDeviceFromDisk(id)) {
                devices.push_back(std::move(*device));
            }
        }
        LOG_INFO("[DeviceManager] Loaded %d devices", devices.size());
        return devices;
    }

    /// Iterate every device. The Device passed to `fn` is a freshly-loaded
    /// stack-local — it goes out of scope once `fn` returns, so the callback
    /// must not retain the reference. (All current call sites — MQTT
    /// discovery + Alexa registration — only use it synchronously.)
    template <typename Fn>
    void forEachDevice(Fn fn) {
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            int id = filename.substring(0, filename.lastIndexOf('.')).toInt();
            if (auto device = loadDeviceFromDisk(id)) {
                fn(*device);
            }
        }
    }

    /// Number of stored devices. Counts JSON files without parsing them, so
    /// this is cheap and safe to call from UI code.
    size_t deviceCount() {
        size_t count = 0;
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            if (String(dir.fileName()).endsWith(".json")) {
                count++;
            }
        }
        return count;
    }

   private:
    /// Stream-parses /devices/<id>.json. Returns std::nullopt on missing
    /// file or malformed JSON. Stream parsing avoids materialising the
    /// whole file as a String (~free fragmentation win vs file.readString()).
    std::optional<Device> loadDeviceFromDisk(int id) {
        String filename = String(id) + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "r");
        if (!file) {
            LOG_ERROR("[DeviceManager] Failed to open file for device %d", id);
            return std::nullopt;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        if (err) {
            LOG_ERROR("[DeviceManager] Failed to parse JSON for device %d: %s", id, err.c_str());
            return std::nullopt;
        }

        Device device;
        device.id = id;
        device.type = (DeviceType)doc["type"].as<int>();
        device.name = doc["name"] | "";
        device.protocolName = doc["protocolName"] | "";
        device.onCommand = IRCode::fromJson(doc["onCommand"]);
        device.offCommand = IRCode::fromJson(doc["offCommand"]);
        return device;
    }
};
