#include <ESP8266WiFi.h>
#include "../../global/Global.h"
#include "../../preferences.h"

class ClearDataConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[ClearDataConfirmation] onEnter");
        ledRing.breathe(COLOR_ERROR);

        button.setClickCallback([this]() {
            LOG_DEBUG("[ClearDataConfirmation] onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[ClearDataConfirmation] onButtonLongPress - Confirm");
            clearAllDataAndRestart();
        });
    }

    void onUpdate() override {
        display.clear();

        // Draw title
        display.setTextSize(1);
        display.printCentered("Erase Saved Data?", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Draw warning message
        display.setTextSize(1);
        display.printCentered("This will delete", 20);
        display.printCentered("ALL stored data", 30);
        display.printCentered("and restart device", 40);

        // Draw instructions
        display.setTextSize(1);
        display.printCentered("Long press to confirm", 48);
        display.printCentered("Click to exit", 54);

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[ClearDataConfirmation] onExit");
    }

   private:
    void clearAllDataAndRestart() {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Erasing data...", 22);
        display.printCentered("Please wait", 36);
        display.update();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();

        // Close network sessions cleanly so PubSubClient doesn't spam
        // reconnect attempts during the format.
        LOG_INFO("[ClearData] Disconnecting MQTT + WiFi before format()");
        mqttConnector.shutdown();
        WiFi.disconnect(true);
        delay(150);

        bool ok = LittleFS.format();
        LOG_INFO("[ClearData] LittleFS.format() returned %s", ok ? "true" : "false");

        speaker.successBeep();

        display.clear();
        display.printCentered("Saved data erased", 22);
        display.printCentered("Restarting...", 38);
        display.update();
        delay(800);

        ESP.restart();
    }
};
