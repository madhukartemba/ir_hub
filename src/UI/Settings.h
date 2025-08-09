#include "../global/Global.h"

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
            switch (currentState) {
                case State::RESET_SETTINGS:
                    drawResetSettings();
                    break;
                case State::BACK:
                    drawBack();
                    break;
            }
        }

        void onExit() override {
            LOG_DEBUG("Settings onExit");
        }

        void drawResetSettings() {
            
        }

        void drawBack() {
            
        }
};