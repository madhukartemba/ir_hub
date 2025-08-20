#include <Arduino.h>
#include "../../global/Global.h"

class DeviceDetails : public Screen {
   private:
    Device device;
    int currentPage;  // 0 = device details, 1 = on command description, 2 = off command description

   public:
    DeviceDetails(Device& device) : device(device), currentPage(0) {}

    void onEnter() override {
        LOG_DEBUG("DeviceDetails onEnter for device %d", device.id);
        ring.breathe(5, CRGB::DarkGreen);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonClick");
            // Navigate through pages
            currentPage = (currentPage + 1) % 3;
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
        switch (currentPage) {
            case 0:
                displayDeviceDetails();
                break;
            case 1:
                displayOnCommandDescription();
                break;
            case 2:
                displayOffCommandDescription();
                break;
        }
        display.update();
    }

    void displayDeviceDetails() {
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
    }

    void displayOnCommandDescription() {
        // Show title
        display.setTextSize(1);
        display.printCentered("ON Command", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show on command description
        int y = 20;
        display.setTextSize(1);

        String description = device.onCommand.getDescription();
        if (description.isEmpty()) {
            display.printCentered("No description", 30);
        } else {
            // Split description into multiple lines if needed
            display.print("Description:", 0, y);
            y += 10;

            // Handle long descriptions by wrapping text
            if (description.length() > 16) {
                // Split into multiple lines
                unsigned int startPos = 0;
                while (startPos < description.length() && y < 50) {
                    String line = description.substring(startPos, startPos + 16);
                    display.print(line, 0, y);
                    y += 10;
                    startPos += 16;
                }
            } else {
                display.print(description, 0, y);
            }
        }
    }

    void displayOffCommandDescription() {
        // Show title
        display.setTextSize(1);
        display.printCentered("OFF Command", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show off command description
        int y = 20;
        display.setTextSize(1);

        String description = device.offCommand.getDescription();
        if (description.isEmpty()) {
            display.printCentered("No description", 30);
        } else {
            // Split description into multiple lines if needed
            display.print("Description:", 0, y);
            y += 10;

            // Handle long descriptions by wrapping text
            if (description.length() > 16) {
                // Split into multiple lines
                unsigned int startPos = 0;
                while (startPos < description.length() && y < 50) {
                    String line = description.substring(startPos, startPos + 16);
                    display.print(line, 0, y);
                    y += 10;
                    startPos += 16;
                }
            } else {
                display.print(description, 0, y);
            }
        }
    }

    void onExit() override {
        LOG_DEBUG("DeviceDetails onExit");
        ring.off();
    }
};
