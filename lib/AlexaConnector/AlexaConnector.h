#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Espalexa.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <cstring>
#include <functional>
extern "C" {
#include <user_interface.h>
}
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"

class AlexaConnector {
   private:
    struct RegisteredDeviceRef {
        char uuid[25];
        uint8_t index;
        bool active;
    };

    DeviceManager& deviceManager;
    IRManager& irManager;
    Espalexa espalexa;
    bool wifiEnabled;
    std::function<void(const Device& device, bool state)> onStateChangeCallback;
    RegisteredDeviceRef registeredDeviceRefs[ESPALEXA_MAXDEVICES] = {};

    WiFiUDP ssdpNotifyUdp;  // outbound SSDP NOTIFY (separate from Espalexa's socket)
    bool ssdpNotifyReady = false;
    unsigned long lastSsdpNotifyMs = 0;
    static constexpr unsigned long kSsdpNotifyIntervalMs = 30UL * 1000UL;
    static constexpr uint8_t kSsdpStartupBurstCount = 5;
    static constexpr unsigned long kSsdpStartupBurstIntervalMs = 2UL * 1000UL;
    uint8_t ssdpStartupBurstRemaining = 0;
    String escapedMacCached;
    uint8_t bridgeMacBytes_[6] = {0, 0, 0, 0, 0, 0};
    static constexpr const char* kBridgeIdPath = "/alexa_bridge_id.bin";

    void loadOrGenerateBridgeMac() {
        File f = LittleFS.open(kBridgeIdPath, "r");
        if (f && f.size() == 6) {
            f.read(bridgeMacBytes_, 6);
            f.close();
            LOG_INFO("[Alexa] Loaded persisted bridge MAC %02x:%02x:%02x:%02x:%02x:%02x",
                     bridgeMacBytes_[0], bridgeMacBytes_[1], bridgeMacBytes_[2],
                     bridgeMacBytes_[3], bridgeMacBytes_[4], bridgeMacBytes_[5]);
            return;
        }
        if (f) {
            f.close();
            LittleFS.remove(kBridgeIdPath);
        }

        uint32_t r0 = os_random();
        uint32_t r1 = os_random();
        bridgeMacBytes_[0] = (uint8_t)((r0 >> 0) & 0xFE) | 0x02;
        bridgeMacBytes_[1] = (uint8_t)((r0 >> 8) & 0xFF);
        bridgeMacBytes_[2] = (uint8_t)((r0 >> 16) & 0xFF);
        bridgeMacBytes_[3] = (uint8_t)((r1 >> 0) & 0xFF);
        bridgeMacBytes_[4] = (uint8_t)((r1 >> 8) & 0xFF);
        bridgeMacBytes_[5] = (uint8_t)((r1 >> 16) & 0xFF);

        File w = LittleFS.open(kBridgeIdPath, "w");
        if (!w) {
            LOG_ERROR("[Alexa] Failed to open %s for write — bridge ID will "
                      "re-roll on next boot, Alexa will need to rediscover",
                      kBridgeIdPath);
            return;
        }
        size_t n = w.write(bridgeMacBytes_, 6);
        w.close();
        if (n != 6) {
            LOG_ERROR("[Alexa] Wrote only %u/6 bytes to %s; bridge ID "
                      "will not survive reboot",
                      (unsigned)n, kBridgeIdPath);
            LittleFS.remove(kBridgeIdPath);
            return;
        }
        LOG_INFO("[Alexa] Generated NEW bridge MAC %02x:%02x:%02x:%02x:%02x:%02x "
                 "(persisted to %s)",
                 bridgeMacBytes_[0], bridgeMacBytes_[1], bridgeMacBytes_[2],
                 bridgeMacBytes_[3], bridgeMacBytes_[4], bridgeMacBytes_[5],
                 kBridgeIdPath);
    }

