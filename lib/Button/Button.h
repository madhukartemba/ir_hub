#include <Arduino.h>
#include <functional>
#include "Speaker.h"
#include "Log.h"

class Button {
    private:
        int pin;
        boolean initialized = false;
        std::function<void()> singleClickCallback;
        std::function<void()> longPressCallback;
        bool lastState = LOW;
        bool currentState = LOW;
        unsigned long pressStartTime = 0;
        unsigned long debounceTime = 50;
        unsigned long longPressTime = 500;
        bool longPressTriggered = false;
        bool buttonPressed = false;
        
        Speaker* speaker;
        bool soundEnabled = true;

    public:
        Button() {}
        ~Button() {}

        void begin(int pin, uint8_t mode) {
            this->pin = pin;
            pinMode(pin, mode);
            initialized = true;
        }

        void setSpeaker(Speaker& speakerRef) {
            this->speaker = &speakerRef;
        }

        void setSoundEnabled(bool enabled) {
            soundEnabled = enabled;
        }

        void setClickCallback(std::function<void()> callback) {
            singleClickCallback = callback;
        }

        void setLongPressCallback(std::function<void()> callback) {
            longPressCallback = callback;
        }

        void setDebounceTime(unsigned long debounceMs) {
            debounceTime = debounceMs;
        }

        void setLongPressTime(unsigned long longPressMs) {
            longPressTime = longPressMs;
        }

        void update() {
            if (!initialized) {
                LOG_ERROR("Button not initialized");
                return;
            }

            currentState = digitalRead(pin);
            unsigned long currentTime = millis();
            
            // Button press detected (transition from LOW to HIGH)
            if (currentState == HIGH && lastState == LOW) {
                pressStartTime = currentTime;
                buttonPressed = true;
                longPressTriggered = false;
            }
            
            // Button is currently pressed
            if (currentState == HIGH && buttonPressed) {
                // Check if long press time has been reached
                if (!longPressTriggered && (currentTime - pressStartTime) >= longPressTime) {
                    longPressTriggered = true;
                    if (soundEnabled && speaker != nullptr) {
                        speaker->longBeep();
                    }
                    if (longPressCallback) {
                        longPressCallback();
                    }
                }
            }
            
            // Button release detected (transition from HIGH to LOW)
            if (currentState == LOW && lastState == HIGH && buttonPressed) {
                unsigned long pressDuration = currentTime - pressStartTime;
                
                // Only trigger single click if debounce time has passed and it wasn't a long press
                if (pressDuration >= debounceTime && !longPressTriggered) {
                    if (soundEnabled && speaker != nullptr) {
                        speaker->shortBeep();
                    }
                    if (singleClickCallback) {
                        singleClickCallback();
                    }
                }
                
                buttonPressed = false;
                longPressTriggered = false;
            }
            
            lastState = currentState;
        }
};