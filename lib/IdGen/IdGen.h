#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "Log.h"

class IdGen {
   private:
    const char* storageFile = "/id_gen.txt";
    int nextId = 0;
    bool initialized = false;

    bool storeNextId() {
        LOG_DEBUG("[IdGen] Storing nextId: %d to file: %s", nextId, storageFile);

        if (!LittleFS.begin()) {
            LOG_ERROR("[IdGen] Failed to initialize LittleFS for storing nextId");
            return false;
        }

        File file = LittleFS.open(storageFile, "w");
        if (!file) {
            LOG_ERROR("[IdGen] Failed to open file for writing: %s", storageFile);
            return false;
        }

        bool success = file.println(nextId);
        file.close();

        if (success) {
            LOG_INFO("[IdGen] Successfully stored nextId: %d", nextId);
        } else {
            LOG_ERROR("[IdGen] Failed to write nextId to file");
        }

        return success;
    }

    bool loadNextId() {
        LOG_DEBUG("[IdGen] Loading nextId from file: %s", storageFile);

        if (!LittleFS.begin()) {
            LOG_ERROR("[IdGen] Failed to initialize LittleFS for loading nextId");
            nextId = 0;
            return false;
        }

        if (!LittleFS.exists(storageFile)) {
            LOG_WARN("[IdGen] Storage file not found, initializing with nextId: 0");
            nextId = 0;
            return storeNextId();
        }

        File file = LittleFS.open(storageFile, "r");
        if (!file) {
            LOG_ERROR("[IdGen] Failed to open file for reading: %s", storageFile);
            nextId = 0;
            return false;
        }

        // Read the file content as string first to check if it's valid
        String content = file.readString();
        file.close();

        // Trim whitespace and newlines
        content.trim();

        // Try to parse the content
        if (content.length() > 0) {
            int storedId = content.toInt();
            if (storedId >= 0) {
                nextId = storedId;
                LOG_INFO("[IdGen] Loaded stored nextId: %d", nextId);
                return true;
            } else {
                LOG_ERROR("[IdGen] Invalid negative ID found in file: %d", storedId);
            }
        } else {
            LOG_ERROR("[IdGen] Empty file content found");
        }

        // If parsing failed or content was invalid, reset to 0
        LOG_ERROR("[IdGen] Failed to parse nextId from file: %s, resetting to 0", storageFile);
        nextId = 0;
        return storeNextId();
    }

    void resetToZero() {
        LOG_WARN("[IdGen] Resetting ID generator to start from 0");
        nextId = 0;
        storeNextId();
    }

   public:
    IdGen() { LOG_DEBUG("[IdGen] Constructor called"); }

    ~IdGen() { LOG_DEBUG("[IdGen] Destructor called"); }

    bool begin() {
        LOG_INFO("[IdGen] Initializing with default storage file");
        bool success = loadNextId();
        initialized = success;
        return success;
    }

    bool begin(const char* customStorageFile) {
        LOG_INFO("[IdGen] Initializing with custom storage file: %s", customStorageFile);
        this->storageFile = customStorageFile;
        bool success = loadNextId();
        initialized = success;
        return success;
    }

    int generateId() {
        if (!initialized) {
            LOG_ERROR("[IdGen] Not initialized, cannot generate ID");
            return -1;
        }

        int id = nextId;
        nextId++;

        LOG_INFO("[IdGen] Generated new ID: %d, nextId now: %d", id, nextId);

        // Store the updated nextId
        if (!storeNextId()) {
            LOG_ERROR("[IdGen] Failed to store updated nextId after generating ID: %d", id);
        }

        return id;
    }

    int getNextId() const { return nextId; }

    bool reset() {
        resetToZero();
        return true;
    }

    bool isInitialized() const { return initialized; }
};