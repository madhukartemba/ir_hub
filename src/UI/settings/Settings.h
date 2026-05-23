#include <ESP.h>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"
#include "ClearDataConfirmation.h"
#include "UserPrefs.h"

class Settings : public Screen {
   private:
    // One row per concrete menu action. HAPTICS row only shown when DRV2605
    // is present (`hasHaptics`).
    enum class Action {
        SOUND,
        HAPTICS,
        RESTART,
        CLEAR_DATA,
        WIFI_WIPE,
        BACK,
    };

    static constexpr int kMaxItems = 6;
    const char* menuItems[kMaxItems];
    Action menuActions[kMaxItems];
    int menuCount = 0;

    int selectedIndex = 0;
    bool isRestarting = false;
    unsigned long restartStartTime = 0;
    bool hasHaptics = false;

   public:
    void onEnter() override {
        LOG_DEBUG("[Settings] onEnter");

        // Gate on `isPresent`, not `isReady`: the chip may be uncalibrated if
        // the user had haptics muted at boot — we still need the toggle row.
        hasHaptics = haptics.isPresent();
        buildMenu();
        selectedIndex = 0;
        isRestarting = false;

        ledRing.breathe(COLOR_SETTINGS);

        button.setClickCallback([this]() {
            LOG_DEBUG("[Settings] onButtonClick");
            if (isRestarting) {
                return;
            }
            selectedIndex = (selectedIndex + 1) % menuCount;
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[Settings] onButtonLongPress");
            if (isRestarting) {
                return;
            }
            handleAction(menuActions[selectedIndex]);
        });
    }

    void onUpdate() override {
        display.clear();

        if (isRestarting) {
            drawRestarting();
            if (millis() - restartStartTime > 2000) {
                ESP.restart();
            }
        } else {
            refreshDynamicLabels();
            drawMenu();
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[Settings] onExit");
    }

   private:
    void buildMenu() {
        menuCount = 0;
        addRow(Action::SOUND);
        if (hasHaptics) {
            addRow(Action::HAPTICS);
        }
        addRow(Action::RESTART);
        addRow(Action::CLEAR_DATA);
        addRow(Action::WIFI_WIPE);
        addRow(Action::BACK);
    }

    void addRow(Action action) {
        if (menuCount >= kMaxItems) {
            return;
        }
        menuActions[menuCount] = action;
        menuItems[menuCount] = labelFor(action);
        menuCount++;
    }

    // Re-resolve labels that change at runtime (the toggles).
    void refreshDynamicLabels() {
        for (int i = 0; i < menuCount; i++) {
            menuItems[i] = labelFor(menuActions[i]);
        }
    }

    static const char* labelFor(Action action) {
        switch (action) {
            case Action::SOUND:
                return userPrefsSoundEnabled() ? "Sound: On" : "Sound: Off";
            case Action::HAPTICS:
                return userPrefsHapticsEnabled() ? "Haptics: On" : "Haptics: Off";
            case Action::RESTART:
                return "Restart";
            case Action::CLEAR_DATA:
                return "Clear Data";
            case Action::WIFI_WIPE:
                return "Wipe Wi-Fi";
            case Action::BACK:
                return "Back";
        }
        return "?";
    }

    void handleAction(Action action) {
        switch (action) {
            case Action::SOUND:
                toggleSound();
                break;
            case Action::HAPTICS:
                toggleHaptics();
                break;
            case Action::RESTART:
                LOG_DEBUG("[Settings] onButtonLongPress RESTART");
                isRestarting = true;
                restartStartTime = millis();
                break;
            case Action::CLEAR_DATA:
                LOG_DEBUG("[Settings] onButtonLongPress CLEAR_DATA");
                router.push(new ClearDataConfirmation());
                break;
            case Action::WIFI_WIPE:
                LOG_DEBUG("[Settings] onButtonLongPress WIFI_WIPE");
                wifiManager.resetWiFi();
                speaker.successBeep();
                isRestarting = true;
                restartStartTime = millis();
                break;
            case Action::BACK:
                LOG_DEBUG("[Settings] onButtonLongPress BACK");
                router.pop();
                break;
        }
    }

    void toggleSound() {
        bool newState = !userPrefsSoundEnabled();
        userPrefsSetSoundEnabled(newState);
        speaker.setMuted(!newState);
        if (newState) {
            speaker.shortBeep();  // confirmation only when turning ON
        }
        LOG_INFO("[Settings] Sound %s", newState ? "ON" : "OFF");
    }

    void toggleHaptics() {
        bool newState = !userPrefsHapticsEnabled();
        userPrefsSetHapticsEnabled(newState);

        if (newState) {
            // Lazy auto-calibration on first re-enable this boot. The LRA
            // buzzes briefly during cal — that's expected user feedback.
            if (!haptics.isReady() && haptics.isPresent()) {
                display.clear();
                display.setTextSize(1);
                display.printCentered("Calibrating", 24);
                display.printCentered("haptics...", 36);
                display.update();
                if (!haptics.begin()) {
                    LOG_WARN("[Settings] DRV2605 calibration failed");
                }
            }
            haptics.setMuted(false);
            if (haptics.isReady()) {
                haptics.playSelection();
            }
        } else {
            haptics.setMuted(true);
        }

        LOG_INFO("[Settings] Haptics %s", newState ? "ON" : "OFF");
    }

    void drawMenu() {
        display.setTextSize(1);
        display.printCentered("Settings", 0);
        display.drawLine(0, 12, display.getWidth(), 12);
        MenuUtils::drawScrollableMenu(menuItems, menuCount, selectedIndex, 3, 20);
    }

    void drawRestarting() {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Restarting...", 20);
        display.setTextSize(1);
        display.printCentered("Please wait", 35);
    }
};
