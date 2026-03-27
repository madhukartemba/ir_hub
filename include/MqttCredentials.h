#pragma once

#include <Arduino.h>

/// Load credentials from LittleFS `/mqtt.json` when present; otherwise use `secrets.h` defaults.
void mqttCredentialsLoad();

const char* mqttCredentialsUser();
const char* mqttCredentialsPass();

/// Persist MQTT username/password to LittleFS (called from Wi‑Fi portal save).
bool mqttCredentialsSave(const char* user, const char* pass);

/// Remove saved MQTT credentials file (e.g. when resetting Wi‑Fi).
void mqttCredentialsRemove();
