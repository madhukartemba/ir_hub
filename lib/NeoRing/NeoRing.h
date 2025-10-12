#pragma once
#include <Arduino.h>
#include <cstdint>
#include <memory>
#include "AnimationEngine.h"
#include "Color.h"
#include "animations/Breathe.h"
#include "animations/Rainbow.h"
#include "animations/SolidColor.h"
#include "animations/Spinner.h"
#include "animations/Wave.h"
#include "drivers/FastLEDDriver.h"
#include "drivers/NeopixelDriver.h"

enum class DisplayDriver { NEOPIXEL, FASTLED };

class NeoRing {
   private:
    size_t ledCount;
    uint8_t pin;

    unsigned long lastUpdate = 0;  // last update timestamp in ms
    float fps = 60.0f;             // default frames per second
    float frameInterval;           // ms per frame, initialized in constructor

    std::unique_ptr<AnimationEngine> engine;

   public:
    // Empty constructor
    NeoRing() : ledCount(0), pin(0), frameInterval(1000.0f / fps) {}

    // Constructor: takes number of LEDs and pin
    NeoRing(size_t ledCount_, uint8_t pin_)
        : ledCount(ledCount_), pin(pin_), frameInterval(1000.0f / fps) {}

    // Initialize hardware
    void begin(size_t ledCount_ = 0, uint8_t pin_ = 0,
               DisplayDriver driver = DisplayDriver::NEOPIXEL) {
        if (ledCount_ != 0) ledCount = ledCount_;
        if (pin_ != 0) pin = pin_;

        // Create the appropriate driver based on the enum selection
        if (driver == DisplayDriver::NEOPIXEL) {
            engine =
                std::make_unique<AnimationEngine>(std::make_unique<NeoPixelDriver>(ledCount, pin));
        } else {
            engine =
                std::make_unique<AnimationEngine>(std::make_unique<FastLEDDriver>(ledCount, pin));
        }

        lastUpdate = millis();
    }

    // Update loop (call frequently in Arduino loop)
    void update() {
        unsigned long now = millis();
        if (now - lastUpdate < frameInterval) return;  // skip if frame interval not reached

        float deltaTime = (now - lastUpdate) / 1000.0f;  // convert ms to seconds
        lastUpdate = now;

        engine->update(deltaTime);
    }

    // Blocking update that waits until any active transition completes
    void finishTransition() {
        lastUpdate = millis();
        while (engine && engine->isFading()) {
            update();
            delay(1);
        }
    }

    // ---------------- User-friendly methods ----------------

    void solid(uint32_t color) {
        engine->addAnimation(std::make_unique<SolidColor>(ledCount, color));
    }

    void rainbow(float speed = 1.0f) {
        engine->addAnimation(std::make_unique<Rainbow>(ledCount, speed));
    }

    void breathe(uint32_t color, float minBrightness = 0.5f, float speed = 1.0f) {
        engine->addAnimation(std::make_unique<Breathe>(ledCount, color, minBrightness));
    }

    void wave(uint32_t color, float wavelength = 10.0f) {
        engine->addAnimation(std::make_unique<Wave>(ledCount, wavelength, 1.0f, color));
    }

    void spinner(size_t trailLength = 5, uint32_t color = 0x0096FF, float speed = 1.0f) {
        auto anim = std::make_unique<Spinner>(ledCount, trailLength, color);
        anim->speed = speed;
        engine->addAnimation(std::move(anim));
    }

    void blink(uint32_t color, uint8_t times = 1) {
        for (uint8_t i = 0; i < times; ++i) {
            engine->addAnimation(std::make_unique<SolidColor>(ledCount, color));
            engine->addAnimation(std::make_unique<SolidColor>(ledCount, Color::Black));
        }
    }

    void blank() { engine->addAnimation(std::make_unique<SolidColor>(ledCount, Color::Black)); }

    // Optional: push custom animation directly
    void addAnimation(std::unique_ptr<Animation> anim) { engine->addAnimation(std::move(anim)); }

    // Optional: allow changing FPS at runtime
    void setFPS(float newFps) {
        fps = newFps;
        frameInterval = 1000.0f / fps;
    }
};
