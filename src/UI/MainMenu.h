#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../ui/adddevice/AddDevice.h"
#include "../ui/devices/Devices.h"
#include "../ui/settings/Settings.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, ADD_DEVICE, SETTINGS };

    State currentState;
    unsigned long lastActivityTime;
    const unsigned long INACTIVITY_TIMEOUT = 30000;  // 30 seconds in milliseconds

   public:
    void onEnter() override {
        LOG_DEBUG("MainMenu onEnter");
        currentState = State::DEVICES;
        lastActivityTime = millis();

        // Change button behavior
        button.setClickCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // Switch to next state using mod operator (now only 3 states)
            LOG_DEBUG("MainMenu onButtonClick");
            currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 3);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            LOG_DEBUG("MainMenu onButtonLongPress");
            if (currentState == State::DEVICES) {
                router.push(new Devices());
            } else if (currentState == State::ADD_DEVICE) {
                router.push(new AddDevice());
            } else if (currentState == State::SETTINGS) {
                router.push(new Settings());
            }
        });
    }

    void onUpdate() override {
        // Check for inactivity timeout - return to status screen (default screen)
        if ((millis() - lastActivityTime) > INACTIVITY_TIMEOUT) {
            LOG_DEBUG("Returning to status screen due to inactivity");
            router.pop();  // This will return to the default screen (StatusScreen)
            return;
        }

        // Update display based on current state
        display.clear();
        showMenuScreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("MainMenu onExit");
        // Make sure display is turned on when exiting
        display.turnOn();
    }

   private:
    void showMenuScreen() {
        // Show title
        display.setTextSize(1);
        display.printCentered("IR Hub - Main Menu", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Devices", "Add Device", "Settings"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == static_cast<int>(currentState));
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }
};