#include <Arduino.h>
#include <algorithm>
#include "../../global/Global.h"
#include "DeviceActions.h"

class Devices : public Screen {
   private:
    std::vector<Device> devices;
    int selectedDeviceIndex;

   public:
    void onEnter() override {
        LOG_DEBUG("Devices onEnter");
        selectedDeviceIndex = 0;
        loadDevices();

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("Devices onButtonClick");
            // Navigate through devices
            if (devices.size() > 0) {
                selectedDeviceIndex = (selectedDeviceIndex + 1) % devices.size();
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("Devices onButtonLongPress");
            if (devices.size() > 0) {
                // Navigate to device actions page
                Device selectedDevice = devices[selectedDeviceIndex];
                router.push(new DeviceActions(selectedDevice));
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
            return;
        }

        // Show devices list
        int startY = 20;
        int maxVisible = 3;  // Maximum devices visible at once
        int startIndex = std::max(0, selectedDeviceIndex - 1);
        int endIndex = std::min((int)devices.size(), startIndex + maxVisible);

        for (int i = startIndex; i < endIndex; i++) {
            bool isSelected = (i == selectedDeviceIndex);
            String deviceText = String(devices[i].id) + ": " + devices[i].name;

            // Truncate if too long
            if (deviceText.length() > 16) {
                deviceText = deviceText.substring(0, 13) + "...";
            }

            display.drawMenuItem(deviceText.c_str(), i - startIndex, endIndex - startIndex,
                                 isSelected, startY);
        }

        // Show navigation hint
        display.setTextSize(1);
        display.printCentered("Long press for actions", 50);
    }

    void onExit() override { LOG_DEBUG("Devices onExit"); }
};
