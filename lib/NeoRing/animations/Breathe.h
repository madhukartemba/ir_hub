#pragma once
#include <Adafruit_NeoPixel.h>
#include <cmath>  // for sinf
#include "Animation.h"

class Breathe : public Animation {
   private:
    float phase = 0.0f;           // phase of the breathing cycle
    uint32_t color = 0xFFFFFFFF;  // default color (white)
    float minBrightness = 0.5f;   // minimum brightness (0.0 to 1.0)

   public:
    // Constructor: pass ledCount and optional initial color
    Breathe(size_t ledCount_, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
        ledCount = ledCount_;  // set ledCount first
        setColor(r, g, b);
        buffer.resize(ledCount, color);
    }

    Breathe(size_t ledCount_, uint32_t packedColor, float minBrightness_ = 0.5f) {
        ledCount = ledCount_;
        color = packedColor;
        minBrightness =
            minBrightness_ < 0.0f ? 0.0f : (minBrightness_ > 1.0f ? 1.0f : minBrightness_);
        buffer.resize(ledCount, color);
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }

    void setMinBrightness(float minBright) {
        // Clamp between 0.0 and 1.0
        minBrightness = minBright < 0.0f ? 0.0f : (minBright > 1.0f ? 1.0f : minBright);
    }

    void update() override {
        if (ledCount == 0) return;

        // Advance the phase based on speed
        phase += speed * 0.05f;  // smaller increment for slower breathing
        if (phase > 2 * 3.14159265f) phase -= 2 * 3.14159265f;  // wrap around

        // Compute brightness using sine wave, scaled between minBrightness and 1.0
        float rawBrightness = (sinf(phase - 3.14159265f / 2) * 0.5f + 0.5f);
        float brightness = minBrightness + (1.0f - minBrightness) * rawBrightness;

        // Apply brightness to each LED
        uint8_t r = static_cast<uint8_t>(((color >> 16) & 0xFF) * brightness);
        uint8_t g = static_cast<uint8_t>(((color >> 8) & 0xFF) * brightness);
        uint8_t b = static_cast<uint8_t>((color & 0xFF) * brightness);

        for (size_t i = 0; i < ledCount; ++i) {
            buffer[i] = Adafruit_NeoPixel::Color(r, g, b);
        }
    }
};
