#include <ESP.h>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"
#include "ClearDataConfirmation.h"

class Settings : public Screen {
   private:
    enum class State {
        RESTART,
        CLEAR_DATA,
        WIFI_WIPE,
        BACK,
        RESTARTING,
    };
    State currentState;
    unsigned long restartStartTime;
    int selectedIndex;
    const char* menuItems[4] = {"Restart", "Clear Data", "Wipe Wi-Fi", "Back"};

   public:
    void onEnter() override {
        LOG_DEBUG("Settings onEnter");
        currentState = State::RESTART;
        selectedIndex = 0;

        ledRing.breathe(COLOR_SETTINGS, 1.0f);

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            if (currentState == State::RESTARTING) {
                // Do nothing during restart
                return;
            }

            // Navigate through menu items
            selectedIndex = (selectedIndex + 1) % 4;
            currentState = static_cast<State>(selectedIndex);
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");

            switch (currentState) {
                case State::RESTART:
                    LOG_DEBUG("Settings onButtonLongPress RESTART");
                    currentState = State::RESTARTING;
                    restartStartTime = millis();
                    break;
                case State::CLEAR_DATA:
                    LOG_DEBUG("Settings onButtonLongPress CLEAR_DATA");
                    router.push(new ClearDataConfirmation());
                    break;
                case State::WIFI_WIPE:
                    LOG_DEBUG("Settings onButtonLongPress WIFI_WIPE");
                    // Wipe WiFi credentials using our WiFiManagerLib
                    wifiManager.resetWiFi();
                    speaker.successBeep();
                    // Restart to trigger WiFi setup
                    currentState = State::RESTARTING;
                    restartStartTime = millis();
                    break;
                case State::BACK:
                    LOG_DEBUG("Settings onButtonLongPress BACK");
                    router.pop();
                    break;
                case State::RESTARTING:
                    // Do nothing during restart
                    break;
            }
        });
    }

    void onUpdate() override {
        display.clear();

        if (currentState == State::RESTARTING) {
            drawRestarting();
            // Restart after 2 seconds
            if (millis() - restartStartTime > 2000) {
                ESP.restart();
            }
        } else {
            drawMenu();
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("Settings onExit");
        ledRing.blank();
    }

    void drawMenu() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Settings", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Use the scrollable menu utility
        MenuUtils::drawScrollableMenu(menuItems, 4, selectedIndex, 3, 20);
    }

    void drawRestarting() {
        // Clear display and show restarting message
        display.clear();

        // Draw title
        display.setTextSize(1);
        display.printCentered("Restarting...", 20);

        // Show a brief message before restart
        display.setTextSize(1);
        display.printCentered("Please wait", 35);
    }
};