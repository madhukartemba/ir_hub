#ifndef LEDRING_H
#define LEDRING_H

#include <FastLED.h>

enum LedRingMode {
    OFF,
    LOADING,
    BREATHE,
    PROGRESS,
    WAVE,
    PULSE,
    FLASH,
    RAINBOW
};

class LedRing {
public:
    LedRing(uint8_t pin, uint8_t numLeds);

    void begin();
    void setBrightness(uint8_t brightness);
    void setMode(LedRingMode mode);
    void setProgress(float progress);
    void setColor(CRGB color); // for applicable modes
    void update();

private:
    CRGB* leds;
    uint8_t pin, numLeds;
    uint8_t brightness;
    LedRingMode mode = OFF;
    CRGB color = CRGB::White;

    // state trackers
    unsigned long lastUpdate = 0;
    uint8_t index = 0;

    // breathe/pulse
    uint8_t breatheStep = 0;
    bool breatheUp = true;

    // flash
    bool flashOn = false;
    unsigned long flashStart = 0;

    // rainbow
    uint8_t rainbowHue = 0;

    // progress
    float progress = 0.0f;

    void clear();
    void show();

    void updateLoading();
    void updateBreathe();
    void updateProgress();
    void updateWave();
    void updatePulse();
    void updateFlash();
    void updateRainbow();
};

#endif
