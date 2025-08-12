#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "IRManager.h"
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
    IRCode onCommand;
    IRCode offCommand;
};

class DeviceManager {
   private:
    const char* storageDir = "/devices";
    IRManager& irManager;
    IdGen& idGen;

   public:
    DeviceManager(IRManager& irManager, IdGen& idGen) : irManager(irManager), idGen(idGen) {}
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

    void setStorageDir(char* dir) { storageDir = dir; }

    int addSingleCommandDevice(IRCode command) {
        Device device;
        device.id = idGen.generateId();
        device.name = String(device.id);
        device.type = SINGLE_COMMAND;
        device.onCommand = command;
        device.offCommand = command;

        saveDevice(device);

        return device.id;
    }

    int addDualCommandDevice(IRCode onCommand, IRCode offCommand) {
        Device device;
        device.id = idGen.generateId();
        device.name = String(device.id);
        device.type = DUAL_COMMAND;
        device.onCommand = onCommand;
        device.offCommand = offCommand;
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
        JsonDocument onCommandDoc;
        irManager.convertIRCodeToJson(device.onCommand, onCommandDoc);
        doc["onCommand"] = onCommandDoc;
        JsonDocument offCommandDoc;
        irManager.convertIRCodeToJson(device.offCommand, offCommandDoc);
        doc["offCommand"] = offCommandDoc;
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

        device.onCommand = irManager.convertJsonToIRCode(doc["onCommand"]);
        device.offCommand = irManager.convertJsonToIRCode(doc["offCommand"]);
        return device;
    }
};