#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../utils/MenuUtils.h"
#include "../ui/adddevice/AddDevice.h"
#include "../ui/devices/Devices.h"
#include "../ui/settings/Settings.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, ADD_DEVICE, SETTINGS };

    State currentState;
    int selectedIndex;
    unsigned long lastActivityTime;
    const unsigned long INACTIVITY_TIMEOUT = 30000;  // 30 seconds in milliseconds
    const char* menuItems[3] = {"Devices", "Add Device", "Settings"};

   public:
    void onEnter() override {
        LOG_DEBUG("MainMenu onEnter");
        currentState = State::DEVICES;
        selectedIndex = 0;
        lastActivityTime = millis();

        // Change button behavior
        button.setClickCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // Switch to next state using mod operator (now only 3 states)
            LOG_DEBUG("MainMenu onButtonClick");
            selectedIndex = (selectedIndex + 1) % 3;
            currentState = static_cast<State>(selectedIndex);
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
        display.printCentered("Main Menu", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Use the scrollable menu utility
        MenuUtils::drawScrollableMenu(menuItems, 3, selectedIndex, 3, 20);
    }
};