#pragma once
#include <Adafruit_NeoPixel.h>
#include <cstdint>
#include "Animation.h"

class Rainbow : public Animation {
   private:
    uint16_t hueOffset = 0;  // current hue position

   public:
    // Constructor
    Rainbow(size_t ledCount_, float speed_ = 1.0f) {
        ledCount = ledCount_;
        speed = speed_;
        buffer.resize(ledCount, 0);
    }

    void update() override {
        if (ledCount == 0) return;

        // Advance hue based on speed (scale to hue units)
        hueOffset += speed * 256;  // 256 units per frame at speed=1
        hueOffset %= 65536;        // wrap around 0-65535 (Adafruit_NeoPixel::ColorHSV range)

        for (size_t i = 0; i < ledCount; ++i) {
            // Hue shifts along the strip
            uint16_t hue = (hueOffset + (i * 65536UL / ledCount)) % 65536;
            buffer[i] = Adafruit_NeoPixel::ColorHSV(hue);
        }
    }
};
