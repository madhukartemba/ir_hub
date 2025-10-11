#pragma once
#include <Adafruit_NeoPixel.h>
#include <cmath>  // for fmodf
#include "Animation.h"

class Spinner : public Animation {
   private:
    float position = 0.0f;       // current position of spinner head
    size_t trailLength;          // number of LEDs in the trail
    uint32_t color;              // spinner color
    bool bidirectional = false;  // whether to bounce back and forth

    // Helper function to add two colors (capped at 255 per channel)
    uint32_t addColors(uint32_t c1, uint32_t c2) {
        uint8_t r1 = (c1 >> 16) & 0xFF;
        uint8_t g1 = (c1 >> 8) & 0xFF;
        uint8_t b1 = c1 & 0xFF;

        uint8_t r2 = (c2 >> 16) & 0xFF;
        uint8_t g2 = (c2 >> 8) & 0xFF;
        uint8_t b2 = c2 & 0xFF;

        uint8_t r = (r1 + r2 > 255) ? 255 : r1 + r2;
        uint8_t g = (g1 + g2 > 255) ? 255 : g1 + g2;
        uint8_t b = (b1 + b2 > 255) ? 255 : b1 + b2;

        return Adafruit_NeoPixel::Color(r, g, b);
    }

   public:
    // Constructor: pass ledCount, optional trail length & color
    Spinner(size_t ledCount_, size_t trailLength_ = 5, uint8_t r = 0, uint8_t g = 150,
            uint8_t b = 255)
        : trailLength(trailLength_) {
        ledCount = ledCount_;
        setColor(r, g, b);
        buffer.resize(ledCount, 0);
        // Ensure trail length doesn't exceed LED count
        if (trailLength > ledCount) {
            trailLength = ledCount / 2;
        }
    }

    Spinner(size_t ledCount_, size_t trailLength_, uint32_t packedColor)
        : trailLength(trailLength_), color(packedColor) {
        ledCount = ledCount_;
        buffer.resize(ledCount, 0);
        if (trailLength > ledCount) {
            trailLength = ledCount / 2;
        }
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }

    void setTrailLength(size_t length) {
        trailLength = length;
        if (trailLength > ledCount) {
            trailLength = ledCount / 2;
        }
    }

    void setBidirectional(bool enabled) { bidirectional = enabled; }

    void update() override {
        if (ledCount == 0) return;

        // Clear the buffer
        std::fill(buffer.begin(), buffer.end(), 0);

        // Advance the spinner position
        position += speed * 0.5f;
        if (position >= ledCount) {
            position = fmodf(position, (float)ledCount);
        }

        // Extract RGB components from packed color
        uint8_t baseR = (color >> 16) & 0xFF;
        uint8_t baseG = (color >> 8) & 0xFF;
        uint8_t baseB = color & 0xFF;

        // Draw the spinner trail with gradient and sub-pixel smoothing
        for (size_t i = 0; i < trailLength; ++i) {
            // Calculate trail position (floating point for smooth interpolation)
            float trailPos = position - i;

            // Get integer part and fractional part for interpolation
            float wrappedPos = fmodf(trailPos + ledCount * 10.0f, (float)ledCount);
            int ledIndex1 = (int)wrappedPos;
            int ledIndex2 = (ledIndex1 + 1) % ledCount;
            float fraction = wrappedPos - (float)ledIndex1;

            // Calculate brightness based on position in trail (head is brightest)
            float brightness = (float)(trailLength - i) / (float)trailLength;
            brightness = brightness * brightness;  // Square for smoother falloff

            // Apply brightness to color for both adjacent LEDs
            uint8_t r = (uint8_t)(baseR * brightness);
            uint8_t g = (uint8_t)(baseG * brightness);
            uint8_t b = (uint8_t)(baseB * brightness);

            // Interpolate between adjacent LEDs for smooth sub-pixel movement
            uint32_t color1 = Adafruit_NeoPixel::Color((uint8_t)(r * (1.0f - fraction)),
                                                       (uint8_t)(g * (1.0f - fraction)),
                                                       (uint8_t)(b * (1.0f - fraction)));
            uint32_t color2 = Adafruit_NeoPixel::Color(
                (uint8_t)(r * fraction), (uint8_t)(g * fraction), (uint8_t)(b * fraction));

            // Blend with existing colors (additive blending for overlapping trails)
            buffer[ledIndex1] = addColors(buffer[ledIndex1], color1);
            buffer[ledIndex2] = addColors(buffer[ledIndex2], color2);
        }
    }

    void reset() override {
        Animation::reset();
        position = 0.0f;
    }
};
