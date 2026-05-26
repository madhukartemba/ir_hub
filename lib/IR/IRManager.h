#pragma once

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>
#include <new>
#include "IRCode.h"
class IRManager {
   private:
    int rxPin;
    int txPin;
    const uint16_t kCaptureBufferSize = 1024;
    const uint8_t kTimeout = 50;
    const uint16_t kMinUnknownSize = 12;
    const uint8_t kTolerancePercentage = 25;  // 25% tolerance

    // IRrecv carries a 1024-entry uint16_t timing buffer plus internal state
    // (~2 KB heap). Only the AddDevice flow needs it, so we allocate lazily on
    // the first startCapture() and free via releaseReceiver() once the user
    // leaves that flow. Steady-state heap recovers ~2 KB.
    IRrecv *irrecv = nullptr;
    IRsend *irsend = nullptr;

    decode_results lastResults{};
    IRCode lastCode{};

    bool isCapturing = false;

    IRCode fromDecodeResults(const decode_results &results) {
        IRCode code = IRCode::fromDecodeResults(results);
        LOG_DEBUG("[IRManager] Decoded: protocol=%d, value=0x%llX, bits=%u",
                  (int)code.getProtocol(), code.getValue(), code.getBits());
        return code;
    }

    bool ensureReceiver() {
        if (irrecv) return true;
        if (rxPin == -1) {
            LOG_ERROR("[IRManager] Cannot allocate receiver: rxPin not set");
            return false;
        }
        irrecv = new (std::nothrow) IRrecv(rxPin, kCaptureBufferSize, kTimeout, true);
        if (!irrecv) {
            LOG_ERROR("[IRManager] Failed to allocate IRrecv (free heap=%u)",
                      (unsigned)ESP.getFreeHeap());
            return false;
        }
        irrecv->setUnknownThreshold(kMinUnknownSize);
        irrecv->setTolerance(kTolerancePercentage);
        LOG_INFO("[IRManager] IRrecv allocated (free heap=%u)", (unsigned)ESP.getFreeHeap());
        return true;
    }

   public:
    IRManager() : rxPin(-1), txPin(-1) {}
    IRManager(int rx, int tx) : rxPin(rx), txPin(tx) {}

    bool begin() {
        if (rxPin == -1 || txPin == -1) {
            LOG_ERROR("[IRManager] Pins not set. Use begin(rx, tx) or constructor with pins.");
            return false;
        }
        LOG_INFO("[IRManager] Initializing TX=%d (RX=%d allocated lazily on first capture)",
                 txPin, rxPin);
        irsend = new IRsend(txPin);
        irsend->begin();
        return true;
    }

    bool begin(int rx, int tx) {
        rxPin = rx;
        txPin = tx;
        return begin();
    }

    void stopCapture() {
        if (irrecv) {
            irrecv->disableIRIn();
        }
        isCapturing = false;
    }

    void startCapture() {
        if (!ensureReceiver()) return;
        irrecv->enableIRIn();
        isCapturing = true;
    }

    void releaseReceiver() {
        if (!irrecv) return;
        irrecv->disableIRIn();
        delete irrecv;
        irrecv = nullptr;
        isCapturing = false;
        LOG_INFO("[IRManager] IRrecv released (free heap=%u)", (unsigned)ESP.getFreeHeap());
    }

    bool decode() {
        if (!isCapturing) {
            LOG_INFO("[IRManager] IR receiver now capturing data");
            startCapture();
        }
        if (!irrecv) {
            return false;
        }

        if (irrecv->decode(&lastResults)) {
            LOG_DEBUG("[IRManager] IR code received");
            lastCode = fromDecodeResults(lastResults);
            stopCapture();
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
        // State-based AC protocols (HITACHI_AC1, DAIKIN, etc.) carry payloads
        // bigger than 64 bits, so they must go through the byte-array
        // overload of IRsend::send. The simple uint64_t overload doesn't
        // even have switch cases for them and would silently emit nothing.
        if (hasACState(code.getProtocol())) {
            const auto &state = code.getState();
            LOG_DEBUG("[IRManager] Sending: protocol=%d, state_bytes=%u, bits=%u",
                      (int)code.getProtocol(), (unsigned)state.size(), code.getBits());
            irsend->send(code.getProtocol(), state.data(),
                         static_cast<uint16_t>(state.size()));
        } else {
            LOG_DEBUG("[IRManager] Sending: protocol=%d, value=0x%llX, bits=%u",
                      (int)code.getProtocol(), code.getValue(), code.getBits());
            irsend->send(code.getProtocol(), code.getValue(), code.getBits());
        }
    }

    void saveLastCodeToJson(JsonDocument &doc) { lastCode.toJson(doc); }

    void sendLastCode() { sendProtocol(lastCode); }

    const decode_results &getLastResults() const {
        return lastResults;  // optional getter for debugging
    }

    const IRCode &getLastCode() const { return lastCode; }
};
