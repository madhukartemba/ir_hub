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

#ifndef FIRMWARE_VERSION
#  define FIRMWARE_VERSION "0.0.0"
#endif

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
    static constexpr unsigned long kInfoPublishIntervalMs = 60000;
    unsigned long lastInfoPublishMs;

    std::function<void(const Device& device, bool state)> onStateChangeCallback;
    std::function<void()> onOtaCheckCallback;

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

    // Topic builders write into a caller-provided stack buffer to keep the
    // MQTT hot-paths heap-free. Returns the number of bytes that *would*
    // have been written (snprintf semantics); callers can ignore unless
    // checking for truncation.
    static constexpr size_t kTopicBufSize = 80;

    size_t discoveryTopicForId(int deviceId, char* out, size_t outSize) const {
        return snprintf(out, outSize,
                        "homeassistant/switch/ir_hub_%s_device_%d/config",
                        hubMacHex.c_str(), deviceId);
    }

    size_t commandTopicForId(int deviceId, char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/device/%d/set",
                        hubMacHex.c_str(), deviceId);
    }

    size_t stateTopicForId(int deviceId, char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/device/%d/state",
                        hubMacHex.c_str(), deviceId);
    }

    size_t otaCheckTopic(char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/ota/check", hubMacHex.c_str());
    }

    size_t infoTopic(char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/info", hubMacHex.c_str());
    }

    size_t infoDiscoveryTopic(const char* key, char* out, size_t outSize) const {
        return snprintf(out, outSize, "homeassistant/sensor/ir_hub_%s_%s/config",
                        hubMacHex.c_str(), key);
    }

    bool publishInfoDiscoverySensor(const char* key, const char* name, const char* valueTemplate,
                                    const char* unit = nullptr, const char* deviceClass = nullptr,
                                    const char* icon = nullptr) {
        char stateTopic[kTopicBufSize];
        infoTopic(stateTopic, sizeof(stateTopic));

        char uniqueId[56];
        snprintf(uniqueId, sizeof(uniqueId), "ir_hub_%s_%s", hubMacHex.c_str(), key);
        char identifier[24];
        snprintf(identifier, sizeof(identifier), "ir_hub_%s", hubMacHex.c_str());

        JsonDocument doc;
        doc["name"] = name;
        doc["state_topic"] = stateTopic;
        doc["value_template"] = valueTemplate;
        doc["unique_id"] = uniqueId;
        doc["entity_category"] = "diagnostic";
        if (unit && *unit) {
            doc["unit_of_measurement"] = unit;
        }
        if (deviceClass && *deviceClass) {
            doc["device_class"] = deviceClass;
        }
        if (icon && *icon) {
            doc["icon"] = icon;
        }

        JsonObject dev = doc["device"].to<JsonObject>();
        JsonArray ids = dev["identifiers"].to<JsonArray>();
        ids.add(identifier);
        dev["name"] = "IR Hub";
        dev["model"] = "ESP8266 IR Hub";
        dev["manufacturer"] = "IR Hub";

        char payload[560];
        size_t n = serializeJson(doc, payload, sizeof(payload));
        if (n >= sizeof(payload)) {
            LOG_ERROR("[MQTT] Info discovery JSON too large for %s", key);
            return false;
        }

        char topic[kTopicBufSize];
        infoDiscoveryTopic(key, topic, sizeof(topic));
        if (!mqttClient.publish(topic, payload, true)) {
            LOG_WARN("[MQTT] Failed to publish info discovery for %s", key);
            return false;
        }
        return true;
    }

    void publishHubInfoDiscovery() {
        publishInfoDiscoverySensor("firmware", "Firmware Version", "{{ value_json.firmware }}");
        publishInfoDiscoverySensor("ip", "IP Address", "{{ value_json.ip }}");
        publishInfoDiscoverySensor("ssid", "WiFi SSID", "{{ value_json.ssid }}");
        publishInfoDiscoverySensor("rssi", "WiFi RSSI", "{{ value_json.rssi }}", "dBm",
                                   "signal_strength");
        publishInfoDiscoverySensor("uptime", "Uptime", "{{ value_json.uptime_s }}", "s", "duration");
        publishInfoDiscoverySensor("free_heap", "Free Heap", "{{ value_json.free_heap }}", "B");
        publishInfoDiscoverySensor("max_block", "Max Heap Block", "{{ value_json.max_block }}", "B");
        publishInfoDiscoverySensor("heap_frag", "Heap Fragmentation",
                                   "{{ value_json.heap_frag }}", "%", nullptr,
                                   "mdi:memory");
    }

    bool publishHubInfo() {
        if (!mqttClient.connected()) {
            return false;
        }

        JsonDocument doc;
        doc["firmware"] = FIRMWARE_VERSION;
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = millis() / 1000UL;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["max_block"] = ESP.getMaxFreeBlockSize();
        doc["heap_frag"] = ESP.getHeapFragmentation();

        char payload[320];
        size_t n = serializeJson(doc, payload, sizeof(payload));
        if (n >= sizeof(payload)) {
            LOG_ERROR("[MQTT] Hub info JSON too large");
            return false;
        }

        char topic[kTopicBufSize];
        infoTopic(topic, sizeof(topic));
        bool ok = mqttClient.publish(topic, payload, true);
        if (!ok) {
            LOG_WARN("[MQTT] Failed to publish hub info");
        }
        return ok;
    }

    bool publishDiscovery(const Device& device) {
        // Stack-build all topic / id strings the discovery JSON references.
        // ArduinoJson v7 stores const char* by reference (no copy), so these
        // buffers must outlive serializeJson — which they do, both live in
        // this function's scope.
        char cmdTopic[kTopicBufSize];
        char stTopic[kTopicBufSize];
        char uniqueId[40];   // "ir_hub_" + 12 mac hex + "_" + 10 digits + null
        char identifier[24]; // "ir_hub_" + 12 mac hex + null
        commandTopicForId(device.id, cmdTopic, sizeof(cmdTopic));
        stateTopicForId(device.id, stTopic, sizeof(stTopic));
        snprintf(uniqueId, sizeof(uniqueId), "ir_hub_%s_%d", hubMacHex.c_str(), device.id);
        snprintf(identifier, sizeof(identifier), "ir_hub_%s", hubMacHex.c_str());

        JsonDocument doc;
        doc["name"] = device.name;
        doc["command_topic"] = cmdTopic;
        doc["state_topic"] = stTopic;
        doc["unique_id"] = uniqueId;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["optimistic"] = false;

        JsonObject dev = doc["device"].to<JsonObject>();
        JsonArray ids = dev["identifiers"].to<JsonArray>();
        ids.add(identifier);
        dev["name"] = "IR Hub";
        dev["model"] = "ESP8266 IR Hub";
        dev["manufacturer"] = "IR Hub";

        char buf[768];
        size_t n = serializeJson(doc, buf, sizeof(buf));
        if (n >= sizeof(buf)) {
            LOG_ERROR("[MQTT] Discovery JSON too large for device %d", device.id);
            return false;
        }

        char topic[kTopicBufSize];
        discoveryTopicForId(device.id, topic, sizeof(topic));
        bool ok = mqttClient.publish(topic, buf, true);
        if (!ok) {
            LOG_ERROR("[MQTT] Failed to publish discovery for device %d", device.id);
        }
        return ok;
    }

    bool publishState(int deviceId, bool on) {
        const char* payload = on ? "ON" : "OFF";
        char topic[kTopicBufSize];
        stateTopicForId(deviceId, topic, sizeof(topic));
        return mqttClient.publish(topic, payload, true);
    }

    bool publishDiscoveryRemove(int deviceId) {
        // Empty retained payload removes MQTT discovery entities in Home Assistant
        char topic[kTopicBufSize];
        discoveryTopicForId(deviceId, topic, sizeof(topic));
        return mqttClient.publish(topic, "", true);
    }

    bool subscribeCommands() {
        char sub[kTopicBufSize];
        snprintf(sub, sizeof(sub), "ir_hub/%s/device/+/set", hubMacHex.c_str());
        bool ok = mqttClient.subscribe(sub);
        if (!ok) {
            LOG_ERROR("[MQTT] Failed to subscribe to command topics");
        }
        char ota[kTopicBufSize];
        otaCheckTopic(ota, sizeof(ota));
        if (!mqttClient.subscribe(ota)) {
            LOG_WARN("[MQTT] Failed to subscribe to OTA check topic");
        }
        return ok;
    }

    void applyCommandForDevice(int deviceId, bool turnOn) {
        // DeviceManager is uncached: this hits LittleFS + JSON parse (~5–15 ms).
        // Acceptable on the MQTT command path.
        auto device = deviceManager.getDeviceById(deviceId);
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
        // Fast-path the (rare) OTA check command before parsing the device
        // topic shape; topic strings are interned by PubSubClient on the stack.
        const size_t topicLen = strlen(topic);
        char otaTopic[kTopicBufSize];
        const size_t otaLen = otaCheckTopic(otaTopic, sizeof(otaTopic));
        if (topicLen == otaLen && memcmp(topic, otaTopic, topicLen) == 0) {
            LOG_INFO("[MQTT] OTA check requested via MQTT");
            if (onOtaCheckCallback) onOtaCheckCallback();
            return;
        }

        // Topic shape is `ir_hub/<macHex>/device/<id>/set`. Pointer-arithmetic
        // parser so an incoming MQTT message costs zero heap.
        const size_t macLen = hubMacHex.length();
        const size_t kPrefixFixed = sizeof("ir_hub/") - 1;        // 7
        const size_t kMid = sizeof("/device/") - 1;               // 8
        const size_t kSuffix = sizeof("/set") - 1;                // 4

        if (topicLen < kPrefixFixed + macLen + kMid + 1 + kSuffix) {
            return;
        }
        if (memcmp(topic, "ir_hub/", kPrefixFixed) != 0) {
            return;
        }
        if (memcmp(topic + kPrefixFixed, hubMacHex.c_str(), macLen) != 0) {
            return;
        }
        if (memcmp(topic + kPrefixFixed + macLen, "/device/", kMid) != 0) {
            return;
        }
        if (memcmp(topic + topicLen - kSuffix, "/set", kSuffix) != 0) {
            return;
        }

        const char* idStart = topic + kPrefixFixed + macLen + kMid;
        const char* idEnd = topic + topicLen - kSuffix;
        if (idEnd <= idStart) {
            return;
        }

        char idBuf[12];
        size_t idLen = (size_t)(idEnd - idStart);
        if (idLen >= sizeof(idBuf)) {
            return;
        }
        memcpy(idBuf, idStart, idLen);
        idBuf[idLen] = '\0';

        char* endp = nullptr;
        long deviceId = strtol(idBuf, &endp, 10);
        if (!endp || *endp != '\0' || deviceId < 0 || deviceId > INT32_MAX) {
            return;
        }

        char cmd[8];
        if (length >= sizeof(cmd)) {
            length = sizeof(cmd) - 1;
        }
        for (unsigned int i = 0; i < length; i++) {
            char ch = (char)payload[i];
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
            cmd[i] = ch;
        }
        cmd[length] = '\0';

        if (strcmp(cmd, "ON") == 0) {
            applyCommandForDevice((int)deviceId, true);
        } else if (strcmp(cmd, "OFF") == 0) {
            applyCommandForDevice((int)deviceId, false);
        } else {
            LOG_WARN("[MQTT] Unknown payload for %s: %s", topic, cmd);
        }
    }

    bool connectBroker() {
        char clientId[24];  // "ir_hub_" (7) + 12 mac hex + null = 20
        snprintf(clientId, sizeof(clientId), "ir_hub_%s", hubMacHex.c_str());
        // Pass nullptr for empty credentials so PubSubClient connects
        // anonymously instead of sending empty username/password frames.
        const char* user = mqttCredentialsUser();
        const char* pass = mqttCredentialsPass();
        const char* userArg = (user && *user) ? user : nullptr;
        const char* passArg = (pass && *pass) ? pass : nullptr;
        if (mqttClient.connect(clientId, userArg, passArg)) {
            LOG_INFO("[MQTT] Connected to broker");
            subscribeCommands();
            deviceManager.forEachDevice([this](Device& d) {
                publishDiscovery(d);
                publishState(d.id, false);
            });
            publishHubInfoDiscovery();
            publishHubInfo();
            lastInfoPublishMs = millis();
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
          lastReconnectAttempt(0),
          lastInfoPublishMs(0) {
        // 768 B holds our worst-case discovery packet (~470 B JSON +
        // ~60 B topic + headers) with comfortable margin. Saves 256 B of
        // permanent heap vs the previous 1024 B. Keep in sync with the
        // 768 B `buf` cap inside publishDiscovery().
        mqttClient.setBufferSize(768);
        // Default 15 s keepalive ⇒ PINGREQ/PINGRESP every 15 s, ~5760 round
        // trips/day. Each one churns the heap a little. 60 s is well within
        // HA's default `birth/will` timeout window and reduces background
        // MQTT traffic and fragmentation drift ~4×.
        mqttClient.setKeepAlive(60);
    }

    ~MQTTConnector() {
        if (instance == this) {
            instance = nullptr;
        }
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void setOnOtaCheckCallback(std::function<void()> callback) {
        onOtaCheckCallback = callback;
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
        unsigned long now = millis();
        if (now - lastInfoPublishMs >= kInfoPublishIntervalMs) {
            publishHubInfo();
            lastInfoPublishMs = now;
        }
    }

    /// Disconnect from the broker and stop attempting reconnects. Use this
    /// before destructive operations like `LittleFS.format()` so we don't
    /// keep retransmitting / publishing during the wipe.
    void shutdown() {
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        enabled = false;
    }

    bool isEnabled() const { return enabled; }
    bool isConnected() { return enabled && mqttClient.connected(); }
};
