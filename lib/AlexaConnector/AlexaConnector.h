#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Espalexa.h>
#include <WiFiUdp.h>
#include <functional>
#include <vector>
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
    /// Cached lowercase MAC (no colons). Matches the format Espalexa uses in
    /// its M-SEARCH responses so Alexa treats our NOTIFY as the same bridge.
    String escapedMacCached;

    void cacheEscapedMac() {
        if (!escapedMacCached.isEmpty()) {
            return;
        }
        escapedMacCached = WiFi.macAddress();
        escapedMacCached.replace(":", "");
        escapedMacCached.toLowerCase();
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

    void handleDeviceCallback(const String& deviceName, uint8_t value) {
        bool state = value > 0;
        LOG_DEBUG("[Alexa] Set state for device %s to %s with value %d", deviceName.c_str(),
                  state ? "ON" : "OFF", value);

        // DeviceManager is uncached: this scans /devices and parses each JSON
        // until it finds a match (~N * 10 ms). Acceptable on the rare Alexa
        // command path.
        auto device = deviceManager.getDeviceByName(deviceName);
        if (device) {
            if (onStateChangeCallback) {
                onStateChangeCallback(*device, state);
            }
            if (state) {
                irManager.sendProtocol(device->onCommand);
                LOG_INFO("[Alexa] Turning ON device %s", deviceName.c_str());
            } else {
                irManager.sendProtocol(device->offCommand);
                LOG_INFO("[Alexa] Turning OFF device %s", deviceName.c_str());
            }
        } else {
            LOG_ERROR("[Alexa] Device %s not found in device manager", deviceName.c_str());
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

    /// Release Espalexa-adjacent network resources. Espalexa itself does
    /// not expose a teardown API, so this only stops our auxiliary NOTIFY
    /// socket and flips the enable flag. Used before ESP.restart() to avoid
    /// half-sent UDP frames flying off during the reboot path.
    void shutdown() {
        if (ssdpNotifyReady) {
            ssdpNotifyUdp.stop();
            ssdpNotifyReady = false;
        }
        wifiEnabled = false;
    }

    void setOnStateChangeCallback(std::function<void(const Device& device, bool state)> callback) {
        onStateChangeCallback = callback;
    }

    void registerDevice(const Device& device) {
        if (wifiEnabled) {
            espalexa.addDevice(
                device.name.c_str(),
                [this, deviceName = device.name](EspalexaDevice* d) {
                    handleDeviceCallback(deviceName, d->getValue());
                },
                EspalexaDeviceType::onoff);
        }
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