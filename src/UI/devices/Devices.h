#include <Arduino.h>
#include <algorithm>
#include "../../global/Global.h"

class Devices : public Screen {
   private:
    enum class State { LIST, DETAILS };
    State currentState;
    std::vector<Device> devices;
    int selectedDeviceIndex;
    Device currentDevice;

   public:
    void onEnter() override {
        LOG_DEBUG("Devices onEnter");
        currentState = State::LIST;
        selectedDeviceIndex = 0;
        loadDevices();

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("Devices onButtonClick");
            if (currentState == State::LIST) {
                // Navigate through devices
                if (devices.size() > 0) {
                    selectedDeviceIndex = (selectedDeviceIndex + 1) % devices.size();
                }
            } else if (currentState == State::DETAILS) {
                // Go back to list
                currentState = State::LIST;
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("Devices onButtonLongPress");
            if (currentState == State::LIST && devices.size() > 0) {
                // Show device details
                currentDevice = devices[selectedDeviceIndex];
                currentState = State::DETAILS;
            } else if (currentState == State::DETAILS) {
                // Go back to list
                currentState = State::LIST;
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

        if (currentState == State::LIST) {
            displayList();
        } else if (currentState == State::DETAILS) {
            displayDetails();
        }

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
        display.printCentered("Long press for details", 50);
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
        display.print("ID: " + String(currentDevice.id), 0, y);
        y += 10;

        // Device Name
        display.print("Name: " + currentDevice.name, 0, y);
        y += 10;

        // Device Type
        display.print("Type: " + String(currentDevice.type == SINGLE_COMMAND ? "Single" : "Dual"),
                      0, y);
        y += 10;

        // Protocol
        display.print("Protocol: " + currentDevice.protocolName, 0, y);
        y += 10;

        // Show back hint
        display.printCentered("Long press to go back", 50);
    }

    void onExit() override { LOG_DEBUG("Devices onExit"); }
};
