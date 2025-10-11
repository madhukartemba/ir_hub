#pragma once
#include <Adafruit_NeoPixel.h>
#include <math.h>  // for sinf
#include "Animation.h"

class Breathe : public Animation {
   private:
    float phase = 0.0f;           // phase of the breathing cycle
    uint32_t color = 0xFFFFFFFF;  // default color (white)

   public:
    // Constructor: optional initial color
    Breathe(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) { setColor(r, g, b); }

    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }

    void update() override {
        if (ledCount == 0) return;

        // Advance the phase based on speed
        phase += speed * 0.05f;  // smaller increment for slower breathing
        if (phase > 2 * 3.14159265f) phase -= 2 * 3.14159265f;  // wrap around

        // Compute brightness using sine wave (0..1)
        float brightness = (sinf(phase - 3.14159265f / 2) * 0.5f + 0.5f);

        // Apply brightness to each LED
        uint8_t r = ((color >> 16) & 0xFF) * brightness;
        uint8_t g = ((color >> 8) & 0xFF) * brightness;
        uint8_t b = (color & 0xFF) * brightness;

        for (size_t i = 0; i < ledCount; ++i) {
            buffer[i] = Adafruit_NeoPixel::Color(r, g, b);
        }
    }
};
