#include "UserPrefs.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Log.h"

static constexpr const char* kPrefsPath = "/prefs.json";

static bool g_soundEnabled = true;
static bool g_hapticsEnabled = true;
static bool g_skipWiFiSetup = false;
static bool g_alexaEnabled = true;

static void writeBack() {
    JsonDocument doc;
    doc["sound"] = g_soundEnabled;
    doc["haptics"] = g_hapticsEnabled;
    doc["skip_wifi_setup"] = g_skipWiFiSetup;
    doc["alexa_enabled"] = g_alexaEnabled;

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
    g_hapticsEnabled = true;
    g_skipWiFiSetup = false;
    g_alexaEnabled = true;

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
    if (!doc["haptics"].isNull()) {
        g_hapticsEnabled = doc["haptics"].as<bool>();
    }
    if (!doc["skip_wifi_setup"].isNull()) {
        g_skipWiFiSetup = doc["skip_wifi_setup"].as<bool>();
    }
    if (!doc["alexa_enabled"].isNull()) {
        g_alexaEnabled = doc["alexa_enabled"].as<bool>();
    }
    LOG_INFO("[Prefs] Loaded: sound=%s haptics=%s alexa=%s", g_soundEnabled ? "on" : "off",
             g_hapticsEnabled ? "on" : "off", g_alexaEnabled ? "on" : "off");
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

bool userPrefsHapticsEnabled() { return g_hapticsEnabled; }

void userPrefsSetHapticsEnabled(bool enabled) {
    if (g_hapticsEnabled == enabled) {
        return;
    }
    g_hapticsEnabled = enabled;
    writeBack();
    LOG_INFO("[Prefs] haptics=%s", g_hapticsEnabled ? "on" : "off");
}

bool userPrefsSkipWiFiSetup() { return g_skipWiFiSetup; }

void userPrefsSetSkipWiFiSetup(bool enabled) {
    if (g_skipWiFiSetup == enabled) {
        return;
    }
    g_skipWiFiSetup = enabled;
    writeBack();
    LOG_INFO("[Prefs] skip_wifi_setup=%s", g_skipWiFiSetup ? "on" : "off");
}

bool userPrefsAlexaEnabled() { return g_alexaEnabled; }

void userPrefsSetAlexaEnabled(bool enabled) {
    if (g_alexaEnabled == enabled) {
        return;
    }
    g_alexaEnabled = enabled;
    writeBack();
    LOG_INFO("[Prefs] alexa_enabled=%s", g_alexaEnabled ? "on" : "off");
}
