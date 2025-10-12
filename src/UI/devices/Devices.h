#include <Arduino.h>
#include <algorithm>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"
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

        ledRing.breathe(COLOR_INFO_LIGHT);

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
        devices = deviceManager.getDevices();
        LOG_INFO("Loaded %d devices", devices.size());
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

        // Create menu items vector
        std::vector<std::string> menuItems;
        for (const auto& device : devices) {
            String deviceText = device.name;
            // Truncate if too long
            if (deviceText.length() > 16) {
                deviceText = deviceText.substring(0, 13) + "...";
            }
            menuItems.push_back(deviceText.c_str());
        }
        menuItems.push_back("Back");

        // Use the scrollable menu utility
        MenuUtils::drawScrollableMenu(menuItems, selectedIndex, 3, 20);
    }

    void onExit() override {
        LOG_DEBUG("Devices onExit");
    }
};
