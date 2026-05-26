#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Espalexa.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <functional>
#include <vector>
extern "C" {
#include <user_interface.h>
}
#include "DeviceManager.h"
#include "IRManager.h"
#include "Log.h"

class AlexaConnector {
   private:
    DeviceManager& deviceManager;
    IRManager& irManager;
    Espalexa espalexa;
    bool wifiEnabled;
    std::function<void(const Device& device, bool state)> onStateChangeCallback;

    // Outbound-only UDP socket used to multicast `ssdp:alive` NOTIFY packets.
    // Espalexa itself is response-only (it answers M-SEARCH but never
    // advertises), which means a single dropped M-SEARCH from Alexa can leave
    // the device invisible for a full 10-minute discovery cycle. Sending
    // periodic NOTIFY frames lets Echo devices pick up the bridge proactively
    // and dramatically improves discovery reliability after a reboot or
    // IP change. We bind a dedicated UDP socket so we don't race Espalexa's
    // private one.
    WiFiUDP ssdpNotifyUdp;
    bool ssdpNotifyReady = false;
    unsigned long lastSsdpNotifyMs = 0;
    /// Hue's max-age is 100 s; sending NOTIFY at ~30 s gives Alexa 3 chances
    /// to see the bridge before its cache entry would expire.
    static constexpr unsigned long kSsdpNotifyIntervalMs = 30UL * 1000UL;
    /// On (re)start, fire one immediate NOTIFY then a short burst (every 2 s
    /// for the first 10 s) so the device shows up in the Alexa app within
    /// seconds rather than minutes.
    static constexpr uint8_t kSsdpStartupBurstCount = 5;
    static constexpr unsigned long kSsdpStartupBurstIntervalMs = 2UL * 1000UL;
    uint8_t ssdpStartupBurstRemaining = 0;
    /// Cached lowercase 12-hex of the EFFECTIVE bridge MAC (the persistent
    /// random EUI-48 we generated in begin(), NOT the hardware WiFi MAC).
    /// Matches the format Espalexa uses in its M-SEARCH responses so Alexa
    /// treats our NOTIFY as the same bridge.
    String escapedMacCached;
    /// Raw 6-byte form of the same value. Persisted to LittleFS at
    /// `/alexa_bridge_id.bin`; regenerated whenever the file is missing
    /// (i.e. on first boot ever, and on every factory reset since
    /// LittleFS.format() wipes it). Rotating this is the whole reason
    /// this class persists anything at all — Alexa's cloud cache keys
    /// off this value as the bridge identity, so a fresh random EUI-48
    /// after a wipe makes Amazon see us as a brand-new bridge and
    /// prevents stale entities from re-attaching.
    uint8_t bridgeMacBytes_[6] = {0, 0, 0, 0, 0, 0};
    static constexpr const char* kBridgeIdPath = "/alexa_bridge_id.bin";

    /// Load `bridgeMacBytes_` from LittleFS, or generate + persist a new
    /// random locally-administered EUI-48 if no file exists / it's the
    /// wrong size. Called once from begin() before we touch Espalexa.
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

        // Generate a fresh locally-administered EUI-48. `os_random()` is
        // the ESP8266 SDK's hardware RNG; we draw two 32-bit words to
        // fill 6 bytes with random data. Setting byte0 bit 1 (LAA) and
        // clearing byte0 bit 0 (unicast) makes this a well-formed,
        // routable MAC that cannot collide with any IEEE-OUI-assigned
        // hardware MAC and is therefore guaranteed disjoint from any
        // previous bridge identity Alexa might still hold in cache.
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

    /// Build and multicast one SSDP NOTIFY frame. `nt` is the Notification
    /// Type (e.g. "upnp:rootdevice"); `usnSuffix` is appended to the UUID in
    /// the USN header (empty for the bare `uuid:...` advert).
    void sendSsdpNotify(const char* nt, const char* usnSuffix) {
        IPAddress localIP = WiFi.localIP();
        if (localIP == IPAddress(0, 0, 0, 0)) {
            return;
        }
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", localIP[0], localIP[1], localIP[2],
                 localIP[3]);

