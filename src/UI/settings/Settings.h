#include <ESP.h>
#include "../../global/Global.h"
#include "ClearDataConfirmation.h"

class Settings : public Screen {
   private:
    enum class State {
        CLEAR_DATA,
        BACK,
    };
    State currentState;

   public:
    void onEnter() override {
        LOG_DEBUG("Settings onEnter");
        currentState = State::CLEAR_DATA;

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            switch (currentState) {
                case State::CLEAR_DATA:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::CLEAR_DATA;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");
            switch (currentState) {
                case State::CLEAR_DATA:
                    LOG_DEBUG("Settings onButtonLongPress CLEAR_DATA");
                    router.push(new ClearDataConfirmation());
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
            case State::CLEAR_DATA:
                drawClearData();
                break;
            case State::BACK:
                drawBack();
                break;
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("Settings onExit"); }

    void drawClearData() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Clear Data", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 0);  // Clear Data is selected
            display.drawMenuItem(menuItems[i], i, 2, isSelected, startY);
        }
    }

    void drawIRTest() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Clear Data", "IR Test", "Back"};
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
        const char* menuItems[] = {"Clear Data", "IR Test", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 2);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }
};