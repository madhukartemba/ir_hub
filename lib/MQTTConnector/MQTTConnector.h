#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"
#include "MqttCredentials.h"

class MQTTConnector {
   private:
    static MQTTConnector* instance;

    DeviceManager& deviceManager;
    IRManager& irManager;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    /// True when MQTT is permitted to run: Wi-Fi is up *and* a broker host
    /// has been configured (either in secrets.h or via the captive portal).
    bool enabled;
    /// Lowercase 12-char hex STA MAC — used in MQTT topics so multiple hubs do not collide.
    String hubMacHex;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long kReconnectIntervalMs = 5000;

    std::function<void(const Device& device, bool state)> onStateChangeCallback;

    static void staticCallback(char* topic, byte* payload, unsigned int length) {
        if (instance) {
            instance->handleIncomingMessage(topic, payload, length);
        }
    }

    static String buildStaMacHex() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char buf[13];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
                 mac[5]);
        return String(buf);
    }

    String discoveryTopicForId(int deviceId) const {
        return String("homeassistant/switch/ir_hub_") + hubMacHex + "_device_" + String(deviceId) +
               "/config";
    }

    String commandTopicForId(int deviceId) const {
        return String("ir_hub/") + hubMacHex + "/device/" + String(deviceId) + "/set";
    }

    String stateTopicForId(int deviceId) const {
        return String("ir_hub/") + hubMacHex + "/device/" + String(deviceId) + "/state";
    }

    bool publishDiscovery(const Device& device) {
        JsonDocument doc;
        doc["name"] = device.name;
        doc["command_topic"] = commandTopicForId(device.id);
        doc["state_topic"] = stateTopicForId(device.id);
        doc["unique_id"] = String("ir_hub_") + hubMacHex + "_" + String(device.id);
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["optimistic"] = false;

        JsonObject dev = doc["device"].to<JsonObject>();
        JsonArray ids = dev["identifiers"].to<JsonArray>();
        ids.add(String("ir_hub_") + hubMacHex);
        dev["name"] = "IR Hub";
        dev["model"] = "ESP8266 IR Hub";
        dev["manufacturer"] = "IR Hub";

        char buf[768];
        size_t n = serializeJson(doc, buf, sizeof(buf));
        if (n >= sizeof(buf)) {
            LOG_ERROR("[MQTT] Discovery JSON too large for device %d", device.id);
            return false;
        }

        const String topic = discoveryTopicForId(device.id);
        bool ok = mqttClient.publish(topic.c_str(), buf, true);
        if (!ok) {
            LOG_ERROR("[MQTT] Failed to publish discovery for device %d", device.id);
        }
        return ok;
    }

    bool publishState(int deviceId, bool on) {
        const char* payload = on ? "ON" : "OFF";
        return mqttClient.publish(stateTopicForId(deviceId).c_str(), payload, true);
    }

    bool publishDiscoveryRemove(int deviceId) {
        // Empty retained payload removes MQTT discovery entities in Home Assistant
        return mqttClient.publish(discoveryTopicForId(deviceId).c_str(), "", true);
    }

    bool subscribeCommands() {
        const String sub = String("ir_hub/") + hubMacHex + "/device/+/set";
        bool ok = mqttClient.subscribe(sub.c_str());
        if (!ok) {
            LOG_ERROR("[MQTT] Failed to subscribe to command topics");
        }
        return ok;
    }

    void applyCommandForDevice(int deviceId, bool turnOn) {
        Device* device = deviceManager.getDeviceById(deviceId);
        if (!device) {
            LOG_ERROR("[MQTT] Unknown device id %d", deviceId);
            return;
        }

        if (onStateChangeCallback) {
            onStateChangeCallback(*device, turnOn);
        }

        if (turnOn) {
            irManager.sendProtocol(device->onCommand);
            LOG_INFO("[MQTT] ON device id %d (%s)", deviceId, device->name.c_str());
        } else {
            irManager.sendProtocol(device->offCommand);
            LOG_INFO("[MQTT] OFF device id %d (%s)", deviceId, device->name.c_str());
        }

        publishState(deviceId, turnOn);
    }

    void handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
        String t(topic);
        const String prefix = String("ir_hub/") + hubMacHex + "/device/";
        if (!t.startsWith(prefix) || !t.endsWith("/set")) {
            return;
        }

        String rest = t.substring(prefix.length());
        int slash = rest.indexOf('/');
        if (slash <= 0) {
            return;
        }

        int deviceId = rest.substring(0, slash).toInt();
        if (deviceId < 0) {
            return;
        }

        char cmd[12];
        if (length >= sizeof(cmd)) {
            length = sizeof(cmd) - 1;
        }
        memcpy(cmd, payload, length);
        cmd[length] = '\0';

        String c(cmd);
        c.trim();
        c.toUpperCase();

        if (c == "ON") {
            applyCommandForDevice(deviceId, true);
        } else if (c == "OFF") {
            applyCommandForDevice(deviceId, false);
        } else {
            LOG_WARN("[MQTT] Unknown payload for %s: %s", topic, cmd);
        }
    }

    bool connectBroker() {
        String clientId = String("ir_hub_") + hubMacHex;
        // Pass nullptr for empty credentials so PubSubClient connects
        // anonymously instead of sending empty username/password frames.
        const char* user = mqttCredentialsUser();
        const char* pass = mqttCredentialsPass();
        const char* userArg = (user && *user) ? user : nullptr;
        const char* passArg = (pass && *pass) ? pass : nullptr;
        if (mqttClient.connect(clientId.c_str(), userArg, passArg)) {
            LOG_INFO("[MQTT] Connected to broker");
            subscribeCommands();
            for (Device& d : deviceManager.getDevices()) {
                publishDiscovery(d);
                publishState(d.id, false);
            }
            return true;
        }

        LOG_WARN("[MQTT] Connect failed, rc=%d", mqttClient.state());
        return false;
    }

    void tryReconnect() {
        if (WiFi.status() != WL_CONNECTED) {
            return;
        }

        unsigned long now = millis();
        if (now - lastReconnectAttempt < kReconnectIntervalMs) {
            return;
        }
        lastReconnectAttempt = now;

        LOG_INFO("[MQTT] Reconnecting...");
        if (connectBroker()) {
            lastReconnectAttempt = 0;
        }
    }

   public:
    MQTTConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager),
          irManager(irManager),
          mqttClient(wifiClient),
          enabled(false),
          lastReconnectAttempt(0) {
        mqttClient.setBufferSize(1024);
    }

    ~MQTTConnector() {
        if (instance == this) {
            instance = nullptr;
        }
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void begin() {
        instance = this;
        mqttCredentialsLoad();

        if (WiFi.status() != WL_CONNECTED) {
            LOG_INFO("[MQTT] WiFi not connected, MQTT disabled");
            enabled = false;
            return;
        }

        if (!mqttCredentialsConfigured()) {
            LOG_INFO("[MQTT] No broker configured; MQTT disabled. "
                     "Set host via the Wi-Fi captive portal to enable.");
            enabled = false;
            return;
        }

        mqttClient.setServer(mqttCredentialsHost(), mqttCredentialsPort());
        mqttClient.setCallback(staticCallback);
        enabled = true;

        hubMacHex = buildStaMacHex();
        LOG_INFO("[MQTT] Broker %s:%u, topic ns ir_hub/%s/device/...",
                 mqttCredentialsHost(), (unsigned)mqttCredentialsPort(),
                 hubMacHex.c_str());

        // Add/remove callbacks are set in main.cpp so Alexa and MQTT both receive updates.

        if (connectBroker()) {
            LOG_INFO("[MQTT] Home Assistant MQTT enabled");
        } else {
            LOG_WARN("[MQTT] Initial broker connection failed; will retry in loop()");
        }
    }

    void registerDevice(const Device& device) {
        if (!enabled) {
            return;
        }
        if (mqttClient.connected()) {
            publishDiscovery(device);
            publishState(device.id, false);
        }
    }

    void unregisterDevice(const Device& device) {
        if (!enabled) {
            return;
        }
        if (mqttClient.connected()) {
            publishDiscoveryRemove(device.id);
        }
    }

    void update() {
        if (!enabled) {
            return;
        }
        if (!mqttClient.connected()) {
            tryReconnect();
            return;
        }
        mqttClient.loop();
    }

    bool isEnabled() const { return enabled; }
};
