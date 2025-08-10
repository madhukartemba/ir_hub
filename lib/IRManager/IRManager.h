#pragma once
#include <Arduino.h>

#define FEEDBACK_LED_IS_ACTIVE_LOW
#define _IR_TIMING_TEST_PIN LED_BUILTIN
// #define tone(...) void()  // tone() inhibits receive timer
// #define noTone(a) void()

#define RAW_BUFFER_LENGTH 250
#define EXCLUDE_EXOTIC_PROTOCOLS

#include <ArduinoJson.h>
#include <IRremote.hpp>
#include "IdGen.h"
#include "Log.h"

class IRManager {
   private:
    int irTxPin;
    int irRxPin;
    IdGen* idGen;

    /**
     * Get protocol name as string
     * @param protocol Protocol enum value
     * @return Protocol name as string
     */
    const char* getProtocolString(decode_type_t protocol) {
        switch (protocol) {
            case NEC:
                return "NEC";
            case SONY:
                return "SONY";
            case RC5:
                return "RC5";
            case RC6:
                return "RC6";
            case PANASONIC:
                return "PANASONIC";
            case JVC:
                return "JVC";
            case SAMSUNG:
                return "SAMSUNG";
            case LG:
                return "LG";
            case DENON:
                return "DENON";
            case SHARP:
                return "SHARP";
            case BOSEWAVE:
                return "BOSEWAVE";
            case LEGO_PF:
                return "LEGO_PF";
            case MAGIQUEST:
                return "MAGIQUEST";
            case WHYNTER:
                return "WHYNTER";
            case KASEIKYO:
                return "KASEIKYO";
            case KASEIKYO_JVC:
                return "KASEIKYO_JVC";
            case KASEIKYO_DENON:
                return "KASEIKYO_DENON";
            case KASEIKYO_MITSUBISHI:
                return "KASEIKYO_MITSUBISHI";
            case KASEIKYO_SHARP:
                return "KASEIKYO_SHARP";
            case UNKNOWN:
                return "UNKNOWN";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * Convert IRData object to ArduinoJson document
     * @param doc Reference to JsonDocument to store the data
     * @param irData IRData object to convert
     * @return true if conversion was successful, false otherwise
     */
    bool convertToJson(JsonDocument& doc, const IRData& irData) {
        // Clear the document
        doc.clear();

        // Add protocol information
        doc["protocol"] = irData.protocol;
        doc["protocol_name"] = getProtocolString(irData.protocol);

        // Add address and command
        doc["address"] = irData.address;
        doc["command"] = irData.command;

        // Add number of bits
        doc["number_of_bits"] = irData.numberOfBits;

        // Add raw data information
        if (irData.rawDataPtr != nullptr) {
            if (irData.protocol == UNKNOWN || irData.protocol == PULSE_WIDTH ||
                irData.protocol == PULSE_DISTANCE) {
                // For RAW protocols, use compensated data like the example
                uint8_t compensatedRawCode[RAW_BUFFER_LENGTH];
                IrReceiver.compensateAndStoreIRResultInArray(compensatedRawCode);
                int rawLen = irData.rawDataPtr->rawlen;
                doc["raw_data_len"] = rawLen;

                // Create raw data array, store as microseconds (like the example)
                JsonArray rawDataArray = doc["raw_data"].to<JsonArray>();
                for (uint16_t i = 0; i < rawLen; i++) {
                    rawDataArray.add((uint16_t)compensatedRawCode[i]);
                }
            } else {
                // For known protocols, store the raw buffer as-is
                doc["raw_data_len"] = irData.rawDataPtr->rawlen;
                JsonArray rawDataArray = doc["raw_data"].to<JsonArray>();
                for (uint16_t i = 0; i < irData.rawDataPtr->rawlen; i++) {
                    rawDataArray.add((uint16_t)irData.rawDataPtr->rawbuf[i]);
                }
            }
        } else {
            doc["raw_data_len"] = 0;
            doc["raw_data"] = JsonArray();
        }

        // Add additional useful information
        doc["flags"] = irData.flags;
        doc["decoded_raw_data"] = irData.decodedRawData;

        return true;
    }

    /**
     * Convert JSON document back to IRData object
     * @param doc JsonDocument containing IR data
     * @param irData Reference to IRData object to populate
     * @return true if conversion was successful, false otherwise
     */
    bool convertFromJson(JsonDocument& doc, IRData& irData) {
        // Validate required fields exist
        if (!doc["protocol"].is<int>() || !doc["address"].is<uint16_t>() ||
            !doc["command"].is<uint16_t>() || !doc["number_of_bits"].is<uint8_t>()) {
            LOG_ERROR("Missing required fields in JSON document");
            return false;
        }

        // Clear the IRData structure
        memset(&irData, 0, sizeof(IRData));

        // Set basic fields
        irData.protocol = static_cast<decode_type_t>(doc["protocol"].as<int>());
        irData.address = doc["address"].as<uint16_t>();
        irData.command = doc["command"].as<uint16_t>();
        irData.numberOfBits = doc["number_of_bits"].as<uint8_t>();

        // Set optional fields if they exist
        if (doc["flags"].is<uint8_t>()) {
            irData.flags = doc["flags"].as<uint8_t>();
        }
        if (doc["decoded_raw_data"].is<uint32_t>()) {
            irData.decodedRawData = doc["decoded_raw_data"].as<uint32_t>();
        }

        // Handle raw data if present
        if (doc["raw_data"].is<JsonArray>()) {
            JsonArray rawDataArray = doc["raw_data"];
            uint16_t rawLen = doc["raw_data_len"].as<uint16_t>();

            if (rawLen > 0 && rawLen <= RAW_BUFFER_LENGTH) {
                // Allocate raw data buffer
                irData.rawDataPtr = new irparams_struct();
                irData.rawDataPtr->rawlen = rawLen;

                // Copy raw data from JSON array
                for (uint16_t i = 0; i < rawLen && i < rawDataArray.size(); i++) {
                    irData.rawDataPtr->rawbuf[i] = rawDataArray[i].as<uint16_t>();
                }
            }
        }

        return true;
    }

    void debugPrintIRData(const IRData& data, const char* label) {
        LOG_INFO("---- IRData Dump (%s) ----", label);
        LOG_INFO("Protocol: %d (%s)", data.protocol, getProtocolString(data.protocol));
        LOG_INFO("Address: 0x%X", data.address);
        LOG_INFO("Command: 0x%X", data.command);
        LOG_INFO("Number of bits: %d", data.numberOfBits);
        LOG_INFO("Flags: 0x%X", data.flags);
        LOG_INFO("Decoded raw data: 0x%lX", data.decodedRawData);

        if (data.rawDataPtr) {
            LOG_INFO("Raw length: %d", data.rawDataPtr->rawlen);
            String rawBufStr;
            for (uint16_t i = 0; i < data.rawDataPtr->rawlen; i++) {
                rawBufStr += String(data.rawDataPtr->rawbuf[i]);
                if (i < data.rawDataPtr->rawlen - 1) rawBufStr += ", ";
            }
            LOG_INFO("Raw buffer: [%s]", rawBufStr.c_str());
        } else {
            LOG_INFO("Raw data pointer: NULL");
        }
        LOG_INFO("--------------------------");
    }

   public:
    IRManager() : idGen(nullptr) {}
    ~IRManager() {}

    void begin(int rxPin, int txPin, IdGen& idGen) {
        this->irRxPin = rxPin;
        this->irTxPin = txPin;
        this->idGen = &idGen;
        IrReceiver.begin(irRxPin, true);
        IrSender.begin(irTxPin);
    }

    void startRecording() { IrReceiver.start(); }

    bool record() { return IrReceiver.decode(); }

    bool isRecordedDataValid() {
        if (IrReceiver.decodedIRData.rawDataPtr->rawlen < 4) {
            LOG_WARN("Ignore data with rawlen=%d", IrReceiver.decodedIRData.rawDataPtr->rawlen);
            return false;
        }
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
            LOG_WARN("Ignore repeat");
            return false;
        }
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_AUTO_REPEAT) {
            LOG_WARN("Ignore autorepeat");
            return false;
        }
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_PARITY_FAILED) {
            LOG_WARN("Ignore parity error");
            return false;
        }
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
            LOG_WARN("Overflow occurred, raw data did not fit into %d byte raw buffer",
                     RAW_BUFFER_LENGTH);
            return false;
        }
        return true;
    }

    void sendIRData(IRData& irData) {
        IrReceiver.stop();

        LOG_INFO("Sending IR data: protocol=%s, address=0x%X, command=0x%X, bits=%d",
                 getProtocolString(irData.protocol), irData.address, irData.command,
                 irData.numberOfBits);

        if (irData.protocol == UNKNOWN || irData.protocol == PULSE_WIDTH ||
            irData.protocol == PULSE_DISTANCE) {
            // RAW protocol handling
            if (irData.rawDataPtr != nullptr) {
                String rawBufStr;
                IRRawbufType* rawbuf = irData.rawDataPtr->rawbuf;
                uint16_t rawlen = irData.rawDataPtr->rawlen;

                for (uint16_t i = 0; i < rawlen; i++) {
                    rawBufStr += String(rawbuf[i]);
                    if (i < rawlen - 1) rawBufStr += ", ";
                }
                LOG_DEBUG("Sending RAW buffer: [%s]", rawBufStr.c_str());
                IrSender.sendRaw(rawbuf, rawlen, 38);  // 38kHz default
                LOG_INFO("Sent RAW IR code");
            } else {
                LOG_WARN("No raw data available for RAW protocol");
            }
        } else {
            // Known protocol handling
            LOG_DEBUG("Sending IR code using protocol: %s", getProtocolString(irData.protocol));
            IrSender.write(&irData);
            LOG_INFO("Sent IR code using protocol: %s", getProtocolString(irData.protocol));
        }
    }

    IRData getRecordedData() {
        IRData data = IrReceiver.decodedIRData;
        if (data.rawDataPtr != nullptr) {
            String rawBufStr;
            for (uint16_t i = 0; i < data.rawDataPtr->rawlen; i++) {
                rawBufStr += String(data.rawDataPtr->rawbuf[i]);
                if (i < data.rawDataPtr->rawlen - 1) rawBufStr += ",";
            }
            LOG_DEBUG("Received rawbuf: [%s]", rawBufStr.c_str());
        }
        debugPrintIRData(data, "RECEIVED");
        return data;
    }

    /**
     * Export recorded IR data to a new JsonDocument
     * @return JsonDocument containing the recorded IR data, or empty document if no valid data
     */
    JsonDocument exportRecordedDataToJson() {
        JsonDocument doc;

        // Check if we have valid recorded data
        if (!isRecordedDataValid()) {
            LOG_WARN("No valid recorded IR data available");
            return doc;  // Return empty document
        }

        // Get the recorded data and convert to JSON
        IRData recordedData = getRecordedData();
        if (convertToJson(doc, recordedData)) {
            return doc;
        } else {
            LOG_ERROR("Failed to convert IR data to JSON");
            doc.clear();
            return doc;
        }
    }

    /**
     * Import IRData object from JSON document
     * @param doc JsonDocument containing IR data
     * @return IRData object, or empty IRData if conversion fails
     */
    IRData importIRDataFromJson(JsonDocument& doc) {
        IRData irData;
        if (convertFromJson(doc, irData)) {
            return irData;
        } else {
            // Return empty IRData on failure
            memset(&irData, 0, sizeof(IRData));
            return irData;
        }
    }

    void sendIRData(JsonDocument& doc) {
        LOG_INFO("Preparing to send IR data from JSON");

        // Print the JSON document
        String jsonStr;
        serializeJson(doc, jsonStr);
        LOG_DEBUG("JSON Document: %s", jsonStr.c_str());

        IRData irData;
        if (convertFromJson(doc, irData)) {
            LOG_DEBUG("Converted JSON to IRData, protocol=%s", getProtocolString(irData.protocol));
            debugPrintIRData(irData, "SENT");
            sendIRData(irData);
        } else {
            LOG_ERROR("Failed to convert JSON to IRData");
        }
    }
};