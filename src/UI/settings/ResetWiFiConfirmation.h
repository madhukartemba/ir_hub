#include <ESP8266WiFi.h>
#include "../../global/Global.h"
#include "../../preferences.h"

class ResetWiFiConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[ResetWiFiConfirmation] onEnter");
        ledRing.breathe(COLOR_WARNING_DARK);

        button.setClickCallback([this]() {
            LOG_DEBUG("[ResetWiFiConfirmation] onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[ResetWiFiConfirmation] onButtonLongPress - Confirm");
            resetWiFiAndRestart();
        });
    }

    void onUpdate() override {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Reset Wi-Fi?", 0);
        display.drawLine(0, 12, display.getWidth(), 12);

        display.printCentered("This removes saved", 22);
        display.printCentered("Wi-Fi credentials", 32);
        display.printCentered("and restarts", 42);
        display.printCentered("Long press confirm", 50);
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[ResetWiFiConfirmation] onExit");
    }

   private:
    void resetWiFiAndRestart() {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Resetting Wi-Fi...", 22);
        display.printCentered("Please wait", 36);
        display.update();
        ledRing.solid(COLOR_WARNING);
        ledRing.finishTransition();

        // Close sessions before dropping network credentials.
        mqttConnector.shutdown();
        WiFi.disconnect(true);
        delay(150);

        wifiManager.resetWiFi();
        speaker.successBeep();

        display.clear();
        display.printCentered("Wi-Fi reset done", 22);
        display.printCentered("Restarting...", 38);
        display.update();
        delay(900);
        ESP.restart();
    }
};
