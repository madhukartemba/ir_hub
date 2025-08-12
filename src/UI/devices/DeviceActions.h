#include <Arduino.h>
#include "../../global/Global.h"
#include "DeviceDeleteConfirmation.h"
#include "DeviceDetails.h"

class DeviceActions : public Screen {
   private:
    enum class ActionType { ON, OFF, DETAILS, REMOVE, BACK };

    Device device;
    ActionType selectedAction;
    int scrollOffset;

   public:
    DeviceActions(Device& device)
        : device(device), selectedAction(ActionType::ON), scrollOffset(0) {}

    void onEnter() override {
        LOG_DEBUG("DeviceActions onEnter for device %d", device.id);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceActions onButtonClick");
            // Navigate through actions: ON, OFF, Details, Remove, Back
            switch (selectedAction) {
                case ActionType::ON:
                    selectedAction = ActionType::OFF;
                    break;
                case ActionType::OFF:
                    selectedAction = ActionType::DETAILS;
                    break;
                case ActionType::DETAILS:
                    selectedAction = ActionType::REMOVE;
                    break;
                case ActionType::REMOVE:
                    selectedAction = ActionType::BACK;
                    break;
                case ActionType::BACK:
                    selectedAction = ActionType::ON;
                    break;
            }
            updateScrollOffset();
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("DeviceActions onButtonLongPress");
            switch (selectedAction) {
                case ActionType::ON:
                case ActionType::OFF:
                    // Execute ON or OFF action
                    executeAction();
                    break;
                case ActionType::DETAILS:
                    // Navigate to details
                    router.push(new DeviceDetails(device));
                    break;
                case ActionType::REMOVE:
                    // Navigate to delete confirmation
                    router.push(new DeviceDeleteConfirmation(device));
                    break;
                case ActionType::BACK:
                    // Go back to devices list
                    router.pop();
                    break;
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

        // Show action options with scrolling
        int startY = 20;
        const char* actions[] = {"Send ON", "Send OFF", "Details", "Delete Device", "Back"};
        const int totalActions = 5;
        const int visibleActions = 3;

        for (int i = 0; i < visibleActions; i++) {
            int actionIndex = scrollOffset + i;
            if (actionIndex < totalActions) {
                bool isSelected = (static_cast<ActionType>(actionIndex) == selectedAction);
                display.drawMenuItem(actions[actionIndex], i, visibleActions, isSelected, startY);
            }
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
            default:
                LOG_ERROR("Unhandled action type in executeAction: %d",
                          static_cast<int>(selectedAction));
        }
    }

    void updateScrollOffset() {
        // Calculate scroll offset based on selected action
        int visibleActions = 3;  // Number of actions that can be displayed at once

        if (static_cast<int>(selectedAction) >= scrollOffset + visibleActions) {
            scrollOffset = static_cast<int>(selectedAction) - visibleActions + 1;
        } else if (static_cast<int>(selectedAction) < scrollOffset) {
            scrollOffset = static_cast<int>(selectedAction);
        }
    }

    void onExit() override { LOG_DEBUG("DeviceActions onExit"); }
};
