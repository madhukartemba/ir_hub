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
        display.drawConfirmLongPress("Disable Alexa?", "Alexa discovery", "and control will",
                                     "stop immediately");
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
