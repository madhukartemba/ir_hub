#include <Arduino.h>
#include "../global/Global.h"

class MainMenu : public Screen {
    public:
        void onEnter() override {
            LOG_INFO("MainMenu onEnter");
        }

        void onUpdate() override {
            LOG_INFO("MainMenu onUpdate");
        }

        void onExit() override {
            LOG_INFO("MainMenu onExit");
        }
};