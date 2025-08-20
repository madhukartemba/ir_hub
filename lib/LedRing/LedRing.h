#include <Arduino.h>
#include <FastLED.h>
#include "Log.h"

class LedRing {
   public:
    enum State { OFF, WAVE, BREATHE, PROGRESS, RAINBOW, PULSE, SOLID };

    LedRing() {}
    ~LedRing() {}

    bool begin(int pin, int numLeds, int centerLed = 0) {
        this->numLeds = numLeds;
        this->dataPin = pin;
        leds = new CRGB[numLeds];
        tempPrev = new CRGB[numLeds];
        tempTarget = new CRGB[numLeds];
        transitionSnapshot = new CRGB[numLeds];  // New snapshot buffer
        currentState = OFF;
        targetState = OFF;
        previousState = OFF;
        brightness = 255;
        speed = 5;            // Default to medium speed (1-10 range)
        color = CRGB::White;  // default
        transitionProgress = 1.0f;
        lastUpdateTime = millis();
        transitionStartTime = millis();  // Initialize transition start time
        this->centerLed = centerLed;
        waveWidth = 255;        // Default wave width (full width)
        progressValue = -1.0f;  // -1 means use animated progress, 0.0-1.0 means static progress
        pulseCount = 3;         // Default pulse count
        pulsesCompleted = 0;    // Track completed pulses
        pulseStartTime = 0;     // Track when current pulse started
        switch (pin) {
            case D7:
                FastLED.addLeds<WS2812B, D7, GRB>(leds, numLeds);
                break;
            default:
                LOG_ERROR("Led Pin Isn't Defined");
                return false;
        }
        FastLED.setBrightness(brightness);
        FastLED.clear(true);
        return true;
    }

    void setState(State newState) {
        if (newState != targetState) {
            previousState = currentState;
            targetState = newState;
            transitionProgress = 0.0f;
            transitionStartTime = millis();  // Capture the exact moment transition starts
            lastUpdateTime = millis();

            // Reset pulse counters when entering PULSE mode
            if (newState == PULSE) {
                pulsesCompleted = 0;
                pulseStartTime = millis();
            }

            // Capture current LED state as snapshot for smooth blending
            for (uint16_t i = 0; i < numLeds; i++) {
                transitionSnapshot[i] = leds[i];
            }
        }
    }

    void nextState() {
        State ns = static_cast<State>((targetState + 1) % 7);  // Updated to 7 states
        setState(ns);
    }

    void setBrightness(uint8_t b) {
        brightness = b;
        FastLED.setBrightness(brightness);
    }

    void setSpeed(uint8_t s) { speed = s; }

    void setColor(const CRGB& c) { color = c; }

    void setCenterLed(uint16_t center) { centerLed = center; }

    void setWaveWidth(uint8_t width) { waveWidth = width; }

    void setProgressValue(float progress) { progressValue = constrain(progress, 0.0f, 1.0f); }

    void setPulseCount(uint8_t count) {
        pulseCount = (count > 0) ? count : 1;  // Ensure at least 1 pulse
    }

    void resetTransition() {
        transitionProgress = 0.0f;
        transitionStartTime = millis();
        lastUpdateTime = millis();

        // Reset pulse counters
        if (targetState == PULSE) {
            pulsesCompleted = 0;
            pulseStartTime = millis();
        }

        // Capture current state
        for (uint16_t i = 0; i < numLeds; i++) {
            transitionSnapshot[i] = leds[i];
        }
    }

    // Convenience methods to set states with parameters
    void wave(uint8_t waveSpeed = 5, const CRGB& waveColor = CRGB::White, uint8_t width = 255) {
        setSpeed(waveSpeed);
        setColor(waveColor);
        setWaveWidth(width);
        resetTransition();
        setState(WAVE);
    }

    void breathe(uint8_t breatheSpeed = 5, const CRGB& breatheColor = CRGB::White) {
        setSpeed(breatheSpeed);
        setColor(breatheColor);
        resetTransition();
        setState(BREATHE);
    }

    void progress(uint8_t progressSpeed = 5, const CRGB& progressColor = CRGB::White,
                  float progress = -1.0f) {
        setSpeed(progressSpeed);
        setColor(progressColor);
        setProgressValue(progress);
        resetTransition();
        setState(PROGRESS);
    }

    void rainbow(uint8_t rainbowSpeed = 5) {
        setSpeed(rainbowSpeed);
        resetTransition();
        setState(RAINBOW);
    }

    void pulse(uint8_t pulseSpeed = 5, const CRGB& pulseColor = CRGB::White, uint8_t count = 3) {
        setSpeed(pulseSpeed);
        setColor(pulseColor);
        setPulseCount(count);
        resetTransition();
        setState(PULSE);
    }

    void solid(const CRGB& solidColor = CRGB::White, uint8_t solidBrightness = 255) {
        setColor(solidColor);
        setBrightness(solidBrightness);
        resetTransition();
        setState(SOLID);
    }

    float getTransitionProgress() { return transitionProgress; }

    void off() {
        resetTransition();
        setState(OFF);
    }

    void update() {
        unsigned long now = millis();
        float delta = (now - lastUpdateTime) / 500.0f;  // Slower transition speed
        lastUpdateTime = now;

        if (transitionProgress < 1.0f) {
            transitionProgress += delta;
            if (transitionProgress >= 1.0f) {
                transitionProgress = 1.0f;
                currentState = targetState;
            }
        }

        if (transitionProgress < 1.0f) {
            // During transition, blend from snapshot to target state
            fillState(targetState, tempTarget, now);

            for (uint16_t i = 0; i < numLeds; i++) {
                leds[i] =
                    blend(transitionSnapshot[i], tempTarget[i], uint8_t(transitionProgress * 255));
            }
        } else {
            // Transition complete, just show target state
            fillState(currentState, leds, now);
        }

        FastLED.show();
    }

    void blockingUpdate() {
        while (transitionProgress < 1.0f) {
            update();
        }
    }

   private:
    uint16_t numLeds;
    uint8_t dataPin;
    CRGB* leds;
    CRGB* tempPrev;
    CRGB* tempTarget;
    CRGB* transitionSnapshot;  // New buffer to store state at transition start

    State currentState, targetState, previousState;
    uint8_t brightness;
    uint8_t speed;
    float transitionProgress;
    unsigned long lastUpdateTime;
    unsigned long transitionStartTime;  // Track when transition started
    CRGB color;
    uint16_t centerLed;
    uint8_t waveWidth;
    float progressValue;           // -1 means use animated progress, 0.0-1.0 means static progress
    uint8_t pulseCount;            // Number of pulses to perform
    uint8_t pulsesCompleted;       // Number of pulses completed
    unsigned long pulseStartTime;  // When current pulse sequence started

    void fillState(State s, CRGB* buffer, unsigned long t) {
        switch (s) {
            case OFF:
                fill_solid(buffer, numLeds, CRGB::Black);
                break;

            case WAVE: {
                // Wave that starts from center and travels around the ring
                for (uint16_t i = 0; i < numLeds; i++) {
                    // Calculate position relative to center, going clockwise around the ring
                    int16_t relativePos = i - centerLed;
                    if (relativePos < 0) {
                        relativePos += numLeds;  // Wrap around for negative positions
                    }
                    // Create wave that starts from center and travels clockwise
                    uint8_t pos =
                        (relativePos * waveWidth / numLeds + t / ((11 - speed) * 10)) % waveWidth;
                    uint8_t b = sin8(pos * 255 / waveWidth);
                    buffer[i] = color;
                    buffer[i].fadeLightBy(255 - b);
                }
                break;
            }

            case BREATHE: {
                uint8_t b = beatsin8(1000 / ((11 - speed) * 10), 50, 255);
                fill_solid(buffer, numLeds, color);
                for (uint16_t i = 0; i < numLeds; i++) {
                    buffer[i].fadeLightBy(255 - b);
                }
                break;
            }

            case PROGRESS: {
                uint8_t progress;
                if (progressValue >= 0.0f) {
                    // Use static progress value
                    progress = progressValue * 100;
                } else {
                    // Use animated progress
                    progress = (t / ((11 - speed) * 10)) % 100;
                }
                uint8_t filled = map(progress, 0, 100, 0, numLeds);
                for (uint16_t i = 0; i < numLeds; i++) {
                    // Calculate position relative to center, going clockwise around the ring
                    int16_t relativePos = i - centerLed;
                    if (relativePos < 0) {
                        relativePos += numLeds;  // Wrap around for negative positions
                    }
                    buffer[i] = (relativePos < filled) ? color : CRGB::Black;
                }
                break;
            }

            case RAINBOW: {
                fill_rainbow(buffer, numLeds, (t / ((11 - speed) * 10)) % 255, 255 / numLeds);
                break;
            }

            case PULSE: {
                // Calculate pulse timing based on speed (slower speed = longer pulse period)
                unsigned long pulsePeriod = (11 - speed) * 200;  // 200-2000ms per pulse
                unsigned long timeSinceStart = t - pulseStartTime;
                uint8_t currentPulseNumber = timeSinceStart / pulsePeriod;

                if (currentPulseNumber < pulseCount) {
                    // Still pulsing
                    unsigned long currentPulseTime = timeSinceStart % pulsePeriod;
                    float pulsePhase = (float)currentPulseTime / pulsePeriod;

                    // *** FIX: Use a quadratic wave for a smooth 0-255-0 pulse. ***
                    // This ensures the pulse starts and ends at zero brightness, allowing
                    // for a seamless transition to OFF after the last pulse completes.
                    uint8_t b = quadwave8((uint8_t)(pulsePhase * 255));

                    fill_solid(buffer, numLeds, color);
                    for (uint16_t i = 0; i < numLeds; i++) {
                        buffer[i].fadeLightBy(255 - b);
                    }
                } else {
                    // All pulses completed. The last pulse has faded to black.
                    // Now, formally switch to the OFF state.
                    setState(OFF);
                }
                break;
            }

            case SOLID: {
                // Solid color mode - all LEDs show the same color at specified brightness
                fill_solid(buffer, numLeds, color);
                break;
            }
        }
    }
};
