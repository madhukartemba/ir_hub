#pragma once

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>

#include "Log.h"

class IRCode {
   private:
    decode_type_t protocol;
    uint64_t value;
    uint16_t bits;

   public:
    // Constructors
    IRCode() : protocol(UNKNOWN), value(0), bits(0) {}
    IRCode(decode_type_t protocol, uint64_t value, uint16_t bits)
        : protocol(protocol), value(value), bits(bits) {}

    // Getters
    decode_type_t getProtocol() const { return protocol; }
    uint64_t getValue() const { return value; }
    uint16_t getBits() const { return bits; }

    // Setters
    void setProtocol(decode_type_t protocol) { this->protocol = protocol; }
    void setValue(uint64_t value) { this->value = value; }
    void setBits(uint16_t bits) { this->bits = bits; }

    // Validation
    bool isValid() const { return (protocol != UNKNOWN) && (bits > 0) && (value != 0); }

    JsonDocument toJson() const {
        JsonDocument doc;
        toJson(doc);
        return doc;
    }

    void toJson(JsonDocument& doc) const {
        if (isValid()) {
            doc["protocol"] = (int)protocol;
            doc["value"] = value;
            doc["bits"] = bits;
        }
    }

    static IRCode fromJson(const JsonDocument& doc) {
        IRCode code;
        code.protocol = (decode_type_t)doc["protocol"].as<int>();
        code.value = doc["value"].as<uint64_t>();
        code.bits = doc["bits"].as<uint16_t>();
        return code;
    }

    // Factory method from decode_results
    static IRCode fromDecodeResults(const decode_results& results) {
        IRCode code;
        code.protocol = results.decode_type;
        code.value = results.value;
        code.bits = results.bits;
        return code;
    }
};
