#include "CircularNeoPixel.h"

// Constructor
CircularNeoPixel::CircularNeoPixel(uint8_t pin, uint8_t numLeds) {
    this->pin = pin;
    this->numLeds = numLeds;
    this->leds = new CRGB[numLeds];
    this->initialized = false;
    this->animationRunning = false;
    this->animationStep = 0;
    this->lastUpdate = 0;
    this->currentAnimation = 0;
    this->animationColor = CRGB::Black;
    this->animationSpeed = 50;
    this->animationEndTime = 0;
    this->timedAnimation = false;
}

// Destructor
CircularNeoPixel::~CircularNeoPixel() {
    if (leds != nullptr) {
        delete[] leds;
    }
}

// Initialize the LED strip
void CircularNeoPixel::init() {
    FastLED.addLeds<WS2812B, D7, GRB>(leds, numLeds);
    FastLED.setBrightness(128); // Default brightness
    initialized = true;
    clear();
    show();
}

// Alias for init()
void CircularNeoPixel::begin() {
    init();
}

// Clear all LEDs
void CircularNeoPixel::clear() {
    if (!initialized) return;
    fill_solid(leds, numLeds, CRGB::Black);
    show();
}

// Show the current LED state
void CircularNeoPixel::show() {
    if (!initialized) return;
    FastLED.show();
}

// Set brightness
void CircularNeoPixel::setBrightness(uint8_t brightness) {
    if (!initialized) return;
    FastLED.setBrightness(brightness);
}

// Set individual pixel color using CRGB
void CircularNeoPixel::setPixelColor(uint8_t pixel, CRGB color) {
    if (!initialized || pixel >= numLeds) return;
    leds[pixel] = color;
}

// Set individual pixel color using RGB values
void CircularNeoPixel::setPixelColor(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized || pixel >= numLeds) return;
    leds[pixel] = CRGB(r, g, b);
}

// Set all pixels to the same color using CRGB
void CircularNeoPixel::setAllPixels(CRGB color) {
    if (!initialized) return;
    fill_solid(leds, numLeds, color);
    show();
}

// Set all pixels to the same color using RGB values
void CircularNeoPixel::setAllPixels(uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized) return;
    fill_solid(leds, numLeds, CRGB(r, g, b));
    show();
}

// Startup animation - default white
void CircularNeoPixel::startUp() {
    startUp(CRGB::White);
}

// Startup animation with RGB values
void CircularNeoPixel::startUp(uint8_t r, uint8_t g, uint8_t b) {
    startUp(CRGB(r, g, b));
}

// Startup animation with CRGB color
void CircularNeoPixel::startUp(CRGB color) {
    if (!initialized) return;
    
    // Clear first
    clear();
    
    // Animate from center outward (assuming circular arrangement)
    for (int i = 0; i < numLeds; i++) {
        setPixelColor(i, color);
        show();
        delay(50); // 50ms delay between each LED
    }
    
    // Brief pause
    delay(200);
    
    // Fade out
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
        FastLED.setBrightness(brightness);
        show();
        delay(10);
    }
    
    // Reset brightness and clear
    FastLED.setBrightness(128);
    clear();
}

// Rotating light effect
void CircularNeoPixel::rotate(CRGB color, uint8_t speed) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 1; // Rotate animation
    animationColor = color;
    animationSpeed = speed;
    
    // Clear all LEDs
    clear();
    
    // Set the current position
    setPixelColor(animationStep, color);
    show();
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Pulsing effect
void CircularNeoPixel::pulse(CRGB color, uint8_t speed) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 2; // Pulse animation
    animationColor = color;
    animationSpeed = speed;
    
    // Set all pixels to the color
    setAllPixels(color);
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Wave effect
void CircularNeoPixel::wave(CRGB color, uint8_t speed) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 3; // Wave animation
    animationColor = color;
    animationSpeed = speed;
    
    // Clear all LEDs
    clear();
    
    // Create wave pattern
    for (int i = 0; i < numLeds; i++) {
        uint8_t brightness = sin8(i * 32 + animationStep * 8);
        setPixelColor(i, color);
        leds[i].nscale8(brightness);
    }
    show();
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Rainbow effect
void CircularNeoPixel::rainbow(uint8_t speed) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 4; // Rainbow animation
    animationSpeed = speed;
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Update animations (call this in loop())
void CircularNeoPixel::update() {
    if (!initialized || !animationRunning) return;
    
    // Check if timed animation has expired
    if (timedAnimation && millis() >= animationEndTime) {
        stopAnimation();
        return;
    }
    
    if (millis() >= lastUpdate) {
        animationStep++;
        
        // Handle different animation types based on currentAnimation
        switch (currentAnimation) {
            case 1: // Rotate
                clear();
                setPixelColor(animationStep % numLeds, animationColor);
                show();
                break;
                
            case 2: // Pulse
                FastLED.setBrightness(sin8(animationStep * 8));
                show();
                break;
                
            case 3: { // Wave
                clear();
                for (int i = 0; i < numLeds; i++) {
                    uint8_t brightness = sin8(i * 32 + animationStep * 8);
                    setPixelColor(i, animationColor);
                    leds[i].nscale8(brightness);
                }
                show();
                break;
            }
                
            case 4: // Rainbow
                fill_rainbow(leds, numLeds, animationStep * 2, 255 / numLeds);
                show();
                break;
        }
        
        lastUpdate = millis() + animationSpeed;
    }
}

// Stop any running animation
void CircularNeoPixel::stopAnimation() {
    animationRunning = false;
    currentAnimation = 0;
    timedAnimation = false;
    FastLED.setBrightness(128); // Reset brightness
    clear();
}

// Get number of LEDs
uint8_t CircularNeoPixel::getNumLeds() {
    return numLeds;
}

// Check if initialized
bool CircularNeoPixel::isInitialized() {
    return initialized;
}

// Check if animation is running
bool CircularNeoPixel::isAnimationRunning() {
    return animationRunning;
} 

// Timed rotate effect
void CircularNeoPixel::rotateFor(CRGB color, uint8_t speed, unsigned long duration) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 1;
    animationColor = color;
    animationSpeed = speed;
    animationEndTime = millis() + duration;
    timedAnimation = true;
    
    // Clear all LEDs
    clear();
    
    // Set the current position
    setPixelColor(animationStep, color);
    show();
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Timed pulse effect
void CircularNeoPixel::pulseFor(CRGB color, uint8_t speed, unsigned long duration) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 2;
    animationColor = color;
    animationSpeed = speed;
    animationEndTime = millis() + duration;
    timedAnimation = true;
    
    // Set all pixels to the color
    setAllPixels(color);
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Timed wave effect
void CircularNeoPixel::waveFor(CRGB color, uint8_t speed, unsigned long duration) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 3;
    animationColor = color;
    animationSpeed = speed;
    animationEndTime = millis() + duration;
    timedAnimation = true;
    
    // Clear all LEDs
    clear();
    
    // Create wave pattern
    for (int i = 0; i < numLeds; i++) {
        uint8_t brightness = sin8(i * 32 + animationStep * 8);
        setPixelColor(i, color);
        leds[i].nscale8(brightness);
    }
    show();
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Timed rainbow effect
void CircularNeoPixel::rainbowFor(uint8_t speed, unsigned long duration) {
    if (!initialized) return;
    
    animationRunning = true;
    animationStep = 0;
    currentAnimation = 4;
    animationSpeed = speed;
    animationEndTime = millis() + duration;
    timedAnimation = true;
    
    // Schedule next update
    lastUpdate = millis() + speed;
} 