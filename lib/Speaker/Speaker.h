#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>
#include "../Log/Log.h"

// // Disable tone functions
// #define tone(...) ((void)0)
// #define noTone(...) ((void)0)

class Speaker {
   private:
    uint8_t pin;
    bool isInitialized;

   public:
    // Constructor
    Speaker(uint8_t speakerPin) : pin(speakerPin), isInitialized(false) {}

    Speaker() : pin(-1), isInitialized(false) {}

    bool begin() {
        if (pin == -1) {
            LOG_ERROR("Speaker pin not set");
            return false;
        }
        return begin(pin);
    }

    // Initialize the speaker pin
    bool begin(uint8_t speakerPin) {
        pin = speakerPin;
        pinMode(pin, OUTPUT);
        isInitialized = true;
        stop();  // Ensure no sound is playing on startup
        return true;
    }

    // Basic beep with default duration (100ms)
    void beep() { beep(100); }

    // Beep with custom duration
    void beep(unsigned long duration) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, 1000, duration);  // 1kHz tone
    }

    // Beep with custom frequency and duration
    void beep(unsigned int frequency, unsigned long duration) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, frequency, duration);
    }

    // Short beep (50ms)
    void shortBeep() { beep(50); }

    // Long beep (500ms)
    void longBeep() { beep(500); }

    // Double beep
    void doubleBeep() {
        beep(100);
        delay(100);
        beep(100);
    }

    // Triple beep
    void tripleBeep() {
        beep(100);
        delay(100);
        beep(100);
        delay(100);
        beep(100);
    }

    // Stop any ongoing tone
    void stop() { noTone(pin); }

    // Play a continuous tone (until stopped)
    void playTone(unsigned int frequency) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, frequency);
    }

    // Check if speaker is initialized
    bool initialized() const { return isInitialized; }

    // Get the pin number
    uint8_t getPin() const { return pin; }
};

#endif  // SPEAKER_H