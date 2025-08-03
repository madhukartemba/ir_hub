#ifndef CIRCULAR_NEOPIXEL_H
#define CIRCULAR_NEOPIXEL_H

#include <Arduino.h>
#include <FastLED.h>

class CircularNeoPixel {
private:
    CRGB* leds;
    uint8_t numLeds;
    uint8_t pin;
    bool initialized;
    
    // Animation variables
    unsigned long lastUpdate;
    uint8_t animationStep;
    bool animationRunning;
    uint8_t currentAnimation; // Track which animation is running
    CRGB animationColor;      // Store the current animation color
    uint8_t animationSpeed;   // Store the current animation speed
    
public:
    // Constructor
    CircularNeoPixel(uint8_t pin, uint8_t numLeds);
    
    // Destructor
    ~CircularNeoPixel();
    
    // Initialization
    void init();
    void begin();
    
    // Basic control functions
    void clear();
    void show();
    void setBrightness(uint8_t brightness);
    void setPixelColor(uint8_t pixel, CRGB color);
    void setPixelColor(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b);
    void setAllPixels(CRGB color);
    void setAllPixels(uint8_t r, uint8_t g, uint8_t b);
    
    // Startup animation
    void startUp();
    void startUp(uint8_t r, uint8_t g, uint8_t b);
    void startUp(CRGB color);
    
    // Circular effects
    void rotate(CRGB color, uint8_t speed = 50);
    void pulse(CRGB color, uint8_t speed = 50);
    void wave(CRGB color, uint8_t speed = 50);
    void rainbow(uint8_t speed = 50);
    
    // Timed effects (run for specified duration)
    void rotateFor(CRGB color, uint8_t speed, unsigned long duration);
    void pulseFor(CRGB color, uint8_t speed, unsigned long duration);
    void waveFor(CRGB color, uint8_t speed, unsigned long duration);
    void rainbowFor(uint8_t speed, unsigned long duration);
    
    // Utility functions
    uint8_t getNumLeds();
    bool isInitialized();
    void stopAnimation();
    
    // Animation control
    void update();
    bool isAnimationRunning();
    
    // Private variables for timed animations
private:
    unsigned long animationEndTime;
    bool timedAnimation;
};

#endif // CIRCULAR_NEOPIXEL_H 