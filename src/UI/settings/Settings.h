#include "../../global/Global.h"

class Settings : public Screen {
   private:
    enum class State {
        RESET_SETTINGS,
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
        const char* menuItems[] = {"Reset Settings", "Back"};
        int startY = 20;

        for (int i = 0; i < 2; i++) {
            bool isSelected = (i == 0);  // Reset Settings is selected
            display.drawMenuItem(menuItems[i], i, 2, isSelected, startY);
        }

        // Show instructions at bottom
        display.setTextSize(1);
        display.print("Click: Navigate", 2, 50);
        display.print("Hold: Select", 2, 58);
    }

    void drawBack() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Reset Settings", "Back"};
        int startY = 20;

        for (int i = 0; i < 2; i++) {
            bool isSelected = (i == 1);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 2, isSelected, startY);
        }

        // Show instructions at bottom
        display.setTextSize(1);
        display.print("Click: Navigate", 2, 50);
        display.print("Hold: Select", 2, 58);
    }
};