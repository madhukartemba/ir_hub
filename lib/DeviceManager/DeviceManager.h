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
    /// 128-bit random primary key, rendered as 32 lowercase hex chars
    /// ("uuid" in JSON). Generated once at creation and never reused or
    /// mutated, so it's safe to use as the file name, MQTT topic segment,
    /// and lookup key. Collision probability across realistic
    /// deployments is ~10^-37 (4× os_random() + micros() + cycle-count).
    String uuid;
    /// 1..255 — the 2-hex Hue `uniqueid` endpoint we ship to Alexa.
    /// The Hue uniqueid format pins us to 8 bits here (Alexa was observed
    /// rejecting 4-hex endpoints), so this is allocated as
    /// "lowest unused slot on this hub" at creation time rather than
    /// derived from the UUID (which would give birthday-bound collisions
    /// at ~10 devices in 256 slots). Persisted alongside the rest of the
    /// device so removing a device frees its slot for the next add.
    uint8_t alexaSlot;
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
/// Identity model (post-IdGen):
/// - `uuid` is a 32-hex-char random primary key. File name is
///   `/devices/<uuid>.json`. Used for UI lookups, MQTT topics, log lines.
/// - `alexaSlot` is a per-hub 1..255 byte assigned at creation time as
///   the lowest unused slot. It maps to the 2-hex Hue uniqueid endpoint
///   shipped to Alexa.
///
/// Lookup APIs return `std::optional<Device>` by value. Callers that need
/// to retain the Device must copy/move it; the returned object owns its
/// own strings + state vector.
class DeviceManager {
   public:
    using DeviceCallback = std::function<void(const Device&)>;

   private:
    const char* storageDir = "/devices";
    DeviceCallback onDeviceAdded;
    DeviceCallback onDeviceRemoved;

    /// Generate a fresh 128-bit identifier as a 32-char lowercase hex
    /// string. Sources four uint32_t from the ESP8266 hardware RNG
    /// (`os_random()`, fed by WiFi RF noise) and XORs two of them with
    /// `micros()` / cycle count so that even if the RNG isn't fully
    /// primed at boot (pre-WiFi) we still get unpredictable bits from
    /// the CPU's own timing. At 128 bits the birthday collision bound
    /// for any realistic deployment is on the order of 10^-30, so we
    /// don't bother with a disk-collision check.
    static String generateUuid() {
        uint32_t a = os_random();
        uint32_t b = os_random() ^ (uint32_t)micros();
        uint32_t c = os_random();
        uint32_t d = os_random() ^ ESP.getCycleCount();
        char buf[33];
        snprintf(buf, sizeof(buf), "%08x%08x%08x%08x", a, b, c, d);
        return String(buf);
    }

    /// Pick the smallest unused alexaSlot in 1..255 by scanning every
    /// stored device. O(N) over the device count; called only at
    /// creation time. Returns 0 if the (cosmically unlikely) full-slot
    /// state is hit — caller treats 0 as "no free slot".
    uint8_t allocateAlexaSlot() {
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
        for (int i = 1; i <= 255; i++) {
            if (!used[i]) return (uint8_t)i;
        }
        return 0;
    }

    /// Snapshot the live device count for friendly-name suffix generation
    /// ("VOLTAS 1", "VOLTAS 2"). Counts JSON files without parsing them.
    /// Number reuse after delete is fine — names are user-editable.
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

    /// Returns the new device's UUID on success, empty String on failure.
    String addSingleCommandDevice(IRCode command) {
        if (!command.isValid()) {
            LOG_ERROR("[DeviceManager] Invalid command");
            return String();
        }

        Device device;
        device.uuid = generateUuid();
        device.alexaSlot = allocateAlexaSlot();
        if (device.alexaSlot == 0) {
            LOG_ERROR("[DeviceManager] No free Alexa slot (255 in use?)");
            return String();
        }
        device.protocolName = String(typeToString(command.getProtocol(), false));
        device.name = device.protocolName + " " + String((unsigned)(countExistingDevices() + 1));
        device.type = SINGLE_COMMAND;
        device.onCommand = command;
        device.offCommand = command;
        saveDevice(device);

        return device.uuid;
    }

    /// Returns the new device's UUID on success, empty String on failure.
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
        device.alexaSlot = allocateAlexaSlot();
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
        saveDevice(device);
        return device.uuid;
    }

    void saveDevice(const Device& device) {
        String filename = device.uuid + ".json";
        File file = LittleFS.open(String(storageDir) + "/" + filename, "w");
        if (!file) {
            LOG_ERROR("[DeviceManager] Failed to open file");
            return;
        }
        JsonDocument doc;
        doc["uuid"] = device.uuid;
        doc["alexaSlot"] = device.alexaSlot;
        doc["type"] = device.type;
        doc["name"] = device.name;
        doc["protocolName"] = device.protocolName;
        doc["onCommand"] = device.onCommand.toJson();
        doc["offCommand"] = device.offCommand.toJson();
        serializeJson(doc, file);
        file.close();

        if (onDeviceAdded) {
            LOG_DEBUG("[DeviceManager] Triggering onDeviceAdded callback for device %s",
                      device.uuid.c_str());
            onDeviceAdded(device);
        }

        LOG_INFO("[DeviceManager] Device saved to %s", filename.c_str());
    }

    bool removeDeviceByUuid(const String& uuid) {
        auto device = getDeviceByUuid(uuid);
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

    /// Each call hits LittleFS — returns std::nullopt if the device doesn't
    /// exist. The returned Device owns its own data; the caller may keep it
    /// for as long as needed.
    std::optional<Device> getDeviceByUuid(const String& uuid) {
        return loadDeviceFromDisk(uuid);
    }

    /// O(N) directory scan + per-file parse. Used only by the Alexa command
    /// path, which is rare enough that the linear scan doesn't matter.
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
            String uuid = filename.substring(0, filename.lastIndexOf('.'));
            if (auto device = loadDeviceFromDisk(uuid)) {
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
    /// Stream-parses /devices/<uuid>.json. Returns std::nullopt on missing
    /// file, malformed JSON, or schema mismatch (e.g. legacy
    /// `<int>.json` files from before the UUID migration — those have no
    /// `uuid` field and are intentionally orphaned).
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

        // Legacy `<int>.json` files have no "uuid" field. Skip with a loud
        // log so the user knows their pre-UUID devices need re-adding.
        const char* storedUuid = doc["uuid"] | (const char*)nullptr;
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
