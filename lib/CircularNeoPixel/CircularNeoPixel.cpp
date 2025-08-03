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
    
    // Schedule next update
    lastUpdate = millis() + speed;
}

// Update animations (call this in loop())
void CircularNeoPixel::update() {
    if (!initialized || !animationRunning) return;
    
    if (millis() >= lastUpdate) {
        animationStep++;
        
        // Handle different animation types
        if (animationRunning) {
            // For now, just rotate through all animations
            // You can expand this based on the last called animation
            clear();
            setPixelColor(animationStep % numLeds, CRGB::Blue);
            show();
            
            lastUpdate = millis() + 100; // Default speed
        }
    }
}

// Stop any running animation
void CircularNeoPixel::stopAnimation() {
    animationRunning = false;
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