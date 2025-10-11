#pragma once
#include <Adafruit_NeoPixel.h>
#include <math.h>  // for sinf
#include "Animation.h"

class Wave : public Animation {
   private:
    float phase = 0.0f;         // current wave phase
    float wavelength;           // number of LEDs per wave cycle
    float amplitude;            // intensity multiplier (0 to 1)
    uint32_t color = 0x0000FF;  // default wave color (blue)

   public:
    // Constructor: optional wavelength & amplitude
    Wave(float wavelength_ = 10.0f, float amplitude_ = 1.0f, uint8_t r = 0, uint8_t g = 0,
         uint8_t b = 255)
        : wavelength(wavelength_), amplitude(amplitude_) {
        color = Adafruit_NeoPixel::Color(r, g, b);
    }

    Wave(float wavelength_, float amplitude_, uint32_t packedColor)
        : wavelength(wavelength_), amplitude(amplitude_), color(packedColor) {}

    void update() override {
        if (ledCount == 0) return;

        phase += speed * 0.1f;  // move the wave forward
        for (size_t i = 0; i < ledCount; ++i) {
            // Compute sine wave value
            float value = (sinf((i / wavelength) + phase) * 0.5f + 0.5f) * amplitude;

            // Map value to color (e.g., blue wave)
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = static_cast<uint8_t>(value * 255);

            buffer[i] = Adafruit_NeoPixel::Color(r, g, b);
        }
    }
};
