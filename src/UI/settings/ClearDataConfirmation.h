#include <ESP8266WiFi.h>
#include "../../global/Global.h"
#include "../../preferences.h"

class ClearDataConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("ClearDataConfirmation onEnter");
        ledRing.breathe(COLOR_ERROR);

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

    void onExit() override {
        LOG_DEBUG("ClearDataConfirmation onExit");
    }

   private:
    void clearAllDataAndRestart() {
        // Show the user what we're about to do *before* tearing the network
        // down, so they don't stare at a frozen screen.
        display.clear();
        display.setTextSize(1);
        display.printCentered("Wiping...", 22);
        display.printCentered("Please wait", 36);
        display.update();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();

        // Bring the network sessions down cleanly first. Otherwise PubSubClient
        // keeps retrying mid-format and we see a noisy log full of "rc=-2".
        LOG_INFO("[ClearData] Disconnecting MQTT + WiFi before format()");
        mqttConnector.shutdown();
        WiFi.disconnect(true);
        delay(150);  // let the radio quiesce

        bool ok = LittleFS.format();
        LOG_INFO("[ClearData] LittleFS.format() returned %s", ok ? "true" : "false");

        speaker.successBeep();

        display.clear();
        display.printCentered("All data cleared", 22);
        display.printCentered("Restarting...", 38);
        display.update();
        delay(800);

        ESP.restart();
    }
};
