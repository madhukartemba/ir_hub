#include "../../global/Global.h"
#include "../../preferences.h"
#include "UserPrefs.h"

class DisableAlexaConfirmation : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[DisableAlexaConfirmation] onEnter");
        ledRing.breathe(COLOR_WARNING_DARK);

        button.setClickCallback([this]() {
            LOG_DEBUG("[DisableAlexaConfirmation] onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[DisableAlexaConfirmation] onButtonLongPress - Confirm");
            disableAlexa();
        });
    }

    void onUpdate() override {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Disable Alexa?", 0);
        display.drawLine(0, 12, display.getWidth(), 12);

        display.printCentered("Alexa discovery", 22);
        display.printCentered("and control will", 32);
        display.printCentered("stop immediately", 42);
        display.printCentered("Long press confirm", 50);
        display.update();
    }

    void onExit() override { LOG_DEBUG("[DisableAlexaConfirmation] onExit"); }

   private:
    void disableAlexa() {
        userPrefsSetAlexaEnabled(false);
        alexaConnector.shutdown();
        speaker.shortBeep();
        router.pop();
    }
};
