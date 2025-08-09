#include <Arduino.h>
#include "../global/Global.h"
#include "../ui/irlearn/IRLearn.h"
#include "../ui/settings/Settings.h"

class MainMenu : public Screen {
    private:
        enum class State {
            STATUS,
            IR_LEARN,
            SETTINGS,
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
                currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 3);
            });

            // Change button long press behavior
            button.setLongPressCallback([this]() {
                LOG_DEBUG("MainMenu onButtonLongPress");
                if(currentState == State::STATUS) {
                    // Do nothing
                } else if(currentState == State::IR_LEARN) {
                    router.push(new IRLearn());
                } else if(currentState == State::SETTINGS) {
                    router.push(new Settings());
                }
            });
        }

        void onUpdate() override {
            switch (currentState) {
                case State::STATUS:
                    break;
                case State::IR_LEARN:
                    break;
                case State::SETTINGS:
                    break;
            }
        }

        void onExit() override {
            LOG_DEBUG("MainMenu onExit");
        }
};