#pragma once

#include <Arduino.h>
#include <stdint.h>

/// Load credentials from LittleFS `/mqtt.json` when present; otherwise fall
/// back to the compile-time defaults in `secrets.h`.
void mqttCredentialsLoad();

const char* mqttCredentialsHost();
uint16_t mqttCredentialsPort();
const char* mqttCredentialsUser();
const char* mqttCredentialsPass();

/// True iff a broker host is configured. An empty host means the user has not
/// set up MQTT yet; callers should disable MQTT in that case.
bool mqttCredentialsConfigured();

/// Persist MQTT settings to LittleFS (called from the Wi-Fi portal save).
/// Pass nullptr or "" for any field that is not set; port==0 falls back to 1883.
bool mqttCredentialsSave(const char* host, uint16_t port, const char* user, const char* pass);

/// Remove saved MQTT credentials file (e.g. when resetting Wi-Fi).
void mqttCredentialsRemove();
