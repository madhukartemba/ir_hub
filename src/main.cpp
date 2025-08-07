#include <Arduino.h>
#include "LedRing.h"
#include "Speaker.h"
#include "config.h"


LedRing ring(NEOPIXEL_PIN, NUM_LEDS);
Speaker speaker(SPEAKER_PIN);

// Use all available modes
const LedRingMode modes[] = {
    OFF, LOADING, BREATHE, PROGRESS, WAVE, PULSE, FLASH, RAINBOW
};
const int modeCount = sizeof(modes) / sizeof(modes[0]);
int currentModeIndex = 0;

unsigned long lastButtonTime = 0;
bool lastButtonState = HIGH;

void setup() {
    Serial.begin(9600);

    pinMode(TOUCH_BUTTON_PIN, INPUT); // assumes active LOW button

    speaker.begin();

    ring.begin();
    ring.setBrightness(255);
    ring.setColor(CRGB::Green); // Default color for all but rainbow
    ring.setMode(modes[currentModeIndex]);

    Serial.println("LedRing Test: Touch D0 to cycle animations");
}

void loop() {
    ring.update();

    // Button debounce
    bool currentButtonState = digitalRead(TOUCH_BUTTON_PIN);
    if (lastButtonState == LOW && currentButtonState == HIGH) {
        unsigned long now = millis();
        if (now - lastButtonTime > 250) { // debounce delay
            // Next mode
            currentModeIndex = (currentModeIndex + 1) % modeCount;
            LedRingMode mode = modes[currentModeIndex];

            // Set color for all except rainbow
            if (mode != RAINBOW) {
                ring.setColor(CRGB::Green);
            }
            // Set progress for PROGRESS mode (demo: 50%)
            if (mode == PROGRESS) {
                ring.setProgress(0.5f);
            }

            ring.setMode(mode);
            Serial.print("Switched to mode: ");
            Serial.println(currentModeIndex);

            lastButtonTime = now;
        }
    }
    lastButtonState = currentButtonState;
}
