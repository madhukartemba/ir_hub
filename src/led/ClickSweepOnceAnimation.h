#pragma once

#include <Adafruit_NeoPixel.h>
#include <Animation.h>
#include <algorithm>
#include <cmath>

// Single pass: bright pulse travels along indices (ledCount−1) → 0, then holds black.
class ClickSweepOnceAnimation : public Animation {
   private:
    uint8_t cr;
    uint8_t cg;
    uint8_t cb;
    float head;
    float sigma;
    float step;
    float doneHead;

   public:
    ClickSweepOnceAnimation(size_t ledCount_, uint32_t packedColor)
        : cr(static_cast<uint8_t>((packedColor >> 16) & 0xFF)),
          cg(static_cast<uint8_t>((packedColor >> 8) & 0xFF)),
          cb(static_cast<uint8_t>(packedColor & 0xFF)),
          head(0.0f),
          sigma(std::max(1.2f, static_cast<float>(ledCount_) * 0.12f)),
          step(0.32f),
          doneHead(0.0f) {
        ledCount = ledCount_;
        buffer.resize(ledCount, 0);
        head = static_cast<float>(ledCount) + 4.0f + sigma;
        doneHead = -4.0f - sigma;
    }

    void update() override {
        if (ledCount == 0) return;

        if (m_isStatic) return;

        head -= step * speed;

        if (head <= doneHead) {
            const uint32_t black = Adafruit_NeoPixel::Color(0, 0, 0);
            std::fill(buffer.begin(), buffer.end(), black);
            m_isStatic = true;
            return;
        }

        const float twoSigmaSq = 2.0f * sigma * sigma;
        for (size_t i = 0; i < ledCount; ++i) {
            const float d = fabsf(static_cast<float>(i) - head);
            float intensity = expf(-(d * d) / twoSigmaSq);
            intensity = (intensity > 1.0f) ? 1.0f : intensity;
            buffer[i] = Adafruit_NeoPixel::Color(static_cast<uint8_t>(cr * intensity),
                                                 static_cast<uint8_t>(cg * intensity),
                                                 static_cast<uint8_t>(cb * intensity));
        }
    }
};
