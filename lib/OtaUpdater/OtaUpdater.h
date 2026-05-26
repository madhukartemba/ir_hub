#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>
#include <memory>
#include "Log.h"
#include "PendingOta.h"

class OtaUpdater {
   public:
    enum class CheckStatus : uint8_t {
        NEVER,
        CHECKING,
        UP_TO_DATE,
        UPDATE_PENDING,
        NO_WIFI,
        LOW_HEAP,
        CHECK_FAILED,
    };

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
        this->lastCheckStatus_ = CheckStatus::NEVER;
        this->lastCheckCompletedAtMs_ = 0;
        this->manualCheckPending_ = false;

        enabled_ = manifestUrl && *manifestUrl;
        if (!enabled_) {
            LOG_INFO("[OTA-HTTP] Disabled (no manifest URL configured)");
            return;
        }
        checkPending_ = true;  // do an initial check shortly after boot
        LOG_INFO("[OTA-HTTP] Enabled, manifest=%s current=%s variant=%s",
                 manifestUrl_, currentVersion_, hwVariant_);
    }

    void checkNow() {
        if (!enabled_) {
            LOG_WARN("[OTA-HTTP] checkNow ignored — updater disabled");
            return;
        }
        LOG_INFO("[OTA-HTTP] Check requested via callback");
        checkPending_ = true;
        manualCheckPending_ = true;
        lastCheckStatus_ = CheckStatus::CHECKING;
        bootDelayUntil_ = 0;  // bypass the boot grace period for on-demand checks
    }

    void setOnUpdatePending(std::function<void(const char* version, const char* url)> cb) {
        onUpdatePending_ = cb;
    }

    void update() {
        if (!enabled_) return;
        unsigned long now = millis();
        if (now < bootDelayUntil_) return;

        if (WiFi.status() != WL_CONNECTED) {
            if (manualCheckPending_) {
                manualCheckPending_ = false;
                checkPending_ = false;
                markCheckComplete(CheckStatus::NO_WIFI);
            }
            return;
        }

        bool intervalElapsed = (now - lastCheck_) >= checkIntervalMs_;
        if (!checkPending_ && !intervalElapsed) return;

        bool isManualCheck = manualCheckPending_;
        manualCheckPending_ = false;
        checkPending_ = false;
        lastCheck_ = now;
        lastCheckStatus_ = CheckStatus::CHECKING;
        uint32_t heapBefore = ESP.getFreeHeap();
        CheckStatus status = performCheck(isManualCheck);
        uint32_t heapAfter = ESP.getFreeHeap();
        int32_t delta = (int32_t)heapAfter - (int32_t)heapBefore;
        LOG_INFO("[OTA-HTTP] Heap after check: free=%u (delta=%+d)",
                 (unsigned)heapAfter, (int)delta);
        markCheckComplete(status);
    }

    bool isEnabled() const { return enabled_; }
    const char* currentVersion() const { return currentVersion_; }
    CheckStatus lastCheckStatus() const { return lastCheckStatus_; }
    bool hasCompletedCheck() const { return lastCheckCompletedAtMs_ != 0; }
    unsigned long lastCheckAgeMs() const {
        if (!hasCompletedCheck()) return 0;
        return millis() - lastCheckCompletedAtMs_;
    }
    const char* lastCheckStatusText() const {
        switch (lastCheckStatus_) {
            case CheckStatus::NEVER:
                return "Never checked";
            case CheckStatus::CHECKING:
                return "Checking...";
            case CheckStatus::UP_TO_DATE:
                return "Up to date";
            case CheckStatus::UPDATE_PENDING:
                return "Update found";
            case CheckStatus::NO_WIFI:
                return "No Wi-Fi";
            case CheckStatus::LOW_HEAP:
                return "Low heap";
            case CheckStatus::CHECK_FAILED:
                return "Check failed";
        }
        return "Unknown";
    }

   private:
    static constexpr unsigned long kBootDelayMs = 30UL * 1000UL;
    static constexpr uint32_t kMinHeapForManifest = 14 * 1024;
    static constexpr uint32_t kMinHeapForManualManifest = 13 * 1024;
    static constexpr uint16_t kMinMaxBlockForManifest = 7 * 1024;  // BearSSL needs ~6 KB contiguous
    static constexpr int kTlsRxBuffer = 1024;  // MFLN; 512 breaks Cloudflare cert chain
    static constexpr int kTlsTxBuffer = 512;

    const char* manifestUrl_ = "";
    const char* currentVersion_ = "0.0.0";
    const char* hwVariant_ = "";
    unsigned long checkIntervalMs_ = 6UL * 60UL * 60UL * 1000UL;
    unsigned long lastCheck_ = 0;
    unsigned long bootDelayUntil_ = 0;
    unsigned long lastCheckCompletedAtMs_ = 0;
    bool enabled_ = false;
    bool checkPending_ = false;
    bool manualCheckPending_ = false;
    CheckStatus lastCheckStatus_ = CheckStatus::NEVER;

    std::function<void(const char*, const char*)> onUpdatePending_;

    void markCheckComplete(CheckStatus status) {
        lastCheckStatus_ = status;
        lastCheckCompletedAtMs_ = millis();
    }

    CheckStatus performCheck(bool isManualCheck) {
        LOG_INFO("[OTA-HTTP] Checking manifest at %s (mode=%s)",
                 manifestUrl_, isManualCheck ? "manual" : "auto");

        uint32_t minHeap = isManualCheck ? kMinHeapForManualManifest : kMinHeapForManifest;
        uint32_t freeHeap = ESP.getFreeHeap();
        uint16_t maxBlock = ESP.getMaxFreeBlockSize();
        uint8_t frag = ESP.getHeapFragmentation();
        LOG_INFO("[OTA-HTTP] Heap before check: free=%u max_block=%u frag=%u%% (min_free=%u min_block=%u)",
                 (unsigned)freeHeap, (unsigned)maxBlock, (unsigned)frag,
                 (unsigned)minHeap, (unsigned)kMinMaxBlockForManifest);
        if (freeHeap < minHeap) {
            LOG_WARN("[OTA-HTTP] Skipping check, low heap (free=%u < %u)",
                     (unsigned)freeHeap, (unsigned)minHeap);
            return CheckStatus::LOW_HEAP;
        }
        if (maxBlock < kMinMaxBlockForManifest) {
            LOG_WARN("[OTA-HTTP] Skipping check, heap too fragmented "
                     "(max_block=%u < %u; total free=%u, frag=%u%%)",
                     (unsigned)maxBlock, (unsigned)kMinMaxBlockForManifest,
                     (unsigned)freeHeap, (unsigned)frag);
            return CheckStatus::LOW_HEAP;
        }

        String firmwareUrl;
        String newVersion;
        String firmwareMd5;
        uint32_t firmwareSize = 0;
        if (!fetchManifest(firmwareUrl, newVersion, firmwareSize, firmwareMd5)) {
            return CheckStatus::CHECK_FAILED;
        }

        if (compareVersions(newVersion.c_str(), currentVersion_) <= 0) {
            LOG_INFO("[OTA-HTTP] Up-to-date (current=%s, latest=%s)",
                     currentVersion_, newVersion.c_str());
            return CheckStatus::UP_TO_DATE;
        }

        LOG_INFO("[OTA-HTTP] New firmware available: %s -> %s (%u bytes)",
                 currentVersion_, newVersion.c_str(), (unsigned)firmwareSize);

        if (firmwareSize == 0) {
            LOG_ERROR("[OTA-HTTP] Manifest is missing 'size' for variant '%s' — "
                      "refusing to OTA (publish a release.py-produced manifest)",
                      hwVariant_);
            return CheckStatus::CHECK_FAILED;
        }

        if (!pending_ota::arm(firmwareUrl.c_str(), newVersion.c_str(),
                              firmwareSize, firmwareMd5.c_str())) {
            LOG_ERROR("[OTA-HTTP] Failed to arm pending OTA (URL/version/md5 too long?)");
            return CheckStatus::CHECK_FAILED;
        }
        markCheckComplete(CheckStatus::UPDATE_PENDING);
        if (onUpdatePending_) onUpdatePending_(newVersion.c_str(), firmwareUrl.c_str());

        LOG_INFO("[OTA-HTTP] Restarting into downloader mode...");
        delay(1500);  // let the UI message render before the reboot
        ESP.restart();
        return CheckStatus::UPDATE_PENDING;
    }

    bool fetchManifest(String& outUrl, String& outVersion,
                       uint32_t& outSize, String& outMd5) {
        uint32_t hStart = ESP.getFreeHeap();
        uint32_t hMin = hStart;
        auto sample = [&](const char* tag) {
            uint32_t h = ESP.getFreeHeap();
            if (h < hMin) hMin = h;
            LOG_INFO("[OTA-HTTP][heap] %-18s free=%u delta=%+d", tag,
                     (unsigned)h, (int)((int32_t)h - (int32_t)hStart));
        };

        bool isHttps = urlIsHttps(manifestUrl_);
        std::unique_ptr<WiFiClient> client = makeClient(isHttps);
        if (!client) {
            LOG_WARN("[OTA-HTTP] Failed to allocate HTTP client");
            return false;
        }
        sample("after client");

        HTTPClient http;
        http.setTimeout(10000);
        http.useHTTP10(true);
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http.setUserAgent(String("IRHub/") + currentVersion_);
        if (!http.begin(*client, manifestUrl_)) {
            LOG_WARN("[OTA-HTTP] http.begin failed for manifest");
            return false;
        }
        sample("after http.begin");

        int code = http.GET();
        sample("after http.GET");  // TLS handshake completes here — usually the spike
        if (code != HTTP_CODE_OK) {
            logHttpFailure("Manifest GET", code, isHttps, client.get());
            http.end();
            return false;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        sample("after json parse");
        http.end();
        sample("after http.end");
        if (err) {
            LOG_WARN("[OTA-HTTP] Manifest JSON parse error: %s", err.c_str());
            return false;
        }
        LOG_INFO("[OTA-HTTP][heap] PEAK usage: %u bytes (min free=%u)",
                 (unsigned)(hStart - hMin), (unsigned)hMin);

        const char* version = doc["version"] | "";
        const char* url = doc["url"] | "";
        const char* md5 = doc["md5"] | "";
        uint32_t size = doc["size"] | 0u;

        if (hwVariant_ && *hwVariant_) {
            JsonVariant v = doc["variants"][hwVariant_];
            if (!v.isNull()) {
                version = v["version"] | version;
                url = v["url"] | url;
                md5 = v["md5"] | md5;
                size = v["size"] | size;
            }
        }

        if (!version || !*version || !url || !*url) {
            LOG_WARN("[OTA-HTTP] Manifest missing version/url for variant '%s'",
                     hwVariant_);
            return false;
        }

        outVersion = version;
        outUrl = url;
        outMd5 = md5;
        outSize = size;
        return true;
    }

    static bool urlIsHttps(const char* url) {
        return url && strncasecmp(url, "https://", 8) == 0;
    }

    static std::unique_ptr<WiFiClient> makeClient(bool isHttps) {
        if (isHttps) {
            auto* secure = new (std::nothrow) WiFiClientSecure();
            if (!secure) return nullptr;
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
