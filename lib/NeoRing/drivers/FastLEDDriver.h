#pragma once
#include <Arduino.h>  // for pinMode
#include <FastLED.h>
#include <vector>
#include "LedDriver.h"

class FastLEDDriver : public LEDDriver {
   private:
    size_t ledCount;
    uint8_t pin;
    std::vector<CRGB> leds;  // FastLED uses CRGB objects

   public:
    FastLEDDriver(size_t ledCount_, uint8_t pin_)
        : ledCount(ledCount_), pin(pin_), leds(ledCount_) {}

    void begin() override {
        switch (pin) {
            case D7:
                FastLED.addLeds<WS2812B, /* LED type */
                                D7,      /* pin placeholder, will set in addLeds() */
                                GRB>(leds.data(), ledCount);
                break;
            default:
                // Add support for other pins/types as needed
                FastLED.addLeds<WS2812B, D7, GRB>(leds.data(), ledCount);
                break;
        }

        pinMode(pin, OUTPUT);  // ensure pin is output
        FastLED.clear();
        FastLED.show();
    }

    void show(const std::vector<uint32_t>& frame) override {
        size_t n = std::min(frame.size(), leds.size());
        for (size_t i = 0; i < n; ++i) {
            uint32_t c = frame[i];
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = c & 0xFF;
            leds[i] = CRGB(r, g, b);
        }
        FastLED.show();
    }

    size_t getLedCount() override { return ledCount; }
};
