#include "../../global/Global.h"
#include "IRTest.h"

class Settings : public Screen {
   private:
    enum class State {
        RESET_SETTINGS,
        IR_TEST,
        BACK,
    };
    State currentState;

   public:
    void onEnter() override {
        LOG_DEBUG("Settings onEnter");
        currentState = State::RESET_SETTINGS;

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            switch (currentState) {
                case State::RESET_SETTINGS:
                    currentState = State::IR_TEST;
                    break;
                case State::IR_TEST:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::RESET_SETTINGS;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");
            switch (currentState) {
                case State::RESET_SETTINGS:
                    LOG_DEBUG("Settings onButtonLongPress RESET_SETTINGS");
                    break;
                case State::IR_TEST:
                    LOG_DEBUG("Entering IR Test");
                    router.push(new IRTest());
                    break;
                case State::BACK:
                    LOG_DEBUG("Settings onButtonLongPress BACK");
                    router.pop();
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        switch (currentState) {
            case State::RESET_SETTINGS:
                drawResetSettings();
                break;
            case State::IR_TEST:
                drawIRTest();
                break;
            case State::BACK:
                drawBack();
                break;
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("Settings onExit"); }

    void drawResetSettings() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Reset Settings", "IR Test", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 0);  // Reset Settings is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }

    void drawIRTest() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Reset Settings", "IR Test", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 1);  // IR Test is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }

    void drawBack() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Reset Settings", "IR Test", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 2);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }
};