#include <Arduino.h>
#include "../global/Global.h"
#include "../ui/devices/Devices.h"
#include "../ui/irlearn/IRLearn.h"
#include "../ui/settings/Settings.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, IR_LEARN, SETTINGS };

    State currentState;

   public:
    void onEnter() override {
        LOG_DEBUG("MainMenu onEnter");
        currentState = State::DEVICES;

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("MainMenu onButtonClick");
            // Switch to next state using mod operator
            currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 3);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("MainMenu onButtonLongPress");
            if (currentState == State::DEVICES) {
                router.push(new Devices());
            } else if (currentState == State::IR_LEARN) {
                router.push(new IRLearn());
            } else if (currentState == State::SETTINGS) {
                router.push(new Settings());
            }
        });
    }

    void onUpdate() override {
        // Update display based on current state
        display.clear();

        // Show title
        display.setTextSize(1);
        display.printCentered("IR Hub - Main Menu", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Devices", "IR Learn", "Settings"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == static_cast<int>(currentState));
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("MainMenu onExit"); }
};