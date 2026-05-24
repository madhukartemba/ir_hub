#include "../../global/Global.h"

class AuthorEasterEggScreen : public Screen {
   private:
    unsigned long enterAtMs = 0;
    bool showStarted = false;
    uint8_t nextNoteIdx = 0;
    static constexpr uint8_t kNoteCount = 6;
    // Short upbeat signature snippet (timings in ms from show start).
    static constexpr unsigned long kNoteAtMs[kNoteCount] = {0UL, 180UL, 360UL, 540UL, 760UL, 980UL};
    static constexpr uint16_t kNoteFreqHz[kNoteCount] = {523, 659, 784, 659, 880, 1047};  // C5 E5 G5 E5 A5 C6
    static constexpr unsigned long kNoteDurMs[kNoteCount] = {120UL, 120UL, 140UL, 120UL, 160UL, 220UL};

    void drawPixelHeart(int centerX, int topY, int scale = 2) {
        static const uint8_t heart[6][7] = {
            {0, 1, 1, 0, 1, 1, 0},
            {1, 1, 1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 1},
            {0, 1, 1, 1, 1, 1, 0},
            {0, 0, 1, 1, 1, 0, 0},
            {0, 0, 0, 1, 0, 0, 0},
        };

        int width = 7 * scale;
        int leftX = centerX - (width / 2);
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 7; col++) {
                if (heart[row][col]) {
                    display.fillRect(leftX + col * scale, topY + row * scale, scale, scale);
                }
            }
        }
    }

   public:
    void onEnter() override {
        LOG_DEBUG("[AuthorEasterEggScreen] onEnter");
        enterAtMs = millis();
        showStarted = false;
        nextNoteIdx = 0;

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
        }

        if (showStarted) {
            unsigned long t = now - enterAtMs;
            while (nextNoteIdx < kNoteCount && t >= kNoteAtMs[nextNoteIdx]) {
                speaker.beep(kNoteFreqHz[nextNoteIdx], kNoteDurMs[nextNoteIdx]);
                nextNoteIdx++;
            }
        }

        display.clear();
        display.setTextSize(1);
        display.printCentered("Made with", 14);
        drawPixelHeart(display.getWidth() / 2, 26, 2);
        display.printCentered("by Madhukar", 44);
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[AuthorEasterEggScreen] onExit");
        ledRing.breathe(COLOR_SETTINGS);
    }
};
