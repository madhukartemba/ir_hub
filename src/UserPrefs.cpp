#include "UserPrefs.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Log.h"

static constexpr const char* kPrefsPath = "/prefs.json";

// Cached state. We deliberately keep this very small (single struct, only
// POD fields) so the in-memory footprint and the on-disk file stay tiny.
static bool g_soundEnabled = true;

static void writeBack() {
    JsonDocument doc;
    doc["sound"] = g_soundEnabled;

    File f = LittleFS.open(kPrefsPath, "w");
    if (!f) {
        LOG_ERROR("[Prefs] Failed to open %s for write", kPrefsPath);
        return;
    }
    if (serializeJson(doc, f) == 0) {
        LOG_ERROR("[Prefs] Failed to serialize prefs");
    }
    f.close();
}

void userPrefsLoad() {
    g_soundEnabled = true;  // defaults

    if (!LittleFS.exists(kPrefsPath)) {
        LOG_INFO("[Prefs] No %s yet — using defaults", kPrefsPath);
        return;
    }

    File f = LittleFS.open(kPrefsPath, "r");
    if (!f) {
        LOG_WARN("[Prefs] Could not open %s", kPrefsPath);
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_WARN("[Prefs] Bad JSON in %s: %s — using defaults", kPrefsPath, err.c_str());
        return;
    }

    if (!doc["sound"].isNull()) {
        g_soundEnabled = doc["sound"].as<bool>();
    }
    LOG_INFO("[Prefs] Loaded: sound=%s", g_soundEnabled ? "on" : "off");
}

bool userPrefsSoundEnabled() { return g_soundEnabled; }

void userPrefsSetSoundEnabled(bool enabled) {
    if (g_soundEnabled == enabled) {
        return;
    }
    g_soundEnabled = enabled;
    writeBack();
    LOG_INFO("[Prefs] sound=%s", g_soundEnabled ? "on" : "off");
}
