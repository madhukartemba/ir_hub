#include <Arduino.h>
#include "config.h"
#include "Global/Global.h"

// Use all available modes
const LedRingMode modes[] = {
    OFF, LOADING, BREATHE, PROGRESS, WAVE, PULSE, FLASH, RAINBOW
};
const int modeCount = sizeof(modes) / sizeof(modes[0]);
int currentModeIndex = 0;

// Function to handle button clicks
void onButtonClick() {
    // Next mode
    currentModeIndex = (currentModeIndex + 1) % modeCount;
    LedRingMode currentMode = modes[currentModeIndex];

    // Set random color for all except rainbow
    if (currentMode != RAINBOW) {
        CRGB randomColor = CRGB(random(0, 256), random(0, 256), random(0, 256));
        ring.setColor(randomColor);
    }
    // Set progress for PROGRESS mode (demo: 50%)
    if (currentMode == PROGRESS) {
        ring.setProgress(0.5f);
    }

    ring.setMode(currentMode);
    Serial.print("Switched to mode: ");
    Serial.println(currentModeIndex);
}

void setup() {
    Serial.begin(9600);

    speaker.begin();
    button.begin(TOUCH_BUTTON_PIN, speaker);
    
    // Set up button callbacks
    button.setClickCallback(onButtonClick);

    ring.begin(NEOPIXEL_PIN, NUM_LEDS);
    ring.setCenterLed(CENTER_LED);
    ring.setBrightness(255);
    ring.setColor(CRGB::Green); // Default color for all but rainbow
    ring.setMode(modes[currentModeIndex]);

    // Initialize the global router
    // You can set a default screen here if needed
    // router.setDefaultScreen(&someDefaultScreen);

    Serial.println("LedRing Test: Touch D0 to cycle animations");
}

void loop() {
    ring.update();
    button.update(); // Update button state
    
    // Update the global router
    router.update();
}
