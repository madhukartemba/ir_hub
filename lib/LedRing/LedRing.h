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
    LedRing() : pin(-1), numLeds(0) {}
    ~LedRing();

    void begin();
    void begin(uint8_t pin, uint8_t numLeds);
    void setBrightness(uint8_t brightness);
    void setMode(LedRingMode mode);
    void setProgress(float progress);
    void setColor(CRGB color); // for applicable modes
    void setSpeed(uint8_t speed); // 0-255, higher = faster
    void setCenterLed(uint8_t centerIndex); // Set the center/starting LED index
    void update();

private:
    CRGB* leds;
    uint8_t pin, numLeds;
    uint8_t brightness;
    uint8_t speed = 128; // Animation speed (0-255, higher = faster)
    uint8_t centerLed = 0; // Center/starting LED index for animations
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
    unsigned long getUpdateInterval();

    void updateLoading();
    void updateBreathe();
    void updateProgress();
    void updateWave();
    void updatePulse();
    void updateFlash();
    void updateRainbow();
};

#endif
