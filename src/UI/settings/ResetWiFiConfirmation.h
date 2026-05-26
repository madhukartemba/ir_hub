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
        display.drawConfirmLongPress("Reset Wi-Fi?", "This removes saved", "Wi-Fi credentials",
                                     "and restarts");
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[ResetWiFiConfirmation] onExit");
    }

   private:
    void resetWiFiAndRestart() {
        display.clear();
        display.setTextSize(1);
        display.drawConfirmBody2("Resetting Wi-Fi...", "Please wait");
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
        display.setTextSize(1);
        display.drawConfirmBody2("Wi-Fi reset done", "Restarting...");
        display.update();
        delay(900);
        ESP.restart();
    }
};
