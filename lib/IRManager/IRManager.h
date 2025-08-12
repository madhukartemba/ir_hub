#pragma once

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

struct IRCode {
    decode_type_t protocol;
    uint64_t value;
    uint16_t bits;

    bool isValid() const { return (protocol != UNKNOWN) && (bits > 0) && (value != 0); }
};
class IRManager {
   private:
    int rxPin;
    int txPin;
    const uint16_t kCaptureBufferSize = 1024;
    const uint8_t kTimeout = 50;
    const uint16_t kMinUnknownSize = 12;
    const uint8_t kTolerancePercentage = 25;  // 25% tolerance

    IRrecv *irrecv = nullptr;
    IRsend *irsend = nullptr;

    decode_results lastResults{};
    IRCode lastCode{};

    bool isPaused = false;

    IRCode fromDecodeResults(const decode_results &results) {
        IRCode code;
        code.protocol = results.decode_type;
        code.value = results.value;
        code.bits = results.bits;
        LOG_DEBUG("[IRManager] Decoded: protocol=%d, value=0x%llX, bits=%u", (int)code.protocol,
                  code.value, code.bits);
        return code;
    }

   public:
    IRManager() : rxPin(-1), txPin(-1) {}
    IRManager(int rx, int tx) : rxPin(rx), txPin(tx) {}

    void begin() {
        if (rxPin == -1 || txPin == -1) {
            LOG_ERROR("[IRManager] Pins not set. Use begin(rx, tx) or constructor with pins.");
            return;
        }
        LOG_INFO("[IRManager] Initializing with RX=%d, TX=%d", rxPin, txPin);
        irrecv = new IRrecv(rxPin, kCaptureBufferSize, kTimeout, true);
        irsend = new IRsend(txPin);

        irrecv->setUnknownThreshold(kMinUnknownSize);
        irrecv->setTolerance(kTolerancePercentage);
        irrecv->enableIRIn();
        irsend->begin();
        pauseIRReceiver();
        LOG_INFO("[IRManager] IR receiver and sender started (receiver paused)");
    }

    void begin(int rx, int tx) {
        rxPin = rx;
        txPin = tx;
        begin();
    }

    void pauseIRReceiver() {
        if (irrecv) {
            irrecv->pause();
            isPaused = true;
            LOG_DEBUG("[IRManager] IR receiver paused");
        }
    }

    void resumeIRReceiver() {
        if (irrecv) {
            irrecv->resume();
            isPaused = false;
            LOG_DEBUG("[IRManager] IR receiver resumed");
        }
    }

    IRCode convertJsonToIRCode(const JsonDocument &doc) {
        IRCode code;
        code.protocol = (decode_type_t)doc["protocol"].as<int>();
        code.value = doc["value"].as<uint64_t>();
        code.bits = doc["bits"].as<uint16_t>();
        LOG_DEBUG("[IRManager] Loaded from JSON: protocol=%d, value=0x%llX, bits=%u",
                  (int)code.protocol, code.value, code.bits);
        return code;
    }

    void convertIRCodeToJson(const IRCode &code, JsonDocument &doc) {
        if (code.isValid()) {
            doc["protocol"] = (int)code.protocol;
            doc["value"] = code.value;
            doc["bits"] = code.bits;
            LOG_DEBUG("[IRManager] Saved code to JSON");
        } else {
            LOG_WARN("[IRManager] Skipped saving invalid code");
        }
    }

    bool decode() {
        if (!irrecv) {
            LOG_ERROR("[IRManager] irrecv not initialized");
            return false;
        }

        if (isPaused) {
            LOG_INFO("[IRManager] IR receiver now capturing data");
            resumeIRReceiver();
        }

        if (irrecv->decode(&lastResults)) {
            LOG_DEBUG("[IRManager] IR code received");
            lastCode = fromDecodeResults(lastResults);
            return true;
        }
        return false;
    }

    bool isValid() {
        bool valid = lastCode.isValid();
        LOG_DEBUG("[IRManager] isValid() -> %s", valid ? "true" : "false");
        return valid;
    }

    void sendProtocol(const IRCode &code) {
        if (!irsend) {
            LOG_ERROR("[IRManager] irsend not initialized");
            return;
        }
        if (!code.isValid()) {
            LOG_ERROR("[IRManager] Attempted to send invalid code");
            return;
        }
        LOG_DEBUG("[IRManager] Sending: protocol=%d, value=0x%llX, bits=%u", (int)code.protocol,
                  code.value, code.bits);
        irsend->send(code.protocol, code.value, code.bits);
    }

    void saveLastCodeToJson(JsonDocument &doc) { convertIRCodeToJson(lastCode, doc); }

    void sendFromJson(const JsonDocument &doc) {
        IRCode code = convertJsonToIRCode(doc);
        sendProtocol(code);
    }

    void sendLastCode() { sendProtocol(lastCode); }

    const decode_results &getLastResults() const {
        return lastResults;  // optional getter for debugging
    }
};