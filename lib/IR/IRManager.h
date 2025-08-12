#pragma once

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "IRCode.h"
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
        IRCode code = IRCode::fromDecodeResults(results);
        LOG_DEBUG("[IRManager] Decoded: protocol=%d, value=0x%llX, bits=%u",
                  (int)code.getProtocol(), code.getValue(), code.getBits());
        return code;
    }

   public:
    IRManager() : rxPin(-1), txPin(-1) {}
    IRManager(int rx, int tx) : rxPin(rx), txPin(tx) {}

    bool begin() {
        if (rxPin == -1 || txPin == -1) {
            LOG_ERROR("[IRManager] Pins not set. Use begin(rx, tx) or constructor with pins.");
            return false;
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
        return true;
    }

    bool begin(int rx, int tx) {
        rxPin = rx;
        txPin = tx;
        return begin();
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
        LOG_DEBUG("[IRManager] Sending: protocol=%d, value=0x%llX, bits=%u",
                  (int)code.getProtocol(), code.getValue(), code.getBits());
        irsend->send(code.getProtocol(), code.getValue(), code.getBits());
    }

    void saveLastCodeToJson(JsonDocument &doc) { lastCode.toJson(doc); }

    void sendLastCode() { sendProtocol(lastCode); }

    const decode_results &getLastResults() const {
        return lastResults;  // optional getter for debugging
    }
};