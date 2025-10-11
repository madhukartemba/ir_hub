#pragma once
#include <Adafruit_NeoPixel.h>
#include <memory>
#include <queue>
#include <vector>
#include "Animation.h"
#include "LedDriver.h"

class AnimationEngine {
   private:
    std::unique_ptr<LEDDriver> driver;

    std::unique_ptr<Animation> currentAnimation;
    std::unique_ptr<Animation> nextAnimation;

    std::queue<std::unique_ptr<Animation>> animationQueue;

   public:
    AnimationEngine(std::unique_ptr<LEDDriver> driver_)
        : driver(std::move(driver_)),  // take ownership of the driver
          currentAnimation(nullptr),   // start with no animation
          nextAnimation(nullptr)       // start with no next animation
    {
        if (driver) {
            driver->begin();
        }
    }
};