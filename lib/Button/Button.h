#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include <OneButton.h>
#include "../Speaker/Speaker.h"

class Button {
public:
    // Constructor - accepts Speaker object reference
    Button(uint8_t buttonPin, Speaker& speakerRef);
    ~Button();

    // Initialize the button
    void begin();

    // Button event callbacks
    void setClickCallback(void (*callback)());
    void setDoubleClickCallback(void (*callback)());
    void setLongPressCallback(void (*callback)());
    void setLongPressStartCallback(void (*callback)());
    void setLongPressStopCallback(void (*callback)());

    // Sound settings
    void setSoundEnabled(bool enabled);
    void setClickSound(unsigned int frequency, unsigned long duration);
    void setDoubleClickSound(unsigned int frequency, unsigned long duration);
    void setLongPressSound(unsigned int frequency, unsigned long duration);

    // Update method (call in loop)
    void update();

    // Get OneButton instance for advanced usage
    OneButton* getOneButton();

private:
    OneButton* oneButton;
    Speaker& speaker;
    uint8_t buttonPin;
    bool soundEnabled;

    // Sound settings
    struct SoundSettings {
        unsigned int frequency;
        unsigned long duration;
    };

    SoundSettings clickSound;
    SoundSettings doubleClickSound;
    SoundSettings longPressSound;

    // Internal callback methods
    void onButtonClick();
    void onButtonDoubleClick();
    void onButtonLongPress();
    void onButtonLongPressStart();
    void onButtonLongPressStop();

    // User callbacks
    void (*userClickCallback)();
    void (*userDoubleClickCallback)();
    void (*userLongPressCallback)();
    void (*userLongPressStartCallback)();
    void (*userLongPressStopCallback)();

    // Helper method to play sound
    void playSound(const SoundSettings& sound);
    
    // Static callback methods for OneButton
    static void staticClickCallback(void* context);
    static void staticDoubleClickCallback(void* context);
    static void staticLongPressStartCallback(void* context);
    static void staticLongPressStopCallback(void* context);
    static void staticDuringLongPressCallback(void* context);
};

#endif // BUTTON_H
