#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "Log.h"

class IdGen {
   private:
    const char* storageFile = "/id_gen.txt";
    int nextId = 0;

    void storeNextId() {
        LOG_DEBUG("Storing nextId: %d to file: %s", nextId, storageFile);
        File file = LittleFS.open(storageFile, "w");
        if (!file) {
            LOG_ERROR("Failed to open file for writing: %s", storageFile);
            return;
        }
        file.println(nextId);
        file.close();
        LOG_INFO("Successfully stored nextId: %d", nextId);
    }

    void loadNextId() {
        LOG_DEBUG("Loading nextId from file: %s", storageFile);
        File file = LittleFS.open(storageFile, "r");
        if (!file) {
            LOG_WARN("File not found, initializing with nextId: 0");
            nextId = 0;
            storeNextId();
            return;
        }

        try {
            int previousId = file.parseInt();
            nextId = previousId + 1;
            LOG_INFO("Loaded previousId: %d, setting nextId: %d", previousId, nextId);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse nextId from file: %s", storageFile);
            nextId = 0;
            storeNextId();
        }
        file.close();
    }

   public:
    IdGen() { LOG_DEBUG("IdGen constructor called"); }

    ~IdGen() { LOG_DEBUG("IdGen destructor called"); }

    bool begin() {
        LOG_INFO("Initializing IdGen with default storage file");
        loadNextId();
        return true;
    }

    bool begin(const char* storageFile) {
        LOG_INFO("Initializing IdGen with custom storage file: %s", storageFile);
        this->storageFile = storageFile;
        loadNextId();
        return true;
    }

    int generateId() {
        int id = nextId;
        nextId++;
        LOG_INFO("Generated new ID: %d, nextId now: %d", id, nextId);
        storeNextId();
        return id;
    }
};