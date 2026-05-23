#pragma once

/// Runtime user preferences persisted to `/prefs.json` on LittleFS.
///
/// Call `userPrefsLoad()` once after LittleFS is mounted. All getters return
/// the in-memory copy and are cheap. Setters write through to flash.

void userPrefsLoad();

/// Whether the speaker should make any sound. Default: true.
bool userPrefsSoundEnabled();
void userPrefsSetSoundEnabled(bool enabled);
