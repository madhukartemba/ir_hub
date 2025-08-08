#include "LedRing.h"
#include <math.h>

LedRing::LedRing(uint8_t pin, uint8_t numLeds)
    : pin(pin), numLeds(numLeds), brightness(128), speed(128) {
    leds = new CRGB[numLeds];
}

LedRing::~LedRing() {
    if (leds) {
        delete[] leds;
        leds = nullptr;
    }
}

void LedRing::begin() {
    FastLED.addLeds<WS2812B, D7, GRB>(leds, numLeds);
    FastLED.setBrightness(brightness);
    clear();
    show();
}

void LedRing::setBrightness(uint8_t b) {
    brightness = b;
    FastLED.setBrightness(brightness);
}

void LedRing::setSpeed(uint8_t s) {
    speed = s;
}

void LedRing::setMode(LedRingMode m) {
    mode = m;
    index = 0;
    breatheStep = 0;
    flashOn = false;
    lastUpdate = 0;
    clear();
    show();
}

void LedRing::setColor(CRGB c) {
    color = c;
}

void LedRing::setProgress(float p) {
    progress = constrain(p, 0.0f, 1.0f);
}

void LedRing::clear() {
    fill_solid(leds, numLeds, CRGB::Black);
}

void LedRing::show() {
    FastLED.show();
}

unsigned long LedRing::getUpdateInterval() {
    // Convert speed (0-255) to interval (1-100ms)
    // Higher speed = lower interval = faster animation
    if (speed == 0) return 1000; // Very slow
    return map(speed, 1, 255, 100, 1);
}

void LedRing::update() {
    unsigned long now = millis();
    unsigned long interval = getUpdateInterval();

    switch (mode) {
        case LOADING:
            if (now - lastUpdate > interval) {
                updateLoading();
                lastUpdate = now;
            }
            break;
        case BREATHE:
            if (now - lastUpdate > interval) {
                updateBreathe();
                lastUpdate = now;
            }
            break;
        case PULSE:
            if (now - lastUpdate > interval) {
                updatePulse();
                lastUpdate = now;
            }
            break;
        case PROGRESS:
            updateProgress();
            break;
        case WAVE:
            if (now - lastUpdate > interval) {
                updateWave();
                lastUpdate = now;
            }
            break;
        case FLASH:
            updateFlash();
            break;
        case RAINBOW:
            if (now - lastUpdate > interval) {
                updateRainbow();
                lastUpdate = now;
            }
            break;
        case OFF:
        default:
            break;
    }
}

// ================= Animations =================

void LedRing::updateLoading() {
    clear();
    leds[index] = color;
    index = (index + 1) % numLeds;
    show();
}

void LedRing::updateProgress() {
    clear();
    uint8_t lit = progress * numLeds;
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i] = (i < lit) ? color : CRGB::Black;
    }
    show();
}

void LedRing::updateBreathe() {
    float scale = sin(breatheStep * PI / 255.0);
    uint8_t b = uint8_t(scale * brightness);

    CRGB breatheColor = color;
    breatheColor.nscale8(b);
    fill_solid(leds, numLeds, breatheColor);
    show();

    uint8_t stepSize = map(speed, 1, 255, 1, 8);
    breatheStep += breatheUp ? stepSize : -stepSize;
    if (breatheStep >= 255) breatheUp = false;
    else if (breatheStep <= 0) breatheUp = true;
}

void LedRing::updatePulse() {
    float scale = sin(breatheStep * PI / 255.0);
    uint8_t b = uint8_t(scale * brightness);

    CRGB pulseColor = color;
    pulseColor.nscale8(b);
    fill_solid(leds, numLeds, pulseColor);
    show();

    uint8_t stepSize = map(speed, 1, 255, 1, 8);
    breatheStep += breatheUp ? stepSize : -stepSize;
    if (breatheStep >= 255) breatheUp = false;
    else if (breatheStep <= 0) breatheUp = true;
}

void LedRing::updateWave() {
    for (uint8_t i = 0; i < numLeds; i++) {
        float angle = (float)(index + i) / numLeds * 2 * PI;
        float intensity = (sin(angle) + 1.0f) / 2.0f;
        leds[i] = color;
        leds[i].fadeToBlackBy(255 - (int)(intensity * 255));
    }
    index = (index + 1) % numLeds;
    show();
}

void LedRing::updateFlash() {
    if (!flashOn) {
        fill_solid(leds, numLeds, color);
        show();
        flashOn = true;
        flashStart = millis();
    } else {
        if (millis() - flashStart > 500) {
            clear();
            show();
            mode = OFF;
        }
    }
}

void LedRing::updateRainbow() {
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i] = CHSV(rainbowHue + i * 255 / numLeds, 255, 255);
        leds[i].nscale8(brightness);
    }
    uint8_t hueStep = map(speed, 1, 255, 1, 4);
    rainbowHue += hueStep;
    show();
}
