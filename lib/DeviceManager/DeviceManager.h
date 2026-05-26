#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <functional>
#include <optional>
#include <vector>
#include "IRCode.h"
#include "Log.h"

extern "C" {
#include "user_interface.h"  // os_random()
}

enum DeviceType {
    SINGLE_COMMAND,
    DUAL_COMMAND,
};

struct Device {
    String uuid;
    uint8_t alexaSlot;
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
    DeviceCallback onDeviceAdded;
    DeviceCallback onDeviceRemoved;

    static String generateUuid() {
        uint32_t a = os_random() ^ (uint32_t)micros();
        uint32_t b = os_random();
        uint32_t c = os_random() ^ ESP.getCycleCount();
        char buf[25];
        snprintf(buf, sizeof(buf), "%08x%08x%08x", a, b, c);
        return String(buf);
    }

    uint8_t allocateAlexaSlot(const String& uuid) {
        bool used[256] = {false};
        used[0] = true;  // reserve 0 — Espalexa's `+1` encoding makes it untouchable anyway
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            File f = LittleFS.open(String(storageDir) + "/" + filename, "r");
            if (!f) continue;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (err) continue;
            int slot = doc["alexaSlot"] | 0;
            if (slot >= 1 && slot <= 255) {
                used[slot] = true;
            }
        }

