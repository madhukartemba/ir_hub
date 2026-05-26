#include <Arduino.h>
#include "../../global/Global.h"
#include "../../preferences.h"

class DeviceDeleteConfirmation : public Screen {
   private:
    Device device;
    bool confirmed;

   public:
    explicit DeviceDeleteConfirmation(const Device& device) : device(device), confirmed(false) {}

    void onEnter() override {
        LOG_DEBUG("[DeviceDeleteConfirmation] onEnter for device %s", device.uuid.c_str());
        ledRing.breathe(COLOR_ERROR_DARK);

        button.setClickCallback([this]() {
            LOG_DEBUG("[DeviceDeleteConfirmation] onButtonClick");
            confirmed = !confirmed;
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[DeviceDeleteConfirmation] onButtonLongPress");
            if (confirmed) {
                LOG_INFO("[DeviceDeleteConfirmation] Deleting device %s", device.uuid.c_str());
                bool success = deviceManager.removeDeviceByUuid(device.uuid);
                if (success) {
                    speaker.successBeep();
                    display.clear();
                    display.printCentered("Device deleted!", 30);
                    display.update();
                    delay(2000);
                } else {
                    speaker.errorBeep();
                    display.clear();
                    display.printCentered("Failed to delete device", 30);
                    display.update();
                    delay(2000);
                }

                // Go back to devices list
                router.pop(2);  // Pop twice to go back to devices list
            } else {
                // Go back to device actions
                router.pop();
            }
        });
    }

    void onUpdate() override {
        display.clear();
        displayConfirmation();
        display.update();
    }

    void displayConfirmation() {
        display.drawConfirmHeader("Delete Device?");
        display.print("Device " + device.uuid.substring(0, 6), 0, Display::kConfirmBody1Y);
        display.print(device.name, 0, Display::kConfirmBody2Y);
        display.printCentered(confirmed ? "> YES <" : "YES", Display::kConfirmChoice1Y);
        display.printCentered(confirmed ? "NO" : "> NO <", Display::kConfirmChoice2Y);
    }

    void onExit() override {
        LOG_DEBUG("[DeviceDeleteConfirmation] onExit");
    }
};
