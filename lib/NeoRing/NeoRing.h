#pragma once
#include <Arduino.h>  // for millis()
#include <cstdint>
#include <memory>
#include "AnimationEngine.h"
#include "Color.h"
#include "animations/Breathe.h"
#include "animations/Rainbow.h"
#include "animations/SolidColor.h"
#include "animations/Wave.h"
#include "drivers/NeopixelDriver.h"

class NeoRing {
   private:
    size_t ledCount;

    unsigned long lastUpdate = 0;  // last update timestamp in ms
    float fps = 60.0f;             // default frames per second
    float frameInterval;           // ms per frame, initialized in constructor

    AnimationEngine engine;

   public:
    // Constructor: takes number of LEDs and pin
    NeoRing(size_t ledCount_, uint8_t pin)
        : ledCount(ledCount_),
          frameInterval(1000.0f / fps),
          engine(std::make_unique<NeoPixelDriver>(ledCount_, pin)) {}

    // Initialize hardware
    void begin() { lastUpdate = millis(); }

    // Update loop (call frequently in Arduino loop)
    void update() {
        unsigned long now = millis();
        if (now - lastUpdate < frameInterval) return;  // skip if frame interval not reached

        float deltaTime = (now - lastUpdate) / 1000.0f;  // convert ms to seconds
        lastUpdate = now;

        engine.update(deltaTime);
    }

    // ---------------- User-friendly methods ----------------

    void solid(uint32_t color) {
        engine.addAnimation(std::make_unique<SolidColor>(ledCount, color));
    }

    void rainbow(float speed = 1.0f) {
        engine.addAnimation(std::make_unique<Rainbow>(ledCount, speed));
    }

    void breathe(uint32_t color, float speed = 1.0f) {
        engine.addAnimation(std::make_unique<Breathe>(ledCount, color));
    }

    void wave(float speed = 1.0f) { engine.addAnimation(std::make_unique<Wave>(ledCount, speed)); }

    void blank() { engine.addAnimation(std::make_unique<SolidColor>(ledCount, Color::Black)); }

    // Optional: push custom animation directly
    void addAnimation(std::unique_ptr<Animation> anim) { engine.addAnimation(std::move(anim)); }

    // Optional: allow changing FPS at runtime
    void setFPS(float newFps) {
        fps = newFps;
        frameInterval = 1000.0f / fps;
    }
};