    void cacheEscapedMac() {
        if (!escapedMacCached.isEmpty()) {
            return;
        }
        char buf[13];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
                 bridgeMacBytes_[0], bridgeMacBytes_[1], bridgeMacBytes_[2],
                 bridgeMacBytes_[3], bridgeMacBytes_[4], bridgeMacBytes_[5]);
        escapedMacCached = String(buf);
    }

    void sendSsdpNotify(const char* nt, const char* usnSuffix) {
        IPAddress localIP = WiFi.localIP();
        if (localIP == IPAddress(0, 0, 0, 0)) {
            return;
        }
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", localIP[0], localIP[1], localIP[2],
                 localIP[3]);

        char buf[400];  // stack buffer — NOTIFY path stays heap-free
        int n = snprintf_P(buf, sizeof(buf), PSTR("NOTIFY * HTTP/1.1\r\n"
                                                  "HOST: 239.255.255.250:1900\r\n"
                                                  "CACHE-CONTROL: max-age=100\r\n"
                                                  "LOCATION: http://%s:80/description.xml\r\n"
                                                  "SERVER: FreeRTOS/6.0.5, UPnP/1.0, "
                                                  "IpBridge/1.17.0\r\n"
                                                  "NTS: ssdp:alive\r\n"
                                                  "NT: %s\r\n"
                                                  "USN: uuid:2f402f80-da50-11e1-9b23-%s%s\r\n"
                                                  "hue-bridgeid: %s\r\n"
                                                  "\r\n"),
                           ipStr, nt, escapedMacCached.c_str(), usnSuffix,
                           escapedMacCached.c_str());
        if (n <= 0 || (size_t)n >= sizeof(buf)) {
            LOG_WARN("[Alexa] SSDP NOTIFY truncated (n=%d)", n);
            return;
        }
        IPAddress mcast(239, 255, 255, 250);
        if (!ssdpNotifyUdp.beginPacket(mcast, 1900)) {
            return;
        }
        ssdpNotifyUdp.write(buf, (size_t)n);
        ssdpNotifyUdp.endPacket();
    }

    void broadcastSsdpAlive() {
        if (!wifiEnabled || WiFi.status() != WL_CONNECTED) {
            return;
        }
        cacheEscapedMac();
        char uuidNt[64];
        snprintf(uuidNt, sizeof(uuidNt), "uuid:2f402f80-da50-11e1-9b23-%s",
                 escapedMacCached.c_str());
        sendSsdpNotify("upnp:rootdevice", "::upnp:rootdevice");
        sendSsdpNotify(uuidNt, "");
        sendSsdpNotify("urn:schemas-upnp-org:device:basic:1",
                       "::urn:schemas-upnp-org:device:basic:1");
    }

    RegisteredDeviceRef* findRefByUuid(const String& uuid) {
        for (auto& ref : registeredDeviceRefs) {
            if (ref.active && uuid == ref.uuid) {
                return &ref;
            }
        }
        return nullptr;
    }

    RegisteredDeviceRef* findRefByIndex(uint8_t index) {
        for (auto& ref : registeredDeviceRefs) {
            if (ref.active && ref.index == index) {
                return &ref;
            }
        }
        return nullptr;
    }

    RegisteredDeviceRef* findOrCreateRefForUuid(const String& uuid) {
        if (RegisteredDeviceRef* existing = findRefByUuid(uuid)) {
            return existing;
        }
        for (auto& ref : registeredDeviceRefs) {
            if (!ref.active) {
                strncpy(ref.uuid, uuid.c_str(), sizeof(ref.uuid) - 1);
                ref.uuid[sizeof(ref.uuid) - 1] = '\0';
                ref.index = 0;
                ref.active = true;
                return &ref;
            }
        }
        return nullptr;
    }

    void clearRefByUuid(const String& uuid) {
        if (RegisteredDeviceRef* ref = findRefByUuid(uuid)) {
            ref->active = false;
            ref->uuid[0] = '\0';
            ref->index = 0;
        }
    }

    void handleDeviceCallback(EspalexaDevice* d) {
        if (d == nullptr) {
            return;
        }
        RegisteredDeviceRef* ref = findRefByIndex(d->getId());
        if (ref == nullptr) {
            LOG_ERROR("[Alexa] Callback index=%u has no UUID mapping", (unsigned)d->getId());
            return;
        }

        const String uuid(ref->uuid);
        uint8_t value = d->getValue();
        bool state = value > 0;
        LOG_DEBUG("[Alexa] Set state for uuid=%s to %s with value %d",
                  uuid.c_str(), state ? "ON" : "OFF", value);

        auto device = deviceManager.getDeviceByUuid(uuid);
        if (device) {
            if (onStateChangeCallback) {
                onStateChangeCallback(*device, state);
            }
            if (state) {
                irManager.sendProtocol(device->onCommand);
                LOG_INFO("[Alexa] Turning ON '%s' (uuid=%s)", device->name.c_str(), uuid.c_str());
            } else {
                irManager.sendProtocol(device->offCommand);
                LOG_INFO("[Alexa] Turning OFF '%s' (uuid=%s)", device->name.c_str(), uuid.c_str());
            }
        } else {
            LOG_ERROR("[Alexa] Device uuid=%s not found in device manager", uuid.c_str());
        }
    }

   public:
    AlexaConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager), irManager(irManager), wifiEnabled(false) {};

    ~AlexaConnector() {};

    void begin() {
        if (WiFi.status() != WL_CONNECTED) {
            LOG_INFO("[Alexa] WiFi not connected, Alexa functionality disabled");
            wifiEnabled = false;
            return;
        }

        if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {  // wait for DHCP before binding UDP
            LOG_WARN("[Alexa] Deferring begin(): waiting for DHCP-assigned IP");
            wifiEnabled = false;
            return;
        }

        wifiEnabled = true;
        WiFi.mode(WIFI_STA);

        loadOrGenerateBridgeMac();  // before setBridgeMac()
        espalexa.setBridgeMac(bridgeMacBytes_);

        {
            char suffixBuf[7];
            snprintf(suffixBuf, sizeof(suffixBuf), "%02x%02x%02x",
                     bridgeMacBytes_[3], bridgeMacBytes_[4], bridgeMacBytes_[5]);
            String friendly = String("IR Hub ") + suffixBuf;
            espalexa.setFriendlyName(friendly);
            LOG_INFO("[Alexa] Bridge friendlyName='%s'", friendly.c_str());
        }

        deviceManager.forEachDevice([this](Device& device) { registerDevice(device); });

        espalexa.begin();

        cacheEscapedMac();
        ssdpNotifyReady = true;
        ssdpStartupBurstRemaining = kSsdpStartupBurstCount;
        lastSsdpNotifyMs = 0;  // forces broadcast on the next update() tick

        IPAddress ip = WiFi.localIP();
        LOG_INFO("[Alexa] Alexa functionality enabled (IP=%u.%u.%u.%u)",
                 (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
    }

    void shutdown() {
        if (ssdpNotifyReady) {
            ssdpNotifyUdp.stop();
            ssdpNotifyReady = false;
        }
        if (wifiEnabled) {
            espalexa.stop();
        }
        wifiEnabled = false;
        for (auto& ref : registeredDeviceRefs) {
            ref.active = false;
            ref.uuid[0] = '\0';
            ref.index = 0;
        }
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void registerDevice(const Device& device) {
        if (!wifiEnabled) {
            return;
        }

        String alexaName = device.uuid.substring(0, 6) + " " + device.name;

        uint8_t alexaIdx = espalexa.addDevice(
            alexaName.c_str(),
            [this](EspalexaDevice* d) { handleDeviceCallback(d); },
            EspalexaDeviceType::dimmable);
        if (alexaIdx == 0) {
            LOG_ERROR("[Alexa] addDevice failed for '%s' — likely at "
                      "ESPALEXA_MAXDEVICES (20) cap",
                      alexaName.c_str());
            return;
        }

        EspalexaDevice* d = espalexa.getDevice((uint8_t)(alexaIdx - 1));
        if (d != nullptr) {
            d->setStableId((uint16_t)(device.alexaSlot - 1));  // Espalexa uses (stableId+1)&0xFF
            if (device.uuid.length() >= 12) {
                uint8_t uidMac[6];
                for (uint8_t i = 0; i < 6; i++) {
                    char hex[3] = {device.uuid.charAt(i * 2),
                                   device.uuid.charAt(i * 2 + 1), '\0'};
                    uidMac[i] = (uint8_t)strtol(hex, nullptr, 16);
                }
                uidMac[0] = (uint8_t)((uidMac[0] & 0xFE) | 0x02);
                d->setUniqueIdMac(uidMac);
            }
        }

        uint8_t deviceIndex = (uint8_t)(alexaIdx - 1);
        RegisteredDeviceRef* ref = findOrCreateRefForUuid(device.uuid);
        if (ref == nullptr) {
            LOG_ERROR("[Alexa] UUID map full; cannot track device uuid=%s", device.uuid.c_str());
            return;
        }
        ref->index = deviceIndex;

        LOG_INFO("[Alexa] Registered '%s' as Hue white lamp, slot=%u alexaSlot=%u uuid=%s",
                 alexaName.c_str(), (unsigned)alexaIdx, (unsigned)device.alexaSlot,
                 device.uuid.c_str());
    }

    void unregisterDevice(const Device& device) {
        if (wifiEnabled) {
            LOG_DEBUG("[Alexa] Device %s unregistered (note: Espalexa keeps devices registered)",
                      device.name.c_str());
        }
        clearRefByUuid(device.uuid);
    }

    // Keep Alexa's internal per-device state in sync when commands originate elsewhere (e.g. MQTT).
    void syncDeviceState(const String& uuid, bool state) {
        if (!wifiEnabled) {
            return;
        }
        RegisteredDeviceRef* ref = findRefByUuid(uuid);
        if (ref == nullptr) {
            return;
        }
        EspalexaDevice* d = espalexa.getDevice(ref->index);
        if (d != nullptr) {
            d->setState(state);
        }
    }

    void update() {
        if (!wifiEnabled) {
            return;
        }
        espalexa.loop();

        unsigned long now = millis();
        unsigned long interval = (ssdpStartupBurstRemaining > 0)
                                     ? kSsdpStartupBurstIntervalMs
                                     : kSsdpNotifyIntervalMs;
        if (lastSsdpNotifyMs == 0 || (now - lastSsdpNotifyMs) >= interval) {
            broadcastSsdpAlive();
            lastSsdpNotifyMs = now;
            if (ssdpStartupBurstRemaining > 0) {
                ssdpStartupBurstRemaining--;
            }
        }
    }

    bool isEnabled() const { return wifiEnabled; }
};