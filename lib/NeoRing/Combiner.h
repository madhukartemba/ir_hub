#pragma once
#include <cstdint>
#include <vector>

class Combiner {
   public:
    // Blend two frames according to fadeValue (0 → 1)
    static std::vector<uint32_t> blend(const std::vector<uint32_t>& currentFrame,
                                       const std::vector<uint32_t>& nextFrame, float fadeValue) {
        size_t n = currentFrame.size();
        std::vector<uint32_t> output(n);

        for (size_t i = 0; i < n; ++i) {
            output[i] = lerpColor(currentFrame[i], nextFrame[i], fadeValue);
        }

        return output;
    }

   private:
    // Linear interpolation between two RGB colors
    static uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
        // Extract RGB
        uint8_t r1 = (c1 >> 16) & 0xFF;
        uint8_t g1 = (c1 >> 8) & 0xFF;
        uint8_t b1 = c1 & 0xFF;

        uint8_t r2 = (c2 >> 16) & 0xFF;
        uint8_t g2 = (c2 >> 8) & 0xFF;
        uint8_t b2 = c2 & 0xFF;

        // Linear interpolation
        uint8_t r = r1 + (r2 - r1) * t;
        uint8_t g = g1 + (g2 - g1) * t;
        uint8_t b = b1 + (b2 - b1) * t;

        return (r << 16) | (g << 8) | b;
    }
};
