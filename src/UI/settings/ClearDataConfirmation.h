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
        display.drawConfirmLongPress("Erase Saved Data?", "This will delete", "ALL stored data",
                                     "and restart device");
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[ClearDataConfirmation] onExit");
    }

   private:
    void clearAllDataAndRestart() {
        display.clear();
        display.setTextSize(1);
        display.drawConfirmBody2("Erasing data...", "Please wait");
        display.update();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();

        LOG_INFO("[ClearData] Disconnecting MQTT + WiFi before format()");
        mqttConnector.shutdown();
        WiFi.disconnect(true);
        delay(150);

        bool ok = LittleFS.format();
        LOG_INFO("[ClearData] LittleFS.format() returned %s", ok ? "true" : "false");

        speaker.successBeep();

        display.clear();
        display.setTextSize(1);
        display.drawConfirmBody2("Saved data erased", "Restarting...");
        display.update();
        delay(800);

        ESP.restart();
    }
};
