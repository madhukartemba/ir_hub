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
        LOG_DEBUG("[DeviceDeleteConfirmation] onEnter for device %d", device.id);
        ledRing.breathe(COLOR_ERROR_DARK);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("[DeviceDeleteConfirmation] onButtonClick");
            confirmed = !confirmed;
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("[DeviceDeleteConfirmation] onButtonLongPress");
            if (confirmed) {
                // Delete the device
                LOG_INFO("[DeviceDeleteConfirmation] Deleting device %d", device.id);
                bool success = deviceManager.removeDeviceById(device.id);
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

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show device info
        display.print("Device " + String(device.id), 0, 20);
        display.print(device.name, 0, 30);

        // Show confirmation options
        display.printCentered(confirmed ? "> YES <" : "YES", 45);
        display.printCentered(confirmed ? "NO" : "> NO <", 55);
    }

    void onExit() override {
        LOG_DEBUG("[DeviceDeleteConfirmation] onExit");
    }
};
