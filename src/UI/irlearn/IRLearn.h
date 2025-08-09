#pragma once
#include "../../global/Global.h"
#include "SingleModeIRLearn.h"
#include "DualModeIRLearn.h"

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
        }
        
        void onExit() override {
            LOG_DEBUG("IRLearn onExit");
        }

    private:
        void drawSingleMode() {
            // Draw UI showing "Single Mode" selected
            // TODO: Implement UI drawing
        }
        
        void drawDualMode() {
            // Draw UI showing "Dual Mode" selected
            // TODO: Implement UI drawing
        }
        
        void drawBack() {
            // Draw UI showing "Back" selected
            // TODO: Implement UI drawing
        }
};
