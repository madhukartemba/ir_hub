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
    bool muted = false;

   public:
    Speaker(uint8_t speakerPin) : pin(speakerPin), isInitialized(false) {}

    Speaker() : pin(-1), isInitialized(false) {}

    bool begin() {
        if (pin == -1) {
            LOG_ERROR("[Speaker] Speaker pin not set");
            return false;
        }
        return begin(pin);
    }

    bool begin(uint8_t speakerPin) {
        pin = speakerPin;
        pinMode(pin, OUTPUT);
        isInitialized = true;
        stop();  // Ensure no sound is playing on startup
        return true;
    }

    void setMuted(bool m) {
        muted = m;
        if (muted) {
            stop();
        }
    }
    bool isMuted() const { return muted; }

    // Basic beep with default duration (100ms)
    void beep() { beep(100); }

    // Beep with custom duration
    void beep(unsigned long duration) {
        if (muted) return;
        if (!isInitialized) {
            begin();
        }
        tone(pin, 1000, duration);  // 1kHz tone
    }

    // Beep with custom frequency and duration
    void beep(unsigned int frequency, unsigned long duration) {
        if (muted) return;
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

    void tripleBeep() {
        beep(100);
        delay(100);
        beep(100);
        delay(100);
        beep(100);
    }

    // Success sound - ascending double beep
    void successBeep() {
        beep(800, 100);   // Lower frequency
        delay(50);        // Short pause
        beep(1200, 150);  // Higher frequency, longer duration
    }

    // Error sound - descending double beep
    void errorBeep() {
        beep(1200, 100);  // Higher frequency
        delay(50);        // Short pause
        beep(600, 200);   // Lower frequency, longer duration
    }

    // Stop any ongoing tone
    void stop() { noTone(pin); }

    // Play a continuous tone (until stopped)
    void playTone(unsigned int frequency) {
        if (muted) return;
        if (!isInitialized) {
            begin();
        }
        tone(pin, frequency);
    }
    // Play a startup melody
    void playStartupSound() {
        if (muted) return;
        if (!isInitialized) {
            begin();
        }
        int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};         // C4 to C5 notes
        int noteDurations[] = {200, 200, 200, 200, 200, 200, 200, 200};  // Duration for each note

        for (size_t i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
            beep(melody[i], noteDurations[i]);
            delay(100);  // Short pause between notes
        }
    }

    // Check if speaker is initialized
    bool initialized() const { return isInitialized; }

    // Get the pin number
    uint8_t getPin() const { return pin; }
};

#endif  // SPEAKER_H