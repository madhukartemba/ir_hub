#pragma once
#include <Adafruit_NeoPixel.h>
#include <cmath>  // for sinf
#include "Animation.h"

class Wave : public Animation {
   private:
    float phase = 0.0f;         // current wave phase
    float wavelength;           // number of LEDs per wave cycle
    float amplitude;            // intensity multiplier (0 to 1)
    uint32_t color = 0x0000FF;  // default wave color (blue)

   public:
    // Constructor: pass ledCount, optional wavelength & amplitude & color
    Wave(size_t ledCount_, float wavelength_ = 10.0f, float amplitude_ = 1.0f, uint8_t r = 0,
         uint8_t g = 0, uint8_t b = 255)
        : wavelength(wavelength_), amplitude(amplitude_) {
        ledCount = ledCount_;  // set ledCount first
        setColor(r, g, b);
        buffer.resize(ledCount, 0);  // resize buffer safely
    }

    Wave(size_t ledCount_, float wavelength_, float amplitude_, uint32_t packedColor)
        : wavelength(wavelength_), amplitude(amplitude_), color(packedColor) {
        ledCount = ledCount_;
        color = packedColor;
        buffer.resize(ledCount, 0);
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }
    void update() override {
        if (ledCount == 0) return;

        phase += speed * 0.1f;  // move the wave forward

        // Use total circumference so that wavelength is relative to LED count
        const float twoPi = 2.0f * M_PI;

        for (size_t i = 0; i < ledCount; ++i) {
            // Map LED index to an angle around the ring
            float angle = (twoPi * i) / ledCount;

            // Apply wave function using angle + phase
            float value = (sinf((angle * ledCount / wavelength) + phase) * 0.5f + 0.5f) * amplitude;

            // Compute RGB based on intensity
            uint8_t r = (uint8_t)((color >> 16 & 0xFF) * value);
            uint8_t g = (uint8_t)((color >> 8 & 0xFF) * value);
            uint8_t b = (uint8_t)((color & 0xFF) * value);

            buffer[i] = Adafruit_NeoPixel::Color(r, g, b);
        }
    }
};
