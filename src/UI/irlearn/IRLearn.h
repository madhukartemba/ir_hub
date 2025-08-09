#pragma once
#include "../../global/Global.h"
#include "DualModeIRLearn.h"
#include "SingleModeIRLearn.h"

class IRLearn : public Screen {
   private:
    enum class State {
        SINGLE_MODE,
        DUAL_MODE,
        BACK,
    };

    State currentState;

   public:
    IRLearn() : currentState(State::SINGLE_MODE) {}

    void onEnter() override {
        LOG_DEBUG("IRLearn onEnter");
        currentState = State::SINGLE_MODE;

        button.setClickCallback([this]() {
            LOG_DEBUG("IRLearn onButtonClick");
            switch (currentState) {
                case State::SINGLE_MODE:
                    currentState = State::DUAL_MODE;
                    break;
                case State::DUAL_MODE:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::SINGLE_MODE;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("IRLearn onButtonLongPress");
            switch (currentState) {
                case State::SINGLE_MODE:
                    LOG_DEBUG("Entering Single Mode IR Learning");
                    router.push(new SingleModeIRLearn());
                    break;
                case State::DUAL_MODE:
                    LOG_DEBUG("Entering Dual Mode IR Learning");
                    router.push(new DualModeIRLearn());
                    break;
                case State::BACK:
                    LOG_DEBUG("Going back to previous menu");
                    router.pop();
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        switch (currentState) {
            case State::SINGLE_MODE:
                drawSingleMode();
                break;
            case State::DUAL_MODE:
                drawDualMode();
                break;
            case State::BACK:
                drawBack();
                break;
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("IRLearn onExit"); }

   private:
    void drawSingleMode() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Learn", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 0);  // Single Mode is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show instructions at bottom
        display.setTextSize(1);
        display.print("Click: Navigate", 2, 50);
        display.print("Hold: Select", 2, 58);
    }

    void drawDualMode() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Learn", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 1);  // Dual Mode is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show instructions at bottom
        display.setTextSize(1);
        display.print("Click: Navigate", 2, 50);
        display.print("Hold: Select", 2, 58);
    }

    void drawBack() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Learn", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 2);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show instructions at bottom
        display.setTextSize(1);
        display.print("Click: Navigate", 2, 50);
        display.print("Hold: Select", 2, 58);
    }
};