        uint8_t seed = 1;
        if (uuid.length() >= 2) {
            char hex[3] = {uuid.charAt(0), uuid.charAt(1), '\0'};
            seed = (uint8_t)strtol(hex, nullptr, 16);
            if (seed == 0) seed = 1;
        }
        for (int offset = 0; offset < 255; offset++) {
            uint8_t candidate = (uint8_t)(((int)seed - 1 + offset) % 255 + 1);
            if (!used[candidate]) return candidate;
        }
        return 0;
    }

    size_t countExistingDevices() {
        size_t n = 0;
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            if (String(dir.fileName()).endsWith(".json")) n++;
        }
        return n;
    }

   public:
    DeviceManager() {}
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

    String addSingleCommandDevice(IRCode command) {
        if (!command.isValid()) {
            LOG_ERROR("[DeviceManager] Invalid command");
            return String();
        }

        Device device;
        device.uuid = generateUuid();
        device.alexaSlot = allocateAlexaSlot(device.uuid);
        if (device.alexaSlot == 0) {
            LOG_ERROR("[DeviceManager] No free Alexa slot (255 in use?)");
            return String();
        }
        device.protocolName = String(typeToString(command.getProtocol(), false));
        device.name = device.protocolName + " " + String((unsigned)(countExistingDevices() + 1));
        device.type = SINGLE_COMMAND;
        device.onCommand = command;
        device.offCommand = command;
        if (!saveDevice(device)) {
            return String();
        }

        return device.uuid;
    }

    String addDualCommandDevice(IRCode onCommand, IRCode offCommand) {
        if (!onCommand.isValid() || !offCommand.isValid()) {
            if (!onCommand.isValid()) {
                LOG_ERROR("[DeviceManager] Invalid on command");
            }
            if (!offCommand.isValid()) {
                LOG_ERROR("[DeviceManager] Invalid off command");
            }
            return String();
        }

        Device device;
        device.uuid = generateUuid();
        device.alexaSlot = allocateAlexaSlot(device.uuid);
        if (device.alexaSlot == 0) {
            LOG_ERROR("[DeviceManager] No free Alexa slot (255 in use?)");
            return String();
        }
        if (onCommand.getProtocol() == offCommand.getProtocol()) {
            device.protocolName = String(typeToString(onCommand.getProtocol(), false));
        } else {
            device.protocolName = String(typeToString(onCommand.getProtocol(), false)) + " & " +
                                  String(typeToString(offCommand.getProtocol(), false));
        }
        device.name = device.protocolName + " " + String((unsigned)(countExistingDevices() + 1));
        device.type = DUAL_COMMAND;
        device.onCommand = onCommand;
        device.offCommand = offCommand;
        if (!saveDevice(device)) {
            return String();
        }
        return device.uuid;
    }

    bool saveDevice(const Device& device) {
        String path = String(storageDir) + "/" + device.uuid + ".json";
        File file = LittleFS.open(path, "w");
        if (!file) {
            LOG_ERROR("[DeviceManager] Failed to open %s for write (path len=%u, "
                      "LFS_NAME_MAX cap is 31 chars per segment)",
                      path.c_str(), (unsigned)(device.uuid.length() + 5));
            return false;
        }
        JsonDocument doc;
        doc["uuid"] = device.uuid;
        doc["alexaSlot"] = device.alexaSlot;
        doc["type"] = device.type;
        doc["name"] = device.name;
        doc["protocolName"] = device.protocolName;
        doc["onCommand"] = device.onCommand.toJson();
        doc["offCommand"] = device.offCommand.toJson();
        size_t written = serializeJson(doc, file);
        file.close();
        if (written == 0) {
            LOG_ERROR("[DeviceManager] serializeJson wrote 0 bytes for %s — disk full?",
                      device.uuid.c_str());
            LittleFS.remove(path);  // don't leave a zero-byte ghost
            return false;
        }

        if (onDeviceAdded) {
            LOG_DEBUG("[DeviceManager] Triggering onDeviceAdded callback for device %s",
                      device.uuid.c_str());
            onDeviceAdded(device);
        }

        LOG_INFO("[DeviceManager] Device saved to %s (%u bytes)", path.c_str(),
                 (unsigned)written);
        return true;
    }

    bool removeDeviceByUuid(const String& uuid) {
        auto device = getDeviceByUuid(uuid);
        if (device) {
            return removeDevice(*device);
        }
        return false;
    }

    bool removeDevice(const Device& device) {
        Device snapshot = device;

        String filename = snapshot.uuid + ".json";
        bool success = LittleFS.remove(String(storageDir) + "/" + filename);
        if (success) {
            LOG_INFO("[DeviceManager] Device removed from %s", filename.c_str());
            if (onDeviceRemoved) {
                LOG_DEBUG("[DeviceManager] Triggering onDeviceRemoved callback for device %s",
                          snapshot.uuid.c_str());
                onDeviceRemoved(snapshot);
            }
        } else {
            LOG_ERROR("[DeviceManager] Failed to remove device from %s", filename.c_str());
        }
        return success;
    }

    std::optional<Device> getDeviceByUuid(const String& uuid) {
        return loadDeviceFromDisk(uuid);
    }

    std::optional<Device> getDeviceByName(const String& name) {
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            String uuid = filename.substring(0, filename.lastIndexOf('.'));
            auto device = loadDeviceFromDisk(uuid);
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
            String uuid = filename.substring(0, filename.lastIndexOf('.'));
            if (auto device = loadDeviceFromDisk(uuid)) {
                devices.push_back(std::move(*device));
            }
        }
        LOG_INFO("[DeviceManager] Loaded %d devices", devices.size());
        return devices;
    }

    template <typename Fn>
    void forEachDevice(Fn fn) {
        Dir dir = LittleFS.openDir(storageDir);
        while (dir.next()) {
            if (!dir.isFile()) continue;
            String filename = String(dir.fileName());
            if (!filename.endsWith(".json")) continue;
            String uuid = filename.substring(0, filename.lastIndexOf('.'));
            if (auto device = loadDeviceFromDisk(uuid)) {
                fn(*device);
            }
        }
    }

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
    std::optional<Device> loadDeviceFromDisk(const String& uuid) {
        String filename = uuid + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "r");
        if (!file) {
            LOG_ERROR("[DeviceManager] Failed to open file for device %s", uuid.c_str());
            return std::nullopt;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        if (err) {
            LOG_ERROR("[DeviceManager] Failed to parse JSON for device %s: %s", uuid.c_str(),
                      err.c_str());
            return std::nullopt;
        }

        const char* storedUuid = doc["uuid"] | (const char*)nullptr;  // skip legacy <int>.json
        int storedSlot = doc["alexaSlot"] | 0;
        if (storedUuid == nullptr || storedSlot < 1 || storedSlot > 255) {
            LOG_WARN("[DeviceManager] Skipping legacy device file %s (no uuid/alexaSlot) — "
                     "re-add this device to migrate to the UUID schema",
                     filename.c_str());
            return std::nullopt;
        }

        Device device;
        device.uuid = String(storedUuid);
        device.alexaSlot = (uint8_t)storedSlot;
        device.type = (DeviceType)doc["type"].as<int>();
        device.name = doc["name"] | "";
        device.protocolName = doc["protocolName"] | "";
        device.onCommand = IRCode::fromJson(doc["onCommand"]);
        device.offCommand = IRCode::fromJson(doc["offCommand"]);
        return device;
    }
};
