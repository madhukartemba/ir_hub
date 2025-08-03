#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>

class Speaker {
private:
    uint8_t pin;
    bool isInitialized;

public:
    // Constructor
    Speaker(uint8_t speakerPin) : pin(speakerPin), isInitialized(false) {}
    
    // Initialize the speaker pin
    void begin() {
        pinMode(pin, OUTPUT);
        isInitialized = true;
    }
    
    // Basic beep with default duration (100ms)
    void beep() {
        beep(100);
    }
    
    // Beep with custom duration
    void beep(unsigned long duration) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, 1000, duration); // 1kHz tone
    }
    
    // Beep with custom frequency and duration
    void beep(unsigned int frequency, unsigned long duration) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, frequency, duration);
    }
    
    // Short beep (50ms)
    void shortBeep() {
        beep(50);
    }
    
    // Long beep (500ms)
    void longBeep() {
        beep(500);
    }
    
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
    void stop() {
        noTone(pin);
    }
    
    // Play a continuous tone (until stopped)
    void playTone(unsigned int frequency) {
        if (!isInitialized) {
            begin();
        }
        tone(pin, frequency);
    }
    
    // Check if speaker is initialized
    bool initialized() const {
        return isInitialized;
    }
    
    // Get the pin number
    uint8_t getPin() const {
        return pin;
    }
};

#endif // SPEAKER_H 