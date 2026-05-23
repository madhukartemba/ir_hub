#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <functional>
#include <memory>
#include "Log.h"

/// Pull-based OTA: periodically fetches a JSON manifest, compares versions,
/// and self-flashes via ESP8266HTTPUpdate when a newer build is published.
/// Designed for remote devices (manifest hosted on GitHub Releases or similar).
///
/// Manifest schema (minimum):
///   { "version": "1.0.1", "url": "https://.../firmware_v3.bin" }
///
/// Per-variant schema (preferred — one manifest serves all PCB revisions):
///   {
///     "variants": {
///       "v3": { "version": "1.0.1", "url": "https://.../firmware_v3.bin" }
///     }
///   }
class OtaUpdater {
   public:
    void begin(const char* manifestUrl,
               const char* currentVersion,
               const char* hwVariant,
               unsigned long checkIntervalMs) {
        this->manifestUrl_ = manifestUrl ? manifestUrl : "";
        this->currentVersion_ = currentVersion ? currentVersion : "0.0.0";
        this->hwVariant_ = hwVariant ? hwVariant : "";
        this->checkIntervalMs_ = checkIntervalMs;
        this->lastCheck_ = 0;
        this->bootDelayUntil_ = millis() + kBootDelayMs;

        enabled_ = manifestUrl && *manifestUrl;
        if (!enabled_) {
            LOG_INFO("[OTA-HTTP] Disabled (no manifest URL configured)");
            return;
        }
        checkPending_ = true;  // do an initial check shortly after boot
        LOG_INFO("[OTA-HTTP] Enabled, manifest=%s current=%s variant=%s",
                 manifestUrl_, currentVersion_, hwVariant_);
    }

    /// Force a manifest check on the next update() tick.
    void checkNow() {
        if (!enabled_) {
            LOG_WARN("[OTA-HTTP] checkNow ignored — updater disabled");
            return;
        }
        LOG_INFO("[OTA-HTTP] Check requested via callback");
        checkPending_ = true;
        bootDelayUntil_ = 0;  // bypass the boot grace period for on-demand checks
    }

    /// Wire UI / subsystem teardown hooks. All optional.
    void setOnUpdateStart(std::function<void()> cb) { onUpdateStart_ = cb; }
    void setOnUpdateProgress(std::function<void(unsigned, unsigned)> cb) {
        onUpdateProgress_ = cb;
    }
    void setOnUpdateError(std::function<void(const char*)> cb) { onUpdateError_ = cb; }

    void update() {
        if (!enabled_) return;
        if (WiFi.status() != WL_CONNECTED) return;

        unsigned long now = millis();
        if (now < bootDelayUntil_) return;

        bool intervalElapsed = (now - lastCheck_) >= checkIntervalMs_;
        if (!checkPending_ && !intervalElapsed) return;

        checkPending_ = false;
        lastCheck_ = now;
        performCheck();
    }

    bool isEnabled() const { return enabled_; }
    const char* currentVersion() const { return currentVersion_; }

   private:
    // Wait a bit after boot before the first check so the device isn't
    // simultaneously connecting to Wi-Fi, registering with HA, AND pulling TLS
    // — the heap spike would risk a boot-time crash on flaky networks.
    static constexpr unsigned long kBootDelayMs = 30UL * 1000UL;
    // A TLS handshake on ESP8266 needs:
    //   - the receive buffer (kTlsRxBuffer below)
    //   - BearSSL session state (~6 KB)
    //   - lwIP TCP packet buffers (~3 KB)
    // So the minimum free heap before we even *attempt* a manifest fetch is
    // ~12-14 KB. Refuse below that — better to log "skipped" than to OOM the
    // SYS task and panic the chip.
    static constexpr uint32_t kMinHeapForManifest = 14 * 1024;
    static constexpr uint32_t kMinHeapForUpdate = 20 * 1024;
    // Small TLS buffers only work when the server supports MFLN (RFC 6066).
    // jsDelivr does; Fastly (which fronts raw.githubusercontent.com) usually
    // doesn't. See docs/OTA_RELEASES.md for the recommended URL pattern.
    static constexpr int kTlsRxBuffer = 1024;
    static constexpr int kTlsTxBuffer = 512;

    const char* manifestUrl_ = "";
    const char* currentVersion_ = "0.0.0";
    const char* hwVariant_ = "";
    unsigned long checkIntervalMs_ = 6UL * 60UL * 60UL * 1000UL;
    unsigned long lastCheck_ = 0;
    unsigned long bootDelayUntil_ = 0;
    bool enabled_ = false;
    bool checkPending_ = false;

    std::function<void()> onUpdateStart_;
    std::function<void(unsigned, unsigned)> onUpdateProgress_;
    std::function<void(const char*)> onUpdateError_;

    void performCheck() {
        LOG_INFO("[OTA-HTTP] Checking manifest at %s", manifestUrl_);

        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < kMinHeapForManifest) {
            LOG_WARN("[OTA-HTTP] Skipping check, low heap (%u < %u)",
                     (unsigned)freeHeap, (unsigned)kMinHeapForManifest);
            return;
        }

        String firmwareUrl;
        String newVersion;
        if (!fetchManifest(firmwareUrl, newVersion)) {
            return;
        }

        if (compareVersions(newVersion.c_str(), currentVersion_) <= 0) {
            LOG_INFO("[OTA-HTTP] Up-to-date (current=%s, latest=%s)",
                     currentVersion_, newVersion.c_str());
            return;
        }

        LOG_INFO("[OTA-HTTP] New firmware available: %s -> %s",
                 currentVersion_, newVersion.c_str());

        // Re-check heap before committing to the download — the manifest fetch
        // may have left the heap slightly more fragmented.
        freeHeap = ESP.getFreeHeap();
        if (freeHeap < kMinHeapForUpdate) {
            LOG_WARN("[OTA-HTTP] New firmware available but heap too low "
                     "(%u < %u) — will retry next cycle",
                     (unsigned)freeHeap, (unsigned)kMinHeapForUpdate);
            return;
        }