        // ~360 B worst case (root NT + long USN). Stack-allocated so this
        // path is heap-free even under fragmentation.
        char buf[400];
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

    /// Send all three NOTIFY variants that a real Hue bridge advertises.
    /// Some Echo generations key off `upnp:rootdevice`, some off the bare
    /// uuid, some off the `device:basic:1` URN — covering all three matches
    /// what nmap captures from a genuine bridge.
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

    void handleDeviceCallback(const String& uuid, const String& displayName, uint8_t value) {
        bool state = value > 0;
        LOG_DEBUG("[Alexa] Set state for '%s' (uuid=%s) to %s with value %d",
                  displayName.c_str(), uuid.c_str(), state ? "ON" : "OFF", value);

        // Direct uuid lookup — single LittleFS open instead of the
        // O(N) directory scan the old getDeviceByName path did.
        auto device = deviceManager.getDeviceByUuid(uuid);
        if (device) {
            if (onStateChangeCallback) {
                onStateChangeCallback(*device, state);
            }
            if (state) {
                irManager.sendProtocol(device->onCommand);
                LOG_INFO("[Alexa] Turning ON '%s' (uuid=%s)", displayName.c_str(), uuid.c_str());
            } else {
                irManager.sendProtocol(device->offCommand);
                LOG_INFO("[Alexa] Turning OFF '%s' (uuid=%s)", displayName.c_str(), uuid.c_str());
            }
        } else {
            LOG_ERROR("[Alexa] Device uuid=%s ('%s') not found in device manager", uuid.c_str(),
                      displayName.c_str());
        }
    }

   public:
    AlexaConnector(DeviceManager& deviceManager, IRManager& irManager)
        : deviceManager(deviceManager), irManager(irManager), wifiEnabled(false) {};

    ~AlexaConnector() {};

    void begin() {
        // Check if WiFi is available and connected
        if (WiFi.status() != WL_CONNECTED) {
            LOG_INFO("[Alexa] WiFi not connected, Alexa functionality disabled");
            wifiEnabled = false;
            return;
        }

        // Defensive: don't bring Espalexa up until DHCP has issued an IP.
        // WiFi.status() can briefly read WL_CONNECTED ~100-500 ms before
        // localIP() is non-zero; binding the UDP multicast socket in that
        // window leaves Espalexa with a permanently broken receive path
        // until the next reboot.
        if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
            LOG_WARN("[Alexa] Deferring begin(): waiting for DHCP-assigned IP");
            wifiEnabled = false;
            return;
        }

        wifiEnabled = true;
        WiFi.mode(WIFI_STA);

        // Load (or, on first boot / after factory reset, generate) the
        // persistent random EUI-48 that uniquely identifies this hub to
        // Alexa. MUST run before setBridgeMac() — begin() snapshots it.
        loadOrGenerateBridgeMac();
        espalexa.setBridgeMac(bridgeMacBytes_);

        // Suffix the bridge friendlyName with the last 3 bytes of the
        // rotated MAC so (a) multiple hubs on one LAN are distinguishable
        // and (b) every factory reset gives a visibly-different name —
        // a clear "this is a new bridge" signal in the Alexa app.
        {
            char suffixBuf[7];
            snprintf(suffixBuf, sizeof(suffixBuf), "%02x%02x%02x",
                     bridgeMacBytes_[3], bridgeMacBytes_[4], bridgeMacBytes_[5]);
            String friendly = String("IR Hub ") + suffixBuf;
            espalexa.setFriendlyName(friendly);
            LOG_INFO("[Alexa] Bridge friendlyName='%s'", friendly.c_str());
        }

        // Register all existing devices (add/remove callbacks are set in main.cpp)
        deviceManager.forEachDevice([this](Device& device) { registerDevice(device); });

        espalexa.begin();

        // Start our auxiliary outbound SSDP socket and queue the startup
        // NOTIFY burst so Alexa sees the device within ~10 s of boot.
        cacheEscapedMac();
        ssdpNotifyReady = true;
        ssdpStartupBurstRemaining = kSsdpStartupBurstCount;
        lastSsdpNotifyMs = 0;  // forces broadcast on the next update() tick

