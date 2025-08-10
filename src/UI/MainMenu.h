#include <Arduino.h>
#include "../global/Global.h"
#include "../ui/irlearn/IRLearn.h"
#include "../ui/settings/Settings.h"
#include "../ui/test/TestMenu.h"

class MainMenu : public Screen {
   private:
    enum class State {
        STATUS,
        IR_LEARN,
        SETTINGS,
        TEST,
    };

    State currentState;

   public:
    void onEnter() override {
        LOG_DEBUG("MainMenu onEnter");
        currentState = State::STATUS;

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("MainMenu onButtonClick");
            // Switch to next state using mod operator
            currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 4);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("MainMenu onButtonLongPress");
            if (currentState == State::STATUS) {
                // Do nothing
            } else if (currentState == State::IR_LEARN) {
                router.push(new IRLearn());
            } else if (currentState == State::SETTINGS) {
                router.push(new Settings());
            } else if (currentState == State::TEST) {
                router.push(new TestMenu());
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
        const char* menuItems[] = {"Status", "IR Learn", "Settings", "Test"};
        int startY = 20;

        for (int i = 0; i < 4; i++) {
            bool isSelected = (i == static_cast<int>(currentState));
            display.drawMenuItem(menuItems[i], i, 4, isSelected, startY);
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("MainMenu onExit"); }
};