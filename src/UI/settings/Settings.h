#include <ESP.h>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"
#include "ClearDataConfirmation.h"
#include "UserPrefs.h"

class Settings : public Screen {
   private:
    enum class State {
        SOUND,
        RESTART,
        CLEAR_DATA,
        WIFI_WIPE,
        BACK,
        RESTARTING,
    };
    State currentState;
    unsigned long restartStartTime;
    int selectedIndex;

    // menuItems[0] (the sound row) is rebuilt on every render from the
    // current preference, so the label flips between "Sound: On" and
    // "Sound: Off" the moment the user toggles it.
    const char* menuItems[5] = {"Sound: On", "Restart", "Clear Data", "Wipe Wi-Fi", "Back"};

   public:
    void onEnter() override {
        LOG_DEBUG("Settings onEnter");
        currentState = State::SOUND;
        selectedIndex = 0;

        ledRing.breathe(COLOR_SETTINGS);

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            if (currentState == State::RESTARTING) {
                return;
            }
            selectedIndex = (selectedIndex + 1) % 5;
            currentState = static_cast<State>(selectedIndex);
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");

            switch (currentState) {
                case State::SOUND:
                    toggleSound();
                    break;
                case State::RESTART:
                    LOG_DEBUG("Settings onButtonLongPress RESTART");
                    currentState = State::RESTARTING;
                    restartStartTime = millis();
                    break;
                case State::CLEAR_DATA:
                    LOG_DEBUG("Settings onButtonLongPress CLEAR_DATA");
                    router.push(new ClearDataConfirmation());
                    break;
                case State::WIFI_WIPE:
                    LOG_DEBUG("Settings onButtonLongPress WIFI_WIPE");
                    wifiManager.resetWiFi();
                    speaker.successBeep();
                    currentState = State::RESTARTING;
                    restartStartTime = millis();
                    break;
                case State::BACK:
                    LOG_DEBUG("Settings onButtonLongPress BACK");
                    router.pop();
                    break;
                case State::RESTARTING:
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        if (currentState == State::RESTARTING) {
            drawRestarting();
            if (millis() - restartStartTime > 2000) {
                ESP.restart();
            }
        } else {
            menuItems[0] = userPrefsSoundEnabled() ? "Sound: On" : "Sound: Off";
            drawMenu();
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("Settings onExit");
    }

    void drawMenu() {
        display.setTextSize(1);
        display.printCentered("Settings", 0);
        display.drawLine(0, 12, display.getWidth(), 12);
        MenuUtils::drawScrollableMenu(menuItems, 5, selectedIndex, 3, 20);
    }

    void drawRestarting() {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Restarting...", 20);
        display.setTextSize(1);
        display.printCentered("Please wait", 35);
    }

   private:
    void toggleSound() {
        bool newState = !userPrefsSoundEnabled();
        userPrefsSetSoundEnabled(newState);
        speaker.setMuted(!newState);
        // Give immediate audible confirmation only when turning the sound
        // ON — otherwise we'd contradict the user's "off" choice.
        if (newState) {
            speaker.shortBeep();
        }
        LOG_INFO("[Settings] Sound %s", newState ? "ON" : "OFF");
    }
};
