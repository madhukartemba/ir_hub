#include <Arduino.h>
#include "../../global/Global.h"
#include "../../utils/MenuUtils.h"
#include "DeviceDeleteConfirmation.h"
#include "DeviceDetails.h"

class DeviceActions : public Screen {
   private:
    enum class ActionType { ON, OFF, DETAILS, REMOVE, BACK };

    Device device;
    ActionType selectedAction;
    int selectedIndex;
    const char* actions[5] = {"Send ON", "Send OFF", "Details", "Delete Device", "Back"};

   public:
    DeviceActions(Device& device)
        : device(device), selectedAction(ActionType::ON), selectedIndex(0) {}

    void onEnter() override {
        LOG_DEBUG("DeviceActions onEnter for device %d", device.id);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceActions onButtonClick");
            // Navigate through actions
            selectedIndex = (selectedIndex + 1) % 5;
            selectedAction = static_cast<ActionType>(selectedIndex);
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

        // Use the scrollable menu utility
        MenuUtils::drawScrollableMenu(actions, 5, selectedIndex, 3, 20);
    }

    void executeAction() {
        switch (selectedAction) {
            case ActionType::ON:
                if (device.onCommand.isValid()) {
                    LOG_INFO("Sending ON command for device %d", device.id);
                    irManager.sendProtocol(device.onCommand);
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

    void onExit() override { LOG_DEBUG("DeviceActions onExit"); }
};
