#include "../../global/Global.h"

class ClearDataConfirmation : public Screen {
   private:
    enum class State {
        YES,
        NO,
    };
    State currentState;

   public:
    void onEnter() override {
        LOG_DEBUG("ClearDataConfirmation onEnter");
        currentState = State::NO;  // Default to NO for safety

        button.setClickCallback([this]() {
            LOG_DEBUG("ClearDataConfirmation onButtonClick");
            switch (currentState) {
                case State::YES:
                    currentState = State::NO;
                    break;
                case State::NO:
                    currentState = State::YES;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("ClearDataConfirmation onButtonLongPress");
            switch (currentState) {
                case State::YES:
                    LOG_DEBUG("ClearDataConfirmation onButtonLongPress YES");
                    clearAllDataAndRestart();
                    break;
                case State::NO:
                    LOG_DEBUG("ClearDataConfirmation onButtonLongPress NO");
                    router.pop();
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        // Draw title
        display.setTextSize(1);
        display.printCentered("Clear All Data?", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Draw warning message
        display.setTextSize(1);
        display.printCentered("This will delete", 20);
        display.printCentered("ALL stored data", 30);
        display.printCentered("and restart device", 40);

        // Show confirmation options with selection indicator
        const char* menuItems[] = {"Yes", "No"};
        int startY = 55;

        for (int i = 0; i < 2; i++) {
            bool isSelected = (i == (currentState == State::YES ? 0 : 1));
            display.drawMenuItem(menuItems[i], i, 2, isSelected, startY);
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("ClearDataConfirmation onExit"); }

   private:
    void clearAllDataAndRestart() {
        // Clear all data from LittleFS
        Dir dir = LittleFS.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            if (fileName != "/") {
                LittleFS.remove(fileName);
                LOG_DEBUG("Removed file: " + fileName);
            }
        }
        LOG_INFO("All data cleared from LittleFS");
        // Restart the device
        ESP.restart();
    }
};
