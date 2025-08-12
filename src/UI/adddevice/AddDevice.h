#pragma once
#include "../../global/Global.h"
#include "DualModeAddDevice.h"
#include "SingleModeAddDevice.h"
#include "AutoModeAddDevice.h"

class AddDevice : public Screen {
   private:
    enum class State {
        SINGLE_MODE,
        DUAL_MODE,
        AUTO_MODE,
        BACK,
    };

    State currentState;

   public:
    AddDevice() : currentState(State::SINGLE_MODE) {}

    void onEnter() override {
        LOG_DEBUG("AddDevice onEnter");
        currentState = State::SINGLE_MODE;

        button.setClickCallback([this]() {
            LOG_DEBUG("AddDevice onButtonClick");
            switch (currentState) {
                case State::SINGLE_MODE:
                    currentState = State::DUAL_MODE;
                    break;
                case State::DUAL_MODE:
                    currentState = State::AUTO_MODE;
                    break;
                case State::AUTO_MODE:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::SINGLE_MODE;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("AddDevice onButtonLongPress");
            switch (currentState) {
                case State::SINGLE_MODE:
                    LOG_DEBUG("Entering Single Mode Device Addition");
                    router.push(new SingleModeAddDevice());
                    break;
                case State::DUAL_MODE:
                    LOG_DEBUG("Entering Dual Mode Device Addition");
                    router.push(new DualModeAddDevice());
                    break;
                case State::AUTO_MODE:
                    LOG_DEBUG("Entering Auto Mode Device Addition");
                    router.push(new AutoModeAddDevice());
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
            case State::AUTO_MODE:
                drawAutoMode();
                break;
            case State::BACK:
                drawBack();
                break;
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("AddDevice onExit"); }

   private:
    void drawSingleMode() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Auto Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 4; i++) {
            bool isSelected = (i == 0);  // Single Mode is selected
            display.drawMenuItem(menuItems[i], i, 4, isSelected, startY);
        }
    }

    void drawDualMode() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Auto Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 4; i++) {
            bool isSelected = (i == 1);  // Dual Mode is selected
            display.drawMenuItem(menuItems[i], i, 4, isSelected, startY);
        }
    }

    void drawAutoMode() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Auto Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 4; i++) {
            bool isSelected = (i == 2);  // Auto Mode is selected
            display.drawMenuItem(menuItems[i], i, 4, isSelected, startY);
        }
    }

    void drawBack() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Single Mode", "Dual Mode", "Auto Mode", "Back"};
        int startY = 20;

        for (int i = 0; i < 4; i++) {
            bool isSelected = (i == 3);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 4, isSelected, startY);
        }
    }
};
