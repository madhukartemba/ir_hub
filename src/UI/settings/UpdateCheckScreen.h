#include "../../global/Global.h"

class UpdateCheckScreen : public Screen {
   private:
    unsigned long enterAtMs = 0;
    bool checkStarted = false;

   public:
    void onEnter() override {
        LOG_DEBUG("[UpdateCheckScreen] onEnter");
        enterAtMs = millis();
        checkStarted = false;
        ledRing.breathe(COLOR_INFO);

        button.setClickCallback([this]() {
            LOG_DEBUG("[UpdateCheckScreen] onButtonClick - Exit");
            router.pop();
        });
        button.setLongPressCallback([this]() {
            LOG_DEBUG("[UpdateCheckScreen] onButtonLongPress - Exit");
            router.pop();
        });
    }

    void onUpdate() override {
        // Let the new screen paint first, then trigger OTA check.
        if (!checkStarted && (millis() - enterAtMs) >= 200UL) {
            otaUpdater.checkNow();
            checkStarted = true;
        }

        display.clear();
        display.setTextSize(1);
        display.printCentered("Check for Updates", 2);
        display.drawLine(0, 14, display.getWidth(), 14);

        display.printCentered("Status", 20);
        display.printCentered(otaUpdater.lastCheckStatusText(), 30);

        display.printCentered("Current firmware", 42);
        char current[32];
        snprintf(current, sizeof(current), "v%s", otaUpdater.currentVersion());
        display.printCentered(current, 52);

        // Give a little movement while checking so users see active progress.
        if (otaUpdater.lastCheckStatus() == OtaUpdater::CheckStatus::CHECKING) {
            unsigned dots = ((millis() - enterAtMs) / 400UL) % 4U;
            char line[8] = "";
            for (unsigned i = 0; i < dots; i++) {
                line[i] = '.';
                line[i + 1] = '\0';
            }
            display.printCentered(line, 60);
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[UpdateCheckScreen] onExit");
    }
};
