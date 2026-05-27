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
    bool enabled;
    int lastConnectState;
    // 12 hex chars + nul — kept on the heap-free side to cut persistent String overhead
    char hubMacHex[13];
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long kReconnectIntervalMs = 5000;
    // Hub info is diagnostic; publishing every minute creates needless heap churn
    // (WiFi.SSID() temp-allocates a String, snprintf walks a 320-byte buffer, etc.).
    // 5 minutes is plenty fresh for HA UI and trims the per-minute allocations.
    static constexpr unsigned long kInfoPublishIntervalMs = 5UL * 60UL * 1000UL;
    unsigned long lastInfoPublishMs;

    std::function<void(const Device& device, bool state)> onStateChangeCallback;
    std::function<void()> onOtaCheckCallback;

    static void staticCallback(char* topic, byte* payload, unsigned int length) {
        if (instance) {
            instance->handleIncomingMessage(topic, payload, length);
        }
    }

    static void buildStaMacHex(char (&out)[13]) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(out, sizeof(out), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],
                 mac[4], mac[5]);
    }

    static constexpr size_t kTopicBufSize = 96;  // stack topic buffers (heap-free MQTT path)

    size_t discoveryTopicForUuid(const char* uuid, char* out, size_t outSize) const {
        return snprintf(out, outSize,
                        "homeassistant/switch/ir_hub_%s_device_%s/config",
                        hubMacHex, uuid);
    }

    size_t commandTopicForUuid(const char* uuid, char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/device/%s/set",
                        hubMacHex, uuid);
    }

    size_t stateTopicForUuid(const char* uuid, char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/device/%s/state",
                        hubMacHex, uuid);
    }

    size_t otaCheckTopic(char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/ota/check", hubMacHex);
    }

    size_t infoTopic(char* out, size_t outSize) const {
        return snprintf(out, outSize, "ir_hub/%s/info", hubMacHex);
    }

    size_t infoDiscoveryTopic(const char* key, char* out, size_t outSize) const {
        return snprintf(out, outSize, "homeassistant/sensor/ir_hub_%s_%s/config",
                        hubMacHex, key);
    }

    bool publishInfoDiscoverySensor(const char* key, const char* name, const char* valueTemplate,
                                    const char* unit = nullptr, const char* deviceClass = nullptr,
                                    const char* icon = nullptr) {
        char stateTopic[kTopicBufSize];
        infoTopic(stateTopic, sizeof(stateTopic));

        char uniqueId[56];
        snprintf(uniqueId, sizeof(uniqueId), "ir_hub_%s_%s", hubMacHex, key);
        char identifier[24];
        snprintf(identifier, sizeof(identifier), "ir_hub_%s", hubMacHex);

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

        // Build the payload by hand — much cheaper than spinning up a JsonDocument
        // every minute (which would otherwise heap-copy the SSID + allocate an internal pool).
        IPAddress ip = WiFi.localIP();
        char payload[320];
        int n = snprintf(payload, sizeof(payload),
                         "{\"firmware\":\"%s\","
                         "\"ssid\":\"%s\","
                         "\"ip\":\"%u.%u.%u.%u\","
                         "\"rssi\":%d,"
                         "\"uptime_s\":%lu,"
                         "\"free_heap\":%u,"
                         "\"max_block\":%u,"
                         "\"heap_frag\":%u}",
                         FIRMWARE_VERSION,
                         WiFi.SSID().c_str(),
                         (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
                         (int)WiFi.RSSI(),
                         (unsigned long)(millis() / 1000UL),
                         (unsigned)ESP.getFreeHeap(),
                         (unsigned)ESP.getMaxFreeBlockSize(),
                         (unsigned)ESP.getHeapFragmentation());
        if (n <= 0 || (size_t)n >= sizeof(payload)) {
            LOG_ERROR("[MQTT] Hub info JSON too large (n=%d)", n);
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
        char cmdTopic[kTopicBufSize];  // ArduinoJson v7 holds const char* by reference
        char stTopic[kTopicBufSize];
        char uniqueId[56];   // "ir_hub_" + 12 mac hex + "_" + 32 uuid + null
        char identifier[24]; // "ir_hub_" + 12 mac hex + null
        commandTopicForUuid(device.uuid.c_str(), cmdTopic, sizeof(cmdTopic));
        stateTopicForUuid(device.uuid.c_str(), stTopic, sizeof(stTopic));
        snprintf(uniqueId, sizeof(uniqueId), "ir_hub_%s_%s", hubMacHex,
                 device.uuid.c_str());
        snprintf(identifier, sizeof(identifier), "ir_hub_%s", hubMacHex);

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
            LOG_ERROR("[MQTT] Discovery JSON too large for device %s", device.uuid.c_str());
            return false;
        }

        char topic[kTopicBufSize];
        discoveryTopicForUuid(device.uuid.c_str(), topic, sizeof(topic));
        bool ok = mqttClient.publish(topic, buf, true);
        if (!ok) {
            LOG_ERROR("[MQTT] Failed to publish discovery for device %s "
                      "(topic_len=%u json_len=%u buf=%u state=%d)",
                      device.uuid.c_str(), (unsigned)strlen(topic), (unsigned)n,
                      (unsigned)mqttClient.getBufferSize(), mqttClient.state());
        } else {
            LOG_INFO("[MQTT] Published discovery for '%s' (uuid=%s, %u-byte JSON)",
                     device.name.c_str(), device.uuid.c_str(), (unsigned)n);
        }
        return ok;
    }

    bool publishState(const char* uuid, bool on) {
        const char* payload = on ? "ON" : "OFF";
        char topic[kTopicBufSize];
        stateTopicForUuid(uuid, topic, sizeof(topic));
        return mqttClient.publish(topic, payload, true);
    }

    bool publishDiscoveryRemove(const char* uuid) {
        char topic[kTopicBufSize];
        discoveryTopicForUuid(uuid, topic, sizeof(topic));
        return mqttClient.publish(topic, "", true);
    }

    bool subscribeCommands() {
        char sub[kTopicBufSize];
        snprintf(sub, sizeof(sub), "ir_hub/%s/device/+/set", hubMacHex);
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

    void applyCommandForDevice(const char* uuid, bool turnOn) {
        auto device = deviceManager.getDeviceByUuid(uuid);
        if (!device) {
            LOG_ERROR("[MQTT] Unknown device uuid %s", uuid);
            return;
        }

        bool hadCallback = false;
        if (onStateChangeCallback) {
            hadCallback = true;
            onStateChangeCallback(*device, turnOn);
        }

        if (turnOn) {
            irManager.sendProtocol(device->onCommand);
            LOG_INFO("[MQTT] ON device %s (%s)", uuid, device->name.c_str());
        } else {
            irManager.sendProtocol(device->offCommand);
            LOG_INFO("[MQTT] OFF device %s (%s)", uuid, device->name.c_str());
        }

        // Fallback publish if app-level callback is not configured.
        if (!hadCallback) {
            publishState(uuid, turnOn);
        }
    }

    void handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
        const size_t topicLen = strlen(topic);
        char otaTopic[kTopicBufSize];
        const size_t otaLen = otaCheckTopic(otaTopic, sizeof(otaTopic));
        if (topicLen == otaLen && memcmp(topic, otaTopic, topicLen) == 0) {
            LOG_INFO("[MQTT] OTA check requested via MQTT");
            if (onOtaCheckCallback) onOtaCheckCallback();
            return;
        }

        const size_t macLen = strlen(hubMacHex);
        const size_t kPrefixFixed = sizeof("ir_hub/") - 1;        // 7
        const size_t kMid = sizeof("/device/") - 1;               // 8
        const size_t kSuffix = sizeof("/set") - 1;                // 4
        const size_t kUuidLen = 24;

        if (topicLen != kPrefixFixed + macLen + kMid + kUuidLen + kSuffix) {
            return;
        }
        if (memcmp(topic, "ir_hub/", kPrefixFixed) != 0) {
            return;
        }
        if (memcmp(topic + kPrefixFixed, hubMacHex, macLen) != 0) {
            return;
        }
        if (memcmp(topic + kPrefixFixed + macLen, "/device/", kMid) != 0) {
            return;
        }
        if (memcmp(topic + topicLen - kSuffix, "/set", kSuffix) != 0) {
            return;
        }

        const char* idStart = topic + kPrefixFixed + macLen + kMid;
        for (size_t i = 0; i < kUuidLen; i++) {
            char ch = idStart[i];
            bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            if (!ok) {
                return;  // not a uuid we generated
            }
        }
        char idBuf[kUuidLen + 1];
        memcpy(idBuf, idStart, kUuidLen);
        idBuf[kUuidLen] = '\0';

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
            applyCommandForDevice(idBuf, true);
        } else if (strcmp(cmd, "OFF") == 0) {
            applyCommandForDevice(idBuf, false);
        } else {
            LOG_WARN("[MQTT] Unknown payload for %s: %s", topic, cmd);
        }
    }

    bool connectBroker() {
        char clientId[24];  // "ir_hub_" (7) + 12 mac hex + null = 20
        snprintf(clientId, sizeof(clientId), "ir_hub_%s", hubMacHex);
        const char* user = mqttCredentialsUser();  // nullptr creds => anonymous connect
        const char* pass = mqttCredentialsPass();
        const char* userArg = (user && *user) ? user : nullptr;
        const char* passArg = (pass && *pass) ? pass : nullptr;
        if (mqttClient.connect(clientId, userArg, passArg)) {
            lastConnectState = MQTT_CONNECTED;
            LOG_INFO("[MQTT] Connected to broker");
            subscribeCommands();
            size_t republished = 0;
            deviceManager.forEachDevice([this, &republished](Device& d) {
                if (publishDiscovery(d)) {
                    publishState(d.uuid.c_str(), false);
                    republished++;
                }
            });
            LOG_INFO("[MQTT] Republished discovery for %u device(s) after connect",
                     (unsigned)republished);
            publishHubInfoDiscovery();
            publishHubInfo();
            lastInfoPublishMs = millis();
            return true;
        }

        lastConnectState = mqttClient.state();
        LOG_WARN("[MQTT] Connect failed, rc=%d", lastConnectState);
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
          lastConnectState(MQTT_DISCONNECTED),
          lastReconnectAttempt(0),
          lastInfoPublishMs(0) {
        hubMacHex[0] = '\0';
        mqttClient.setBufferSize(640);  // reduced from 768 to lower persistent heap
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
            lastConnectState = MQTT_DISCONNECTED;
            return;
        }

        if (!mqttCredentialsConfigured()) {
            LOG_INFO("[MQTT] No broker configured; MQTT disabled. "
                     "Set host via the Wi-Fi captive portal to enable.");
            enabled = false;
            lastConnectState = MQTT_DISCONNECTED;
            return;
        }

        mqttClient.setServer(mqttCredentialsHost(), mqttCredentialsPort());
        mqttClient.setCallback(staticCallback);
        enabled = true;

        buildStaMacHex(hubMacHex);
        LOG_INFO("[MQTT] Broker %s:%u, topic ns ir_hub/%s/device/...",
                 mqttCredentialsHost(), (unsigned)mqttCredentialsPort(),
                 hubMacHex);

        if (connectBroker()) {
            LOG_INFO("[MQTT] Home Assistant MQTT enabled");
        } else {
            LOG_WARN("[MQTT] Initial broker connection failed; will retry in loop()");
        }
    }

    void registerDevice(const Device& device) {
        if (!enabled) {
            LOG_WARN("[MQTT] registerDevice('%s') skipped: MQTT disabled "
                     "(will republish on next broker connect)",
                     device.name.c_str());
            return;
        }
        if (!mqttClient.connected()) {
            LOG_WARN("[MQTT] registerDevice('%s') skipped: broker not "
                     "connected (state=%d) — will republish on reconnect",
                     device.name.c_str(), mqttClient.state());
            return;
        }
        publishDiscovery(device);
        publishState(device.uuid.c_str(), false);
    }

    void unregisterDevice(const Device& device) {
        if (!enabled) {
            LOG_WARN("[MQTT] unregisterDevice('%s') skipped: MQTT disabled",
                     device.name.c_str());
            return;
        }
        if (!mqttClient.connected()) {
            LOG_WARN("[MQTT] unregisterDevice('%s') skipped: broker not "
                     "connected (state=%d) — stale entity may linger in HA",
                     device.name.c_str(), mqttClient.state());
            return;
        }
        publishDiscoveryRemove(device.uuid.c_str());
    }

    void update() {
        if (!enabled) {
            return;
        }
        if (!mqttClient.connected()) {
            lastConnectState = mqttClient.state();
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

    void shutdown() {
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        enabled = false;
        lastConnectState = MQTT_DISCONNECTED;
    }

    bool isEnabled() const { return enabled; }
    bool isConnected() { return enabled && mqttClient.connected(); }
    bool syncDeviceState(const String& uuid, bool on) {
        if (!enabled || !mqttClient.connected()) {
            return false;
        }
        return publishState(uuid.c_str(), on);
    }
    int getLastConnectState() { return lastConnectState; }
    bool hasError() {
        if (!enabled || mqttClient.connected()) {
            return false;
        }
        return isFatalConnectState(lastConnectState);
    }
    static const char* connectStateShort(int state) {
        switch (state) {
            case MQTT_CONNECTED:
                return "Connected";
            case MQTT_DISCONNECTED:
                return "Disconnected";
            case MQTT_CONNECTION_LOST:
                return "Connection lost";
            case MQTT_CONNECTION_TIMEOUT:
                return "Connection timeout";
            case MQTT_CONNECT_FAILED:
                return "Connect failed";
            case MQTT_CONNECT_BAD_PROTOCOL:
                return "Bad protocol";
            case MQTT_CONNECT_BAD_CLIENT_ID:
                return "Bad client ID";
            case MQTT_CONNECT_UNAVAILABLE:
                return "Server unavailable";
            case MQTT_CONNECT_BAD_CREDENTIALS:
                return "Bad credentials";
            case MQTT_CONNECT_UNAUTHORIZED:
                return "Unauthorized";
            default:
                return "Unknown";
        }
    }
    const char* lastConnectStateShort() { return connectStateShort(lastConnectState); }
    static bool isFatalConnectState(int state) {
        // Fatal/configuration-ish failures (bad auth/protocol/client ID/authorization).
        return state == MQTT_CONNECT_BAD_PROTOCOL || state == MQTT_CONNECT_BAD_CLIENT_ID ||
               state == MQTT_CONNECT_BAD_CREDENTIALS || state == MQTT_CONNECT_UNAUTHORIZED;
    }
};
