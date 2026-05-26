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
        // Show title
        display.setTextSize(1);
        display.printCentered("Delete Device?", 0);

        display.drawLine(0, 12, display.getWidth(), 12);

        // Show device info — short uuid prefix keeps the line ≤16 chars wide.
        display.print("Device " + device.uuid.substring(0, 6), 0, 20);
        display.print(device.name, 0, 30);

        // Show confirmation options
        display.printCentered(confirmed ? "> YES <" : "YES", 45);
        display.printCentered(confirmed ? "NO" : "> NO <", 55);
    }

    void onExit() override {
        LOG_DEBUG("[DeviceDeleteConfirmation] onExit");
    }
};
