#pragma once
#include <Arduino.h>

#define FEEDBACK_LED_IS_ACTIVE_LOW
#define _IR_TIMING_TEST_PIN LED_BUILTIN
// #define tone(...) void()  // tone() inhibits receive timer
// #define noTone(a) void()

#define RAW_BUFFER_LENGTH 250

#include <IRremote.hpp>
#include "IdGen.h"
#include "Log.h"

class IRManager {
   private:
    int irTxPin;
    int irRxPin;
    IdGen* idGen;

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
};