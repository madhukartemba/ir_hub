#include "../../global/Global.h"

class ClearDataConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("ClearDataConfirmation onEnter");

        button.setClickCallback([this]() {
            LOG_DEBUG("ClearDataConfirmation onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("ClearDataConfirmation onButtonLongPress - Confirm");
            clearAllDataAndRestart();
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

        // Draw instructions
        display.setTextSize(1);
        display.printCentered("Long press to confirm", 55);
        display.printCentered("Click to exit", 65);

        display.update();
    }

    void onExit() override { LOG_DEBUG("ClearDataConfirmation onExit"); }

   private:
    void clearAllDataAndRestart() {
        // Clear all data from LittleFS
        LittleFS.format();
        LOG_INFO("All data cleared from LittleFS");
        // Restart the device
        ESP.restart();
    }
};
