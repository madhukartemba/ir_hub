#include <Arduino.h>
#include "../../global/Global.h"
#include "DeviceDetails.h"

class DeviceActions : public Screen {
   private:
    enum class ActionType { ON, OFF, DETAILS };

    Device device;
    ActionType selectedAction;

   public:
    DeviceActions(Device& device) : device(device), selectedAction(ActionType::ON) {}

    void onEnter() override {
        LOG_DEBUG("DeviceActions onEnter for device %d", device.id);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceActions onButtonClick");
            // Navigate through actions: ON, OFF, Details
            switch (selectedAction) {
                case ActionType::ON:
                    selectedAction = ActionType::OFF;
                    break;
                case ActionType::OFF:
                    selectedAction = ActionType::DETAILS;
                    break;
                case ActionType::DETAILS:
                    selectedAction = ActionType::ON;
                    break;
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("DeviceActions onButtonLongPress");
            if (selectedAction == ActionType::DETAILS) {
                // Navigate to details
                router.push(new DeviceDetails(device));
            } else {
                // Execute ON or OFF action
                executeAction();
            }
        });
    }

    void onUpdate() override {
        display.clear();
        displayActions();
        display.update();
    }

    void displayActions() {
        // Show title with device name
        display.setTextSize(1);
        String title = "Device " + String(device.id);
        display.printCentered(title.c_str(), 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show action options
        int startY = 20;
        const char* actions[] = {"Send ON", "Send OFF", "Details"};

        for (int i = 0; i < 3; i++) {
            bool isSelected = (static_cast<ActionType>(i) == selectedAction);
            display.drawMenuItem(actions[i], i, 3, isSelected, startY);
        }

        // Show navigation hint
        display.setTextSize(1);
        if (selectedAction == ActionType::DETAILS) {
            display.printCentered("Click to select, Long press for details", 50);
        } else {
            display.printCentered("Click to select, Long press to execute", 50);
        }
    }

    void executeAction() {
        switch (selectedAction) {
            case ActionType::ON:
                if (device.onCommand.isValid()) {
                    LOG_INFO("Sending ON command for device %d", device.id);
                    irManager.sendProtocol(device.onCommand);
                    // Show feedback
                    display.clear();
                    display.printCentered("Sending ON...", 30);
                    display.update();
                    delay(1000);
                } else {
                    LOG_ERROR("Invalid ON command for device %d", device.id);
                    display.clear();
                    display.printCentered("Invalid ON command", 30);
                    display.update();
                    delay(2000);
                }
                break;

            case ActionType::OFF:
                if (device.offCommand.isValid()) {
                    LOG_INFO("Sending OFF command for device %d", device.id);
                    irManager.sendProtocol(device.offCommand);
                    // Show feedback
                    display.clear();
                    display.printCentered("Sending OFF...", 30);
                    display.update();
                    delay(1000);
                } else {
                    LOG_ERROR("Invalid OFF command for device %d", device.id);
                    display.clear();
                    display.printCentered("Invalid OFF command", 30);
                    display.update();
                    delay(2000);
                }
                break;

            case ActionType::DETAILS:
                // This should not be called, but handle it gracefully
                router.push(new DeviceDetails(device));
                break;
        }
    }

    void onExit() override { LOG_DEBUG("DeviceActions onExit"); }
};
