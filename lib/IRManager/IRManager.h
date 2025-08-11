#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

class IRManager {
   private:
    struct IRCode {
        decode_type_t protocol;
        uint64_t value;
        uint16_t bits;

        bool isValid() const { return (protocol != UNKNOWN) && (bits > 0) && (value != 0); }
    };

    int rxPin;
    int txPin;
    const uint16_t kCaptureBufferSize = 1024;
    const uint8_t kTimeout = 50;
    const uint16_t kMinUnknownSize = 12;
    const uint8_t kTolerancePercentage = kTolerance;

    IRrecv *irrecv = nullptr;
    IRsend *irsend = nullptr;

    decode_results lastResults{};
    IRCode lastCode{};

    IRCode fromDecodeResults(const decode_results &results) {
        IRCode code;
        code.protocol = results.decode_type;
        code.value = results.value;
        code.bits = results.bits;
        Serial.printf("[IRManager] Decoded: protocol=%d, value=0x%llX, bits=%u\n",
                      (int)code.protocol, code.value, code.bits);
        return code;
    }

    IRCode fromJson(const JsonDocument &doc) {
        IRCode code;
        code.protocol = (decode_type_t)doc["protocol"].as<int>();
        code.value = doc["value"].as<uint64_t>();
        code.bits = doc["bits"].as<uint16_t>();
        Serial.printf("[IRManager] Loaded from JSON: protocol=%d, value=0x%llX, bits=%u\n",
                      (int)code.protocol, code.value, code.bits);
        return code;
    }

    void toJson(const IRCode &code, JsonDocument &doc) {
        if (code.isValid()) {
            doc["protocol"] = (int)code.protocol;
            doc["value"] = code.value;
            doc["bits"] = code.bits;
            Serial.println("[IRManager] Saved code to JSON");
        } else {
            Serial.println("[IRManager] Skipped saving invalid code");
        }
    }

    void sendProtocol(const IRCode &code) {
        if (!irsend) {
            Serial.println("[IRManager] ERROR: irsend not initialized");
            return;
        }
        if (!code.isValid()) {
            Serial.println("[IRManager] ERROR: Attempted to send invalid code");
            return;
        }
        Serial.printf("[IRManager] Sending: protocol=%d, value=0x%llX, bits=%u\n",
                      (int)code.protocol, code.value, code.bits);
        irsend->send(code.protocol, code.value, code.bits);
    }

   public:
    IRManager() : rxPin(-1), txPin(-1) {}
    IRManager(int rx, int tx) : rxPin(rx), txPin(tx) {}

    void begin() {
        if (rxPin == -1 || txPin == -1) {
            Serial.println(
                "[IRManager] ERROR: Pins not set. Use begin(rx, tx) or constructor with pins.");
            return;
        }
        Serial.printf("[IRManager] Initializing with RX=%d, TX=%d\n", rxPin, txPin);
        irrecv = new IRrecv(rxPin, kCaptureBufferSize, kTimeout, true);
        irsend = new IRsend(txPin);

        irrecv->setUnknownThreshold(kMinUnknownSize);
        irrecv->setTolerance(kTolerancePercentage);
        irrecv->enableIRIn();
        irsend->begin();
        Serial.println("[IRManager] IR receiver and sender started");
    }

    void begin(int rx, int tx) {
        rxPin = rx;
        txPin = tx;
        begin();
    }

    bool decode() {
        if (!irrecv) {
            Serial.println("[IRManager] ERROR: irrecv not initialized");
            return false;
        }
        if (irrecv->decode(&lastResults)) {
            Serial.println("[IRManager] IR code received");
            lastCode = fromDecodeResults(lastResults);
            return true;
        }
        return false;
    }

    bool isValid() {
        bool valid = lastCode.isValid();
        Serial.printf("[IRManager] isValid() -> %s\n", valid ? "true" : "false");
        return valid;
    }

    void resume() {
        if (irrecv) {
            irrecv->resume();
            Serial.println("[IRManager] IR receiver resumed");
        }
    }

    void saveToJson(JsonDocument &doc) { toJson(lastCode, doc); }

    void sendFromJson(const JsonDocument &doc) {
        IRCode code = fromJson(doc);
        sendProtocol(code);
    }

    void sendLast() { sendProtocol(lastCode); }

    const decode_results &getLastResults() const {
        return lastResults;  // optional getter for debugging
    }
};