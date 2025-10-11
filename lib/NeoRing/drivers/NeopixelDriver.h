#pragma once
#include <Adafruit_NeoPixel.h>
#include "LedDriver.h"

class NeoPixelDriver : public LEDDriver {
   private:
    uint8_t pin;
    size_t ledCount;
    Adafruit_NeoPixel strip;

   public:
    NeoPixelDriver(uint16_t ledCount_, uint8_t pin_)
        : pin(pin_), ledCount(ledCount_), strip(ledCount_, pin_, NEO_GRB + NEO_KHZ800) {
        this->ledCount = ledCount_;
    }
    void begin() override {
        strip.begin();
        strip.show();
    }
    void show(const std::vector<uint32_t>& frame) override {
        for (size_t i = 0; i < frame.size(); ++i) strip.setPixelColor(i, frame[i]);
        strip.show();
    }
    size_t getLedCount() override { return ledCount; }
};
