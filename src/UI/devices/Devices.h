#include <Arduino.h>
#include <algorithm>
#include "../../global/Global.h"
#include "DeviceActions.h"

class Devices : public Screen {
   private:
    std::vector<Device> devices;
    int selectedIndex;  // Changed from selectedDeviceIndex to handle back button too

   public:
    void onEnter() override {
        LOG_DEBUG("Devices onEnter");
        selectedIndex = 0;
        loadDevices();

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("Devices onButtonClick");
            // Navigate through devices and back option
            int totalItems = devices.size() + 1;  // +1 for back button
            selectedIndex = (selectedIndex + 1) % totalItems;
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("Devices onButtonLongPress");
            if (selectedIndex < static_cast<int>(devices.size())) {
                // Navigate to device actions page
                Device selectedDevice = devices[selectedIndex];
                router.push(new DeviceActions(selectedDevice));
            } else {
                // Back button selected - go back
                router.pop();
            }
        });
    }

    void loadDevices() {
        try {
            devices = deviceManager.getDevices();
            LOG_INFO("Loaded %d devices", devices.size());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to load devices: %s", e.what());
            devices.clear();
        }
    }

    void onUpdate() override {
        display.clear();
        displayList();
        display.update();
    }

    void displayList() {
        // Show title
        display.setTextSize(1);
        display.printCentered("Devices", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        if (devices.empty()) {
            display.printCentered("No devices found", 30);
            // Still show back button when no devices
            display.drawMenuItem("Back", 0, 1, selectedIndex == 0, 50);
            return;
        }

        // Show devices list
        int startY = 20;
        int maxVisible = 3;                   // Maximum devices visible at once
        int totalItems = devices.size() + 1;  // +1 for back button

        // Calculate which items to show
        int startIndex = std::max(0, selectedIndex - 1);
        int endIndex = std::min(totalItems, startIndex + maxVisible);

        // Adjust if we're at the end and need to show back button
        if (selectedIndex == static_cast<int>(devices.size()) &&
            endIndex <= static_cast<int>(devices.size())) {
            startIndex = std::max(0, static_cast<int>(devices.size()) - maxVisible + 1);
            endIndex = totalItems;
        }

        for (int i = startIndex; i < endIndex; i++) {
            bool isSelected = (i == selectedIndex);

            if (i < static_cast<int>(devices.size())) {
                // Show device
                String deviceText = String(devices[i].id) + ": " + devices[i].name;

                // Truncate if too long
                if (deviceText.length() > 16) {
                    deviceText = deviceText.substring(0, 13) + "...";
                }

                display.drawMenuItem(deviceText.c_str(), i - startIndex, endIndex - startIndex,
                                     isSelected, startY);
            } else {
                // Show back button
                display.drawMenuItem("Back", i - startIndex, endIndex - startIndex, isSelected,
                                     startY);
            }
        }
    }

    void onExit() override { LOG_DEBUG("Devices onExit"); }
};
