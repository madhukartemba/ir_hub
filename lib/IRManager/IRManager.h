#pragma once
#include <Arduino.h>

#define FEEDBACK_LED_IS_ACTIVE_LOW
#define _IR_TIMING_TEST_PIN LED_BUILTIN
// #define tone(...) void()  // tone() inhibits receive timer
// #define noTone(a) void()

#define RAW_BUFFER_LENGTH 250

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
            doc["raw_data_len"] = irData.rawDataPtr->rawlen;

            // Create raw data array
            JsonArray rawDataArray = doc.createNestedArray("raw_data");
            for (uint16_t i = 0; i < irData.rawDataPtr->rawlen; i++) {
                rawDataArray.add(irData.rawDataPtr->rawbuf[i]);
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
        if (!doc.containsKey("protocol") || !doc.containsKey("address") ||
            !doc.containsKey("command") || !doc.containsKey("number_of_bits")) {
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
        if (doc.containsKey("flags")) {
            irData.flags = doc["flags"].as<uint8_t>();
        }
        if (doc.containsKey("decoded_raw_data")) {
            irData.decodedRawData = doc["decoded_raw_data"].as<uint32_t>();
        }

        // Handle raw data if present
        if (doc.containsKey("raw_data") && doc["raw_data"].is<JsonArray>()) {
            JsonArray rawDataArray = doc["raw_data"];
            uint16_t rawLen = doc["raw_data_len"].as<uint16_t>();

            if (rawLen > 0 && rawLen <= RAW_BUFFER_LENGTH) {
                // Allocate raw data buffer
                irData.rawDataPtr = new irparams_struct();
                irData.rawDataPtr->rawlen = rawLen;

                // Copy raw data from JSON array
                for (uint16_t i = 0; i < rawLen && i < rawDataArray.size(); i++) {
                    irData.rawDataPtr->rawbuf[i] =
                        rawDataArray[i].as<uint16_t>() * 50;  // 50us pulse duration
                }
            }
        }

        return true;
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

    IRData getRecordedData() { return IrReceiver.decodedIRData; }

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

    void sendIRData(IRData& irData) {
        IrReceiver.stop();

        if (irData.protocol == UNKNOWN || irData.protocol == PULSE_WIDTH ||
            irData.protocol == PULSE_DISTANCE) {
            // Assume 38 KHz for raw protocols
            if (irData.rawDataPtr != nullptr) {
                IrSender.sendRaw(irData.rawDataPtr->rawbuf, irData.rawDataPtr->rawlen, 38);
            }
        } else {
            // Use standard protocol sending
            IrSender.write(&irData);
        }
    }

    void sendIRData(JsonDocument& doc) {
        IRData irData;
        convertFromJson(doc, irData);
        sendIRData(irData);
    }
};