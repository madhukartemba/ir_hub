#include <Arduino.h>
#include "../../global/Global.h"

class DeviceDetails : public Screen {
   private:
    Device device;

   public:
    DeviceDetails(Device& device) : device(device) {}

    void onEnter() override {
        LOG_DEBUG("DeviceDetails onEnter for device %d", device.id);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonClick");
            // Go back to device actions
            router.pop();
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonLongPress");
            // Go back to device actions
            router.pop();
        });
    }

    void onUpdate() override {
        display.clear();
        displayDetails();
        display.update();
    }

    void displayDetails() {
        // Show title
        display.setTextSize(1);
        display.printCentered("Device Details", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show device information
        int y = 20;
        display.setTextSize(1);

        // Device ID
        display.print("ID: " + String(device.id), 0, y);
        y += 10;

        // Device Name
        display.print("Name: " + device.name, 0, y);
        y += 10;

        // Device Type
        display.print("Type: " + String(device.type == SINGLE_COMMAND ? "Single" : "Dual"), 0, y);
        y += 10;

        // Protocol
        display.print("Protocol: " + device.protocolName, 0, y);
        y += 10;

        // Show back hint
        display.printCentered("Click or Long press to go back", 50);
    }

    void onExit() override { LOG_DEBUG("DeviceDetails onExit"); }
};
