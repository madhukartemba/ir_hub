#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../ui/help/HelpQrScreen.h"
#include "../preferences.h"
#include "../ui/adddevice/AddDevice.h"
#include "../ui/devices/Devices.h"
#include "../ui/settings/ContactQrScreen.h"
#include "../ui/settings/Settings.h"
#include "../utils/MenuUtils.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, ADD_DEVICE, SETTINGS, HELP, CONTACT };

    State currentState;
    int selectedIndex;
    const char* menuItems[5] = {"Devices", "Add Device", "Settings", "Help", "Contact"};

   public:
    void onEnter() override {
        LOG_DEBUG("[MainMenu] onEnter");
        currentState = State::DEVICES;
        selectedIndex = 0;
        ledRing.breathe(COLOR_INFO_DARK);

        // Change button behavior
        button.setClickCallback([this]() {
            // Switch to next state using mod operator (now only 3 states)
            LOG_DEBUG("[MainMenu] onButtonClick");
            selectedIndex = (selectedIndex + 1) % 5;
            currentState = static_cast<State>(selectedIndex);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("[MainMenu] onButtonLongPress");
            if (currentState == State::DEVICES) {
                router.push(new Devices());
            } else if (currentState == State::ADD_DEVICE) {
                router.push(new AddDevice());
            } else if (currentState == State::SETTINGS) {
                router.push(new Settings());
            } else if (currentState == State::HELP) {
                router.push(new HelpQrScreen());
            } else if (currentState == State::CONTACT) {
                router.push(new ContactQrScreen());
            }
        });
    }

    void onUpdate() override {
        // Update display based on current state
        display.clear();
        showMenuScreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[MainMenu] onExit");
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
        MenuUtils::drawScrollableMenu(menuItems, 5, selectedIndex, 3, 20);
    }
};