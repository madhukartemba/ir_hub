#include <ESP.h>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"
#include "ClearDataConfirmation.h"
#include "UserPrefs.h"

class Settings : public Screen {
   private:
    // One row per concrete action. RESTARTING is an internal screen state,
    // not a row. The "HAPTICS" row only appears when the DRV2605 driver was
    // detected at boot — otherwise toggling it would have no observable
    // effect and would just confuse the user.
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
        LOG_DEBUG("Settings onEnter");

        hasHaptics = haptics.isReady();
        buildMenu();
        selectedIndex = 0;
        isRestarting = false;

        ledRing.breathe(COLOR_SETTINGS);

        button.setClickCallback([this]() {
            LOG_DEBUG("Settings onButtonClick");
            if (isRestarting) {
                return;
            }
            selectedIndex = (selectedIndex + 1) % menuCount;
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("Settings onButtonLongPress");
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
        LOG_DEBUG("Settings onExit");
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

    // Re-resolve labels that change at runtime (the toggles). Static labels
    // are picked up via the same call but are no-ops.
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
                LOG_DEBUG("Settings onButtonLongPress RESTART");
                isRestarting = true;
                restartStartTime = millis();
                break;
            case Action::CLEAR_DATA:
                LOG_DEBUG("Settings onButtonLongPress CLEAR_DATA");
                router.push(new ClearDataConfirmation());
                break;
            case Action::WIFI_WIPE:
                LOG_DEBUG("Settings onButtonLongPress WIFI_WIPE");
                wifiManager.resetWiFi();
                speaker.successBeep();
                isRestarting = true;
                restartStartTime = millis();
                break;
            case Action::BACK:
                LOG_DEBUG("Settings onButtonLongPress BACK");
                router.pop();
                break;
        }
    }

    void toggleSound() {
        bool newState = !userPrefsSoundEnabled();
        userPrefsSetSoundEnabled(newState);
        speaker.setMuted(!newState);
        // Audible confirmation only when turning ON, so we don't contradict
        // the user's "off" choice.
        if (newState) {
            speaker.shortBeep();
        }
        LOG_INFO("[Settings] Sound %s", newState ? "ON" : "OFF");
    }

    void toggleHaptics() {
        bool newState = !userPrefsHapticsEnabled();
        userPrefsSetHapticsEnabled(newState);
        haptics.setMuted(!newState);
        // Tactile confirmation only when turning ON, same rationale as
        // toggleSound. Skipped if the driver isn't actually present.
        if (newState && haptics.isReady()) {
            haptics.playSelection();
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
