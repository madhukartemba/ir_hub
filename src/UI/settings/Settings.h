#include <ESP.h>
#include "../../global/Global.h"
#include "ClearDataConfirmation.h"

class Settings : public Screen {
   private:
    enum class State {
        RESTART,
        CLEAR_DATA,
        BACK,
        RESTARTING,
    };
    State currentState;
    unsigned long restartStartTime;

   public:
    void onEnter() override {
        LOG_DEBUG("Settings onEnter");
        currentState = State::RESTART;

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            switch (currentState) {
                case State::RESTART:
                    currentState = State::CLEAR_DATA;
                    break;
                case State::CLEAR_DATA:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::RESTART;
                    break;
                case State::RESTARTING:
                    // Do nothing during restart
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");
            switch (currentState) {
                case State::RESTART:
                    LOG_DEBUG("Settings onButtonLongPress RESTART");
                    currentState = State::RESTARTING;
                    restartStartTime = millis();
                    break;
                case State::CLEAR_DATA:
                    LOG_DEBUG("Settings onButtonLongPress CLEAR_DATA");
                    router.push(new ClearDataConfirmation());
                    break;
                case State::BACK:
                    LOG_DEBUG("Settings onButtonLongPress BACK");
                    router.pop();
                    break;
                case State::RESTARTING:
                    // Do nothing during restart
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        switch (currentState) {
            case State::RESTART:
                drawRestart();
                break;
            case State::CLEAR_DATA:
                drawClearData();
                break;
            case State::BACK:
                drawBack();
                break;
            case State::RESTARTING:
                drawRestarting();
                // Restart after 2 seconds
                if (millis() - restartStartTime > 2000) {
                    ESP.restart();
                }
                break;
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("Settings onExit"); }

    void drawRestart() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Restart", "Clear Data", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 0);  // Restart is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }

    void drawClearData() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Restart", "Clear Data", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 1);  // Clear Data is selected
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
        const char* menuItems[] = {"Restart", "Clear Data", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 2);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }

    void drawRestarting() {
        // Clear display and show restarting message
        display.clear();

        // Draw title
        display.setTextSize(1);
        display.printCentered("Restarting...", 20);

        // Show a brief message before restart
        display.setTextSize(1);
        display.printCentered("Please wait", 35);
    }
};