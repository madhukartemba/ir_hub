#pragma once

/// Runtime user preferences persisted to `/prefs.json` on LittleFS.
///
/// Call `userPrefsLoad()` once after LittleFS is mounted. All getters return
/// the in-memory copy and are cheap. Setters write through to flash.

void userPrefsLoad();

/// Whether the speaker should make any sound. Default: true.
bool userPrefsSoundEnabled();
void userPrefsSetSoundEnabled(bool enabled);

/// Whether tactile feedback (DRV2605) should fire on button events.
/// Default: true. Ignored at runtime if the driver isn't present.
bool userPrefsHapticsEnabled();
void userPrefsSetHapticsEnabled(bool enabled);

/// If true, boot skips Wi-Fi connect/setup until the user explicitly re-enables it.
/// Default: false.
bool userPrefsSkipWiFiSetup();
void userPrefsSetSkipWiFiSetup(bool enabled);

/// Whether Alexa bridge emulation is enabled. Default: true.
/// When false, no Alexa sockets are opened and no SSDP/HTTP Hue traffic is served.
bool userPrefsAlexaEnabled();
void userPrefsSetAlexaEnabled(bool enabled);
