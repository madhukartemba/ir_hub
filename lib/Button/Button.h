#include <Arduino.h>
#include <functional>
#include "Haptics.h"
#include "Log.h"
#include "Speaker.h"

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
    unsigned long lastInteractionTime = 0;  // Track last button interaction

    Speaker* speaker;
    bool soundEnabled = true;

    Haptics* haptics = nullptr;
    bool hapticsEnabled = true;

   public:
    Button() {}
    ~Button() {}

    bool begin(int pin, uint8_t mode) {
        this->pin = pin;
        pinMode(pin, mode);
        initialized = true;
        return true;
    }

    void setSpeaker(Speaker& speakerRef) { this->speaker = &speakerRef; }

    void setHaptics(Haptics& hapticsRef) { this->haptics = &hapticsRef; }

    void setSoundEnabled(bool enabled) { soundEnabled = enabled; }

    void setHapticsEnabled(bool enabled) { hapticsEnabled = enabled; }

    void setClickCallback(std::function<void()> callback) { singleClickCallback = callback; }

    void setLongPressCallback(std::function<void()> callback) { longPressCallback = callback; }

    void setDebounceTime(unsigned long debounceMs) { debounceTime = debounceMs; }

    void setLongPressTime(unsigned long longPressMs) { longPressTime = longPressMs; }

    // Getter for last interaction time
    unsigned long getLastInteractionTime() const { return lastInteractionTime; }

    void update() {
        if (!initialized) {
            LOG_ERROR("[Button] Button not initialized");
            return;
        }

        currentState = digitalRead(pin);
        unsigned long currentTime = millis();

        // Button press detected (transition from LOW to HIGH)
        if (currentState == HIGH && lastState == LOW) {
            pressStartTime = currentTime;
            buttonPressed = true;
            longPressTriggered = false;
            lastInteractionTime = currentTime;  // Update interaction time on press
            // Simulated mechanical click: down-stroke
            if (hapticsEnabled && haptics != nullptr && haptics->isReady()) {
                haptics->playButtonPress();
            }
        }

        // Button is currently pressed
        if (currentState == HIGH && buttonPressed) {
            // Check if long press time has been reached
            if (!longPressTriggered && (currentTime - pressStartTime) >= longPressTime) {
                longPressTriggered = true;
                lastInteractionTime = currentTime;  // Update interaction time on long press
                if (hapticsEnabled && haptics != nullptr && haptics->isReady()) {
                    haptics->playLongPressAck();
                }
                if (soundEnabled && speaker != nullptr) {
                    speaker->doubleBeep();
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
                lastInteractionTime = currentTime;  // Update interaction time on release
                // Simulated mechanical click: up-stroke (paired with playButtonPress on down)
                if (hapticsEnabled && haptics != nullptr && haptics->isReady()) {
                    haptics->playButtonRelease();
                }
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