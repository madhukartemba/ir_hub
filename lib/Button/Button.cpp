#include "Button.h"

Button::Button()
    : oneButton(nullptr),
      speaker(nullptr),
      buttonPin(0),
      soundEnabled(true) {
    // Set default sound settings
    clickSound = {1000, 25};        // 1kHz, 25ms
    doubleClickSound = {1000, 25};  // 1kHz, 25ms
    longPressSound = {1000, 25};    // 1kHz, 25ms

    // Initialize user callbacks to empty
    userClickCallback = nullptr;
    userDoubleClickCallback = nullptr;
    userLongPressCallback = nullptr;
    userLongPressStartCallback = nullptr;
    userLongPressStopCallback = nullptr;
}

Button::~Button() {
    if (oneButton) {
        delete oneButton;
        oneButton = nullptr;
    }
}

void Button::begin(uint8_t buttonPin, Speaker& speakerRef) {
    this->buttonPin = buttonPin;
    this->speaker = &speakerRef;
    
    // Initialize OneButton
    oneButton = new OneButton(buttonPin, false, false);

    if (oneButton) {
        // Initialize OneButton
        oneButton->setClickMs(50);        // Default is 200ms for single click

        // Set up internal callbacks using static methods
        oneButton->attachClick(staticClickCallback, this);
        oneButton->attachDoubleClick(staticDoubleClickCallback, this);
        oneButton->attachLongPressStart(staticLongPressStartCallback, this);
        oneButton->attachLongPressStop(staticLongPressStopCallback, this);
        oneButton->attachDuringLongPress(staticDuringLongPressCallback, this);
    }
}

void Button::setClickCallback(std::function<void()> callback) {
    userClickCallback = callback;
}

void Button::setDoubleClickCallback(std::function<void()> callback) {
    userDoubleClickCallback = callback;
}

void Button::setLongPressCallback(std::function<void()> callback) {
    userLongPressCallback = callback;
}

void Button::setLongPressStartCallback(std::function<void()> callback) {
    userLongPressStartCallback = callback;
}

void Button::setLongPressStopCallback(std::function<void()> callback) {
    userLongPressStopCallback = callback;
}

void Button::setSoundEnabled(bool enabled) {
    soundEnabled = enabled;
}

void Button::setClickSound(unsigned int frequency, unsigned long duration) {
    clickSound.frequency = frequency;
    clickSound.duration = duration;
}

void Button::setDoubleClickSound(unsigned int frequency, unsigned long duration) {
    doubleClickSound.frequency = frequency;
    doubleClickSound.duration = duration;
}

void Button::setLongPressSound(unsigned int frequency, unsigned long duration) {
    longPressSound.frequency = frequency;
    longPressSound.duration = duration;
}

void Button::update() {
    if (oneButton) {
        oneButton->tick();
    }
}

OneButton* Button::getOneButton() {
    return oneButton;
}

void Button::onButtonClick() {
    if (soundEnabled) {
        playSound(clickSound);
    }
    if (userClickCallback) {
        userClickCallback();
    }
}

void Button::onButtonDoubleClick() {
    if (soundEnabled) {
        playSound(doubleClickSound);
    }
    if (userDoubleClickCallback) {
        userDoubleClickCallback();
    }
}

void Button::onButtonLongPress() {
    if (soundEnabled) {
        playSound(longPressSound);
    }
    if (userLongPressCallback) {
        userLongPressCallback();
    }
}

void Button::onButtonLongPressStart() {
    if (userLongPressStartCallback) {
        userLongPressStartCallback();
    }
}

void Button::onButtonLongPressStop() {
    if (userLongPressStopCallback) {
        userLongPressStopCallback();
    }
}

void Button::playSound(const SoundSettings& sound) {
    if (speaker) {
        speaker->beep(sound.frequency, sound.duration);
    }
}

// Static callback implementations
void Button::staticClickCallback(void* context) {
    Button* button = static_cast<Button*>(context);
    button->onButtonClick();
}

void Button::staticDoubleClickCallback(void* context) {
    Button* button = static_cast<Button*>(context);
    button->onButtonDoubleClick();
}

void Button::staticLongPressStartCallback(void* context) {
    Button* button = static_cast<Button*>(context);
    button->onButtonLongPressStart();
}

void Button::staticLongPressStopCallback(void* context) {
    Button* button = static_cast<Button*>(context);
    button->onButtonLongPressStop();
}

void Button::staticDuringLongPressCallback(void* context) {
    Button* button = static_cast<Button*>(context);
    button->onButtonLongPress();
}
