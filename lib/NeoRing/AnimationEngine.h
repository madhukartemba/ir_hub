#pragma once
#include <Adafruit_NeoPixel.h>
#include <memory>
#include <queue>
#include <vector>
#include "Animation.h"
#include "Combiner.h"
#include "Fader.h"
#include "LedDriver.h"

class AnimationEngine {
   private:
    std::unique_ptr<LEDDriver> driver;

    std::unique_ptr<Animation> currentAnimation;
    std::unique_ptr<Animation> nextAnimation;

    std::queue<std::unique_ptr<Animation>> animationQueue;

    Fader fader;        // Handles fade between current and next
    Combiner combiner;  // Handles blending frames

    bool currentAnimationUpdated = false;  // Track if static animation has been updated
    bool nextAnimationUpdated = false;     // Track if static animation has been updated

   public:
    AnimationEngine(std::unique_ptr<LEDDriver> driver_)
        : driver(std::move(driver_)), currentAnimation(nullptr), nextAnimation(nullptr) {
        if (driver) {
            driver->begin();
        }
    }

    // Push a new animation to play
    void addAnimation(std::unique_ptr<Animation> anim) {
        if (!anim) return;  // ignore null

        if (!nextAnimation) {
            nextAnimation = std::move(anim);
            nextAnimationUpdated = false;  // Reset flag for new animation
            fader.start(0.0f);             // start fade from 0 → 1
        } else {
            animationQueue.push(std::move(anim));
        }
    }

    // Called every frame with deltaTime in seconds
    void update(float deltaTime) {
        bool frameChanged = false;

        // Only update if animation is not static or hasn't been updated yet
        if (currentAnimation && (!currentAnimation->isStatic() || !currentAnimationUpdated)) {
            currentAnimation->update();
            currentAnimationUpdated = true;
            frameChanged = true;
        }
        if (nextAnimation && (!nextAnimation->isStatic() || !nextAnimationUpdated)) {
            nextAnimation->update();
            nextAnimationUpdated = true;
            frameChanged = true;
        }

        std::vector<uint32_t> frame;

        if (nextAnimation) {
            // Fading is always dynamic, so we need to update
            frameChanged = true;
            fader.update(deltaTime);
            frame =
                combiner.blend(currentAnimation ? currentAnimation->getFrame()
                                                : std::vector<uint32_t>(driver->getLedCount(), 0),
                               nextAnimation->getFrame(), fader.getValue());

            // Fade complete
            if (fader.isComplete()) {
                currentAnimation = std::move(nextAnimation);
                currentAnimationUpdated = nextAnimationUpdated;  // Transfer the update state
                nextAnimation.reset();
                nextAnimationUpdated = false;

                // Start next animation if queued
                if (!animationQueue.empty()) {
                    nextAnimation = std::move(animationQueue.front());
                    animationQueue.pop();
                    nextAnimationUpdated = false;  // Reset flag for new animation
                    fader.start(0.0f);
                }
            }
        } else if (currentAnimation) {
            frame = currentAnimation->getFrame();
        }

        // Only send to driver if frame changed
        if (frameChanged && driver) driver->show(frame);
    }

    bool isFading() const { return (nextAnimation != nullptr) || !fader.isComplete(); }
};
