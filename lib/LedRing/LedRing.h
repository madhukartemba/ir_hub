#include <Arduino.h>
#include <FastLED.h>
#include "Log.h"

class LedRing {
   public:
    enum State { OFF, WAVE, BREATHE, PROGRESS, RAINBOW };

    LedRing() {}
    ~LedRing() {}

    void begin(int pin, int numLeds, int centerLed = 0) {
        this->numLeds = numLeds;
        this->dataPin = pin;
        leds = new CRGB[numLeds];
        tempPrev = new CRGB[numLeds];
        tempTarget = new CRGB[numLeds];
        currentState = OFF;
        targetState = OFF;
        previousState = OFF;
        brightness = 255;
        speed = 5;            // Default to medium speed (1-10 range)
        color = CRGB::White;  // default
        transitionProgress = 1.0f;
        lastUpdateTime = millis();
        this->centerLed = centerLed;
        waveWidth = 255;        // Default wave width (full width)
        progressValue = -1.0f;  // -1 means use animated progress, 0.0-1.0 means static progress
        switch (pin) {
            case D7:
                FastLED.addLeds<WS2812B, D7, GRB>(leds, numLeds);
            default:
                LOG_ERROR("Led Pin Isn't Defined");
        }
        FastLED.setBrightness(brightness);
        FastLED.clear(true);
    }

    void setState(State newState) {
        if (newState != targetState) {
            previousState = currentState;
            targetState = newState;
            transitionProgress = 0.0f;
            lastUpdateTime = millis();
        }
    }

    void nextState() {
        State ns = static_cast<State>((targetState + 1) % 5);
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

    void resetTransition() {
        transitionProgress = 0.0f;
        lastUpdateTime = millis();
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
                  uint16_t center = 0, float progress = -1.0f) {
        setSpeed(progressSpeed);
        setColor(progressColor);
        setCenterLed(center);
        setProgressValue(progress);
        resetTransition();
        setState(PROGRESS);
    }

    void rainbow(uint8_t rainbowSpeed = 5) {
        setSpeed(rainbowSpeed);
        resetTransition();
        setState(RAINBOW);
    }

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

        fillState(previousState, tempPrev, now);
        fillState(targetState, tempTarget, now);

        for (uint16_t i = 0; i < numLeds; i++) {
            leds[i] = blend(tempPrev[i], tempTarget[i], uint8_t(transitionProgress * 255));
        }

        FastLED.show();
    }

   private:
    uint16_t numLeds;
    uint8_t dataPin;
    CRGB* leds;
    CRGB* tempPrev;
    CRGB* tempTarget;

    State currentState, targetState, previousState;
    uint8_t brightness;
    uint8_t speed;
    float transitionProgress;
    unsigned long lastUpdateTime;
    CRGB color;
    uint16_t centerLed;
    uint8_t waveWidth;
    float progressValue;  // -1 means use animated progress, 0.0-1.0 means static progress

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
        }
    }
};
