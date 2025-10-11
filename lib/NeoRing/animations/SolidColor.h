#pragma once
#include <Adafruit_NeoPixel.h>  // for Adafruit_NeoPixel::Color
#include <cstdint>
#include "Animation.h"

class SolidColor : public Animation {
   private:
    uint32_t color = 0;  // packed NeoPixel color

   public:
    // Constructor to set initial color
    SolidColor(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) { setColor(r, g, b); }

    // Set/change the color dynamically
    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }

    void update() override {
        if (ledCount == 0) return;
        for (size_t i = 0; i < ledCount; ++i) {
            buffer[i] = color;
        }
    }
};