        LOG_INFO("[Alexa] Alexa functionality enabled (IP=%s)",
                 WiFi.localIP().toString().c_str());
    }

    /// Release Espalexa-adjacent network resources. Used before
    /// ESP.restart() so we don't leave stale UDP/TCP sockets dangling in
    /// lwIP across the reboot AND so Alexa receives `ssdp:byebye` frames
    /// (via our vendored Espalexa::stop) — that prompts Alexa to expire
    /// its cached bridge entry instead of holding stale uniqueid mappings
    /// across the restart.
    void shutdown() {
        if (ssdpNotifyReady) {
            ssdpNotifyUdp.stop();
            ssdpNotifyReady = false;
        }
        if (wifiEnabled) {
            espalexa.stop();
        }
        wifiEnabled = false;
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    // Register one IR-Hub device as a virtual Hue light on the bridge.
    //
    // Device type — registered as a `dimmable` Hue white lamp rather than
    // a Hue plug because multiple Echo generations omit the on/off toggle
    // from a plug's detail page. The brightness slider this exposes is
    // harmless: handleDeviceCallback treats any value > 0 as ON.
    //
    // Friendly name — prepended with the 6-char uuid prefix
    // ("0a850a SONY 1"). Alexa's app groups smart-home cards by the
    // first word of the friendly name, so a unique-per-device first
    // token is the cheapest way to bypass that grouping. Users rename
    // the card inside the Alexa app for voice control; the rename stays
    // Alexa-side and never reaches the hub, so our internal identity
    // stays stable.
    //
    // Uniqueid — the EUI-64 prefix is derived from the first 6 hex bytes
    // of the device UUID (with the LAA bit forced on). This is the
    // primary axis Alexa uses to dedupe cards — see Espalexa::
    // perDeviceModelId banner for the full story. The 2-hex endpoint
    // suffix is pinned to `device.alexaSlot` so Alexa's cloud cache
    // keeps pointing at the right physical IR command across reorders.
    void registerDevice(const Device& device) {
        if (!wifiEnabled) {
            return;
        }

        String alexaName = device.uuid.substring(0, 6) + " " + device.name;

        uint8_t alexaIdx = espalexa.addDevice(
            alexaName.c_str(),
            [this, uuid = device.uuid, displayName = alexaName](EspalexaDevice* d) {
                handleDeviceCallback(uuid, displayName, d->getValue());
            },
            EspalexaDeviceType::dimmable);
        if (alexaIdx == 0) {
            LOG_ERROR("[Alexa] addDevice failed for '%s' — likely at "
                      "ESPALEXA_MAXDEVICES (20) cap",
                      alexaName.c_str());
            return;
        }

        EspalexaDevice* d = espalexa.getDevice((uint8_t)(alexaIdx - 1));
        if (d != nullptr) {
            // Espalexa encodes the uniqueid endpoint as (stableId + 1) & 0xFF
            // — pass slot - 1 to arrive back at our intended endpoint byte.
            d->setStableId((uint16_t)(device.alexaSlot - 1));

            // Per-device EUI-48 for the Hue uniqueid prefix. LAA bit forced
            // on so the result is a well-formed locally-administered address.
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
        LOG_INFO("[Alexa] Registered '%s' as Hue white lamp, slot=%u alexaSlot=%u uuid=%s",
                 alexaName.c_str(), (unsigned)alexaIdx, (unsigned)device.alexaSlot,
                 device.uuid.c_str());
    }

    void unregisterDevice(const Device& device) {
        if (wifiEnabled) {
            // Espalexa doesn't have a direct removeDevice method
            // We'll need to handle this differently - devices will remain registered
            // but won't be accessible through the device manager
            LOG_DEBUG("[Alexa] Device %s unregistered (note: Espalexa keeps devices registered)",
                      device.name.c_str());
        }
    }

    void update() {
        if (!wifiEnabled) {
            return;
        }
        espalexa.loop();

        // Periodic outbound SSDP NOTIFY. Cheap (~360 B stack, no heap) and
        // gated by a millis() comparison so it costs almost nothing per loop.
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