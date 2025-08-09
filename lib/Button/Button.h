#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include <OneButton.h>
#include <functional>
#include "../Speaker/Speaker.h"

class Button {
public:
    // Empty constructor
    Button();
    ~Button();

    // Initialize the button with pin and speaker
    void begin(uint8_t buttonPin, Speaker& speakerRef);

    // Button event callbacks
    void setClickCallback(std::function<void()> callback);
    void setDoubleClickCallback(std::function<void()> callback);
    void setLongPressCallback(std::function<void()> callback);
    void setLongPressStartCallback(std::function<void()> callback);
    void setLongPressStopCallback(std::function<void()> callback);

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
    Speaker* speaker;
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
    std::function<void()> userClickCallback;
    std::function<void()> userDoubleClickCallback;
    std::function<void()> userLongPressCallback;
    std::function<void()> userLongPressStartCallback;
    std::function<void()> userLongPressStopCallback;

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
