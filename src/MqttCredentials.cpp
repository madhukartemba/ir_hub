#include "MqttCredentials.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Log.h"
#include "secrets.h"

static constexpr const char* kMqttCredsPath = "/mqtt.json";
static constexpr uint16_t kDefaultPort = 1883;

static String g_host;
static uint16_t g_port = kDefaultPort;
static String g_user;
static String g_pass;

void mqttCredentialsLoad() {
    g_host = MQTT_HOST;
    g_port = MQTT_PORT ? (uint16_t)MQTT_PORT : kDefaultPort;
    g_user = MQTT_USER;
    g_pass = MQTT_PASSWORD;

    if (!LittleFS.exists(kMqttCredsPath)) {
        return;
    }

    File f = LittleFS.open(kMqttCredsPath, "r");
    if (!f) {
        LOG_WARN("[MQTT] Could not open %s", kMqttCredsPath);
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_WARN("[MQTT] Bad JSON in %s: %s", kMqttCredsPath, err.c_str());
        return;
    }

    if (!doc["host"].isNull()) {
        const char* h = doc["host"].as<const char*>();
        if (h) {
            g_host = h;
        }
    }
    if (!doc["port"].isNull()) {
        long p = doc["port"].as<long>();
        if (p > 0 && p <= 65535) {
            g_port = (uint16_t)p;
        }
    }
    if (!doc["user"].isNull()) {
        const char* u = doc["user"].as<const char*>();
        if (u) {
            g_user = u;
        }
    }
    if (!doc["pass"].isNull()) {
        const char* p = doc["pass"].as<const char*>();
        if (p) {
            g_pass = p;
        }
    }
}

const char* mqttCredentialsHost() { return g_host.c_str(); }

uint16_t mqttCredentialsPort() { return g_port ? g_port : kDefaultPort; }

const char* mqttCredentialsUser() { return g_user.c_str(); }

const char* mqttCredentialsPass() { return g_pass.c_str(); }

bool mqttCredentialsConfigured() { return g_host.length() > 0; }

bool mqttCredentialsSave(const char* host, uint16_t port, const char* user, const char* pass) {
    JsonDocument doc;
    doc["host"] = host ? host : "";
    doc["port"] = port ? port : kDefaultPort;
    doc["user"] = user ? user : "";
    doc["pass"] = pass ? pass : "";

    File f = LittleFS.open(kMqttCredsPath, "w");
    if (!f) {
        LOG_ERROR("[MQTT] Failed to write %s", kMqttCredsPath);
        return false;
    }

    if (serializeJson(doc, f) == 0) {
        f.close();
        LOG_ERROR("[MQTT] Failed to serialize credentials to %s", kMqttCredsPath);
        return false;
    }
    f.close();

    mqttCredentialsLoad();
    LOG_INFO("[MQTT] Saved credentials to LittleFS (host=%s, port=%u, user=%s)",
             mqttCredentialsHost(), (unsigned)mqttCredentialsPort(),
             mqttCredentialsUser());
    return true;
}

void mqttCredentialsRemove() {
    if (LittleFS.exists(kMqttCredsPath)) {
        LittleFS.remove(kMqttCredsPath);
        LOG_INFO("[MQTT] Removed %s", kMqttCredsPath);
    }
    mqttCredentialsLoad();
}