        LOG_INFO("[OTA-HTTP] Downloading from %s", firmwareUrl.c_str());
        performUpdate(firmwareUrl.c_str());
    }

    bool fetchManifest(String& outUrl, String& outVersion) {
        bool isHttps = urlIsHttps(manifestUrl_);
        std::unique_ptr<WiFiClient> client = makeClient(isHttps);
        if (!client) {
            LOG_WARN("[OTA-HTTP] Failed to allocate HTTP client");
            return false;
        }

        HTTPClient http;
        http.setTimeout(10000);
        http.useHTTP10(true);
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http.setUserAgent(String("IRHub/") + currentVersion_);
        if (!http.begin(*client, manifestUrl_)) {
            LOG_WARN("[OTA-HTTP] http.begin failed for manifest");
            return false;
        }

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            logHttpFailure("Manifest GET", code, isHttps, client.get());
            http.end();
            return false;
        }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            LOG_WARN("[OTA-HTTP] Manifest JSON parse error: %s", err.c_str());
            return false;
        }

        const char* version = doc["version"] | "";
        const char* url = doc["url"] | "";

        // Per-variant entry takes precedence so a single manifest can target
        // multiple PCB revisions with one bump.
        if (hwVariant_ && *hwVariant_) {
            JsonVariant v = doc["variants"][hwVariant_];
            if (!v.isNull()) {
                version = v["version"] | version;
                url = v["url"] | url;
            }
        }

        if (!version || !*version || !url || !*url) {
            LOG_WARN("[OTA-HTTP] Manifest missing version/url for variant '%s'",
                     hwVariant_);
            return false;
        }

        outVersion = version;
        outUrl = url;
        return true;
    }

    void performUpdate(const char* url) {
        if (onUpdateStart_) onUpdateStart_();

        bool isHttps = urlIsHttps(url);
        std::unique_ptr<WiFiClient> client = makeClient(isHttps);
        if (!client) {
            LOG_ERROR("[OTA-HTTP] Failed to allocate update client");
            if (onUpdateError_) onUpdateError_("no client");
            return;
        }

        ESPhttpUpdate.rebootOnUpdate(false);
        ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        ESPhttpUpdate.onProgress([this](int cur, int total) {
            if (onUpdateProgress_) onUpdateProgress_((unsigned)cur, (unsigned)total);
        });

        t_httpUpdate_return ret = ESPhttpUpdate.update(*client, url);
        switch (ret) {
            case HTTP_UPDATE_FAILED: {
                const String& msg = ESPhttpUpdate.getLastErrorString();
                LOG_ERROR("[OTA-HTTP] Update failed: %s", msg.c_str());
                if (onUpdateError_) onUpdateError_(msg.c_str());
                break;
            }
            case HTTP_UPDATE_NO_UPDATES:
                LOG_INFO("[OTA-HTTP] Server reported no updates");
                break;
            case HTTP_UPDATE_OK:
                LOG_INFO("[OTA-HTTP] Update successful — restarting");
                delay(500);
                ESP.restart();
                break;
        }
    }

    static bool urlIsHttps(const char* url) {
        return url && strncasecmp(url, "https://", 8) == 0;
    }

    static std::unique_ptr<WiFiClient> makeClient(bool isHttps) {
        if (isHttps) {
            auto* secure = new (std::nothrow) WiFiClientSecure();
            if (!secure) return nullptr;
            // Trust-on-first-use. Bytes are still encrypted in transit; for
            // supply-chain protection sign the binary (see OTA_RELEASES.md).
            secure->setInsecure();
            secure->setBufferSizes(kTlsRxBuffer, kTlsTxBuffer);
            return std::unique_ptr<WiFiClient>(secure);
        }
        return std::unique_ptr<WiFiClient>(new (std::nothrow) WiFiClient());
    }

    static void logHttpFailure(const char* op, int code, bool isHttps, WiFiClient* client) {
        const char* httpErr = "";
        switch (code) {
            case -1:  httpErr = " (CONNECTION_FAILED — likely TLS handshake)"; break;
            case -2:  httpErr = " (SEND_HEADER_FAILED)"; break;
            case -3:  httpErr = " (SEND_PAYLOAD_FAILED)"; break;
            case -4:  httpErr = " (NOT_CONNECTED)"; break;
            case -5:  httpErr = " (CONNECTION_LOST)"; break;
            case -6:  httpErr = " (NO_STREAM)"; break;
            case -7:  httpErr = " (NO_HTTP_SERVER)"; break;
            case -8:  httpErr = " (TOO_LESS_RAM — bump TLS buffers down or free heap)"; break;
            case -11: httpErr = " (READ_TIMEOUT)"; break;
            default:  break;
        }
        if (isHttps && client) {
            auto* secure = static_cast<WiFiClientSecure*>(client);
            char errBuf[64] = {0};
            int sslErr = secure->getLastSSLError(errBuf, sizeof(errBuf));
            if (sslErr != 0) {
                LOG_WARN("[OTA-HTTP] %s returned %d%s — SSL err %d: %s",
                         op, code, httpErr, sslErr, errBuf);
                return;
            }
        }
        LOG_WARN("[OTA-HTTP] %s returned %d%s", op, code, httpErr);
    }

    /// Returns >0 if a is newer than b, 0 if equal, <0 if older.
    /// Handles "1.10.0" > "1.2.0" correctly (integer-wise per segment).
    static int compareVersions(const char* a, const char* b) {
        if (!a) a = "";
        if (!b) b = "";
        if (*a == 'v' || *a == 'V') a++;
        if (*b == 'v' || *b == 'V') b++;
        while (*a || *b) {
            long na = 0, nb = 0;
            while (*a && *a != '.') {
                if (*a >= '0' && *a <= '9') na = na * 10 + (*a - '0');
                a++;
            }
            while (*b && *b != '.') {
                if (*b >= '0' && *b <= '9') nb = nb * 10 + (*b - '0');
                b++;
            }
            if (na != nb) return na > nb ? 1 : -1;
            if (*a == '.') a++;
            if (*b == '.') b++;
        }
        return 0;
    }
};
