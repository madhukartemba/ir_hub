#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "PendingOta.h"
#include "../../global/Global.h"
#include "../../preferences.h"

class FactoryResetConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[FactoryResetConfirmation] onEnter");
        ledRing.breathe(COLOR_ERROR);

        button.setClickCallback([this]() {
            LOG_DEBUG("[FactoryResetConfirmation] onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[FactoryResetConfirmation] onButtonLongPress - Confirm");
            factoryResetAndRestart();
        });
    }

    void onUpdate() override {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Factory Reset?", 0);
        display.drawLine(0, 12, display.getWidth(), 12);

        display.printCentered("Will reset Wi-Fi", 20);
        display.printCentered("+ all saved data", 30);
        display.printCentered("+ pending OTA", 40);

        display.printCentered("Long press confirm", 52);
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[FactoryResetConfirmation] onExit");
    }

   private:
    void factoryResetAndRestart() {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Factory reset...", 20);
        display.printCentered("Please wait", 34);
        display.update();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();

        LOG_INFO("[FactoryReset] Shutting down MQTT + Wi-Fi");
        mqttConnector.shutdown();
        WiFi.disconnect(true);
        delay(150);

        LOG_INFO("[FactoryReset] Clearing Wi-Fi credentials");
        wifiManager.resetWiFi();

        LOG_INFO("[FactoryReset] Clearing pending OTA RTC slot");
        pending_ota::clear();

        LOG_INFO("[FactoryReset] Formatting LittleFS");
        bool fsOk = LittleFS.format();
        LOG_INFO("[FactoryReset] LittleFS.format() returned %s", fsOk ? "true" : "false");

        if (fsOk) {
            speaker.successBeep();
            display.clear();
            display.printCentered("Factory reset done", 22);
            display.printCentered("Restarting...", 38);
        } else {
            speaker.errorBeep();
            display.clear();
            display.printCentered("Reset partial", 22);
            display.printCentered("Restarting...", 38);
        }
        display.update();
        delay(900);
        ESP.restart();
    }
};
