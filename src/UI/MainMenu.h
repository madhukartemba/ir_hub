#include <Arduino.h>
#include <ESP.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../ui/help/HelpQrScreen.h"
#include "UserPrefs.h"
#include "../preferences.h"
#include "../ui/adddevice/AddDevice.h"
#include "../ui/devices/Devices.h"
#include "../ui/settings/ContactQrScreen.h"
#include "../ui/settings/Settings.h"
#include "../ui/settings/AuthorEasterEggScreen.h"
#include "../utils/MenuUtils.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, ADD_DEVICE, SETTINGS, HELP, CONTACT, CONNECT_WIFI, AUTHOR };

    static constexpr int kMaxMenuItems = 7;
    State currentState = State::DEVICES;
    int selectedIndex = 0;
    int menuCount = 0;
    const char* menuItems[kMaxMenuItems];
    State menuStates[kMaxMenuItems];

   public:
    void onEnter() override {
        LOG_DEBUG("[MainMenu] onEnter");
        buildMenu();
        selectedIndex = 0;
        currentState = menuStates[0];
        ledRing.breathe(COLOR_INFO_DARK);

        button.setClickCallback([this]() {
            // Switch to next state using mod operator.
            LOG_DEBUG("[MainMenu] onButtonClick");
            selectedIndex = (selectedIndex + 1) % menuCount;
            currentState = menuStates[selectedIndex];
        });

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
            } else if (currentState == State::CONNECT_WIFI) {
                userPrefsSetSkipWiFiSetup(false);
                display.clear();
                display.printCentered("Connect to Wi-Fi", 20);
                display.printCentered("Restarting...", 38);
                display.update();
                delay(700);
                ESP.restart();
            } else if (currentState == State::AUTHOR) {
                router.push(new AuthorEasterEggScreen());
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
        display.turnOn();
    }

   private:
    void showMenuScreen() {
        // Show title
        display.setTextSize(1);
        display.printCentered("Main Menu", 0);

        display.drawLine(0, 12, display.getWidth(), 12);

        // Use the scrollable menu utility
        MenuUtils::drawScrollableMenu(menuItems, menuCount, selectedIndex, 3, 20);
    }

    void addMenuItem(State state, const char* label) {
        if (menuCount >= kMaxMenuItems) {
            return;
        }
        menuStates[menuCount] = state;
        menuItems[menuCount] = label;
        menuCount++;
    }

    void buildMenu() {
        menuCount = 0;
        addMenuItem(State::DEVICES, "Devices");
        addMenuItem(State::ADD_DEVICE, "Add Device");
        if (userPrefsSkipWiFiSetup()) {
            addMenuItem(State::CONNECT_WIFI, "Connect to Wi-Fi");
        }
        addMenuItem(State::SETTINGS, "Settings");
        addMenuItem(State::HELP, "Help");
        addMenuItem(State::CONTACT, "Contact");
        addMenuItem(State::AUTHOR, "By Madhukar Temba :)");
    }
};