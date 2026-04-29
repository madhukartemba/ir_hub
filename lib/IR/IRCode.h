#pragma once

#include <ArduinoJson.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

#include <vector>

#include "Log.h"

class IRCode {
   private:
    decode_type_t protocol;
    uint64_t value;
    uint16_t bits;
    String description;
    // For state-based AC protocols (e.g. HITACHI_AC1, DAIKIN, etc.) the
    // payload doesn't fit in `value` and lives in this byte buffer instead.
    // Empty for simple protocols.
    std::vector<uint8_t> state;

    static String stateToHex(const std::vector<uint8_t>& s) {
        String hex;
        hex.reserve(s.size() * 2);
        for (uint8_t b : s) {
            if (b < 0x10) hex += '0';
            hex += String(b, HEX);
        }
        return hex;
    }

    static std::vector<uint8_t> hexToState(const String& hex) {
        std::vector<uint8_t> out;
        out.reserve(hex.length() / 2);
        for (size_t i = 0; i + 1 < hex.length(); i += 2) {
            char buf[3] = {hex[i], hex[i + 1], '\0'};
            out.push_back((uint8_t)strtoul(buf, nullptr, 16));
        }
        return out;
    }

   public:
    IRCode() : protocol(UNKNOWN), value(0), bits(0), description("") {}
    IRCode(decode_type_t protocol, uint64_t value, uint16_t bits)
        : protocol(protocol), value(value), bits(bits), description("") {}
    IRCode(decode_type_t protocol, uint64_t value, uint16_t bits, const String& description)
        : protocol(protocol), value(value), bits(bits), description(description) {}

    decode_type_t getProtocol() const { return protocol; }
    uint64_t getValue() const { return value; }
    uint16_t getBits() const { return bits; }
    String getDescription() const { return description; }
    const std::vector<uint8_t>& getState() const { return state; }
    bool hasState() const { return !state.empty(); }

    void setProtocol(decode_type_t protocol) { this->protocol = protocol; }
    void setValue(uint64_t value) { this->value = value; }
    void setBits(uint16_t bits) { this->bits = bits; }
    void setDescription(const String& description) { this->description = description; }
    void setState(const std::vector<uint8_t>& s) { state = s; }

    bool isValid() const {
        if (protocol == UNKNOWN || bits == 0) return false;
        if (hasACState(protocol)) return !state.empty();
        return value != 0;
    }

    bool operator==(const IRCode& other) const {
        if (protocol != other.protocol || bits != other.bits) return false;
        if (hasACState(protocol)) return state == other.state;
        return value == other.value;
    }

    bool operator!=(const IRCode& other) const { return !(*this == other); }

    JsonDocument toJson() const {
        JsonDocument doc;
        toJson(doc);
        return doc;
    }

    void toJson(JsonDocument& doc) const {
        if (!isValid()) return;
        doc["protocol"] = (int)protocol;
        doc["bits"] = bits;
        doc["description"] = description;
        if (hasACState(protocol)) {
            doc["state"] = stateToHex(state);
        } else {
            doc["value"] = value;
        }
    }

    static IRCode fromJson(const JsonDocument& doc) {
        IRCode code;
        code.protocol = (decode_type_t)doc["protocol"].as<int>();
        code.bits = doc["bits"].as<uint16_t>();
        code.description = doc["description"] | "";
        if (!doc["state"].isNull()) {
            code.state = hexToState(doc["state"].as<String>());
        } else {
            code.value = doc["value"].as<uint64_t>();
        }
        return code;
    }

    static IRCode fromDecodeResults(const decode_results& results) {
        IRCode code;
        code.protocol = results.decode_type;
        code.bits = results.bits;
        code.description = IRAcUtils::resultAcToString(&results);
        if (hasACState(results.decode_type)) {
            // results.state and results.value share memory via a union, so we
            // must read the state buffer (not value) for AC protocols.
            uint16_t nbytes = (results.bits + 7) / 8;
            if (nbytes > kStateSizeMax) nbytes = kStateSizeMax;
            code.state.assign(results.state, results.state + nbytes);
        } else {
            code.value = results.value;
        }
        return code;
    }
};
