#include "../../global/Global.h"

class AuthorEasterEggScreen : public Screen {
   private:
    unsigned long enterAtMs = 0;
    bool showStarted = false;
    bool chime2Played = false;
    bool chime3Played = false;

   public:
    void onEnter() override {
        LOG_DEBUG("[AuthorEasterEggScreen] onEnter");
        enterAtMs = millis();
        showStarted = false;
        chime2Played = false;
        chime3Played = false;

        // Allow quick exit too.
        button.setClickCallback([this]() { router.pop(); });
        button.setLongPressCallback([this]() { router.pop(); });
    }

    void onUpdate() override {
        unsigned long now = millis();

        // Enter screen first, then kick off the show.
        if (!showStarted && (now - enterAtMs) >= 120UL) {
            showStarted = true;
            enterAtMs = now;
            ledRing.rainbow();  // RGB signature
            speaker.shortBeep();
        }

        if (showStarted) {
            unsigned long t = now - enterAtMs;
            if (!chime2Played && t >= 260UL) {
                speaker.shortBeep();
                chime2Played = true;
            }
            if (!chime3Played && t >= 520UL) {
                speaker.shortBeep();
                chime3Played = true;
            }
            if (t >= 2000UL) {
                router.pop();
                return;
            }
        }

        display.clear();
        display.setTextSize(1);
        display.printCentered("Made with ❤️", 20);
        display.printCentered("by Madhukar", 38);
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[AuthorEasterEggScreen] onExit");
        ledRing.breathe(COLOR_SETTINGS);
    }
};
