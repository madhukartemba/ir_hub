#include <Arduino.h>
#include "../../global/Global.h"
#include "../../preferences.h"
#include "../../utils/MenuUtils.h"

class DeviceDetails : public Screen {
   private:
    Device device;
    int currentPage;  // 0 = device details, 1 = on command description, 2 = off command description
    std::vector<std::string> descriptionItems;
    int selectedDescriptionItem;

   public:
    DeviceDetails(Device& device) : device(device), currentPage(0), selectedDescriptionItem(0) {}

    void onEnter() override {
        LOG_DEBUG("DeviceDetails onEnter for device %d", device.id);
        ledRing.breathe(COLOR_SUCCESS_DARK);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonClick");
            if (currentPage == 0) {
                // On device details page, go to page 1
                currentPage = 1;
                selectedDescriptionItem = 0;
                descriptionItems.clear();
            } else if (currentPage == 1) {
                // On page 1, navigate through items, then go to page 2
                if (!descriptionItems.empty()) {
                    selectedDescriptionItem =
                        (selectedDescriptionItem + 1) % descriptionItems.size();
                    // If we've cycled through all items, go to page 2
                    if (selectedDescriptionItem == 0) {
                        currentPage = 2;
                        selectedDescriptionItem = 0;
                        descriptionItems.clear();
                    }
                } else {
                    // No items to cycle through, go directly to page 2
                    currentPage = 2;
                    selectedDescriptionItem = 0;
                    descriptionItems.clear();
                }
            } else if (currentPage == 2) {
                // On page 2, navigate through items, then go back to page 0
                if (!descriptionItems.empty()) {
                    selectedDescriptionItem =
                        (selectedDescriptionItem + 1) % descriptionItems.size();
                    // If we've cycled through all items, go back to page 0
                    if (selectedDescriptionItem == 0) {
                        currentPage = 0;
                        selectedDescriptionItem = 0;
                        descriptionItems.clear();
                    }
                } else {
                    // No items to cycle through, go directly to page 0
                    currentPage = 0;
                    selectedDescriptionItem = 0;
                    descriptionItems.clear();
                }
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonLongPress");
            // Exit from anywhere
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

    void parseDescription(const String& description) {
        descriptionItems.clear();
        if (description.isEmpty()) {
            return;
        }

        // Split by comma and trim whitespace
        int startPos = 0;
        int commaPos = description.indexOf(',');

        while (commaPos != -1) {
            String item = description.substring(startPos, commaPos);
            item.trim();  // Remove leading/trailing whitespace
            if (!item.isEmpty()) {
                descriptionItems.push_back(item.c_str());
            }
            startPos = commaPos + 1;
            commaPos = description.indexOf(',', startPos);
        }

        // Add the last item (after the last comma)
        if (startPos < (int)description.length()) {
            String item = description.substring(startPos);
            item.trim();
            if (!item.isEmpty()) {
                descriptionItems.push_back(item.c_str());
            }
        }
    }

    void displayOnCommandDescription() {
        // Show title
        display.setTextSize(1);
        display.printCentered("ON Command", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Always show description items in scrollable list
        if (descriptionItems.empty()) {
            String description = device.onCommand.getDescription();
            if (!description.isEmpty()) {
                parseDescription(description);
            }
        }

        if (descriptionItems.empty()) {
            display.printCentered("No description", 30);
        } else {
            MenuUtils::drawScrollableMenu(descriptionItems, selectedDescriptionItem, 3, 20);
        }
    }

    void displayOffCommandDescription() {
        // Show title
        display.setTextSize(1);
        display.printCentered("OFF Command", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Always show description items in scrollable list
        if (descriptionItems.empty()) {
            String description = device.offCommand.getDescription();
            LOG_DEBUG("Off command description: %s", description.c_str());
            if (!description.isEmpty()) {
                parseDescription(description);
            }
        }

        if (descriptionItems.empty()) {
            display.printCentered("No description", 30);
        } else {
            MenuUtils::drawScrollableMenu(descriptionItems, selectedDescriptionItem, 3, 20);
        }
    }

    void onExit() override {
        LOG_DEBUG("DeviceDetails onExit");
        selectedDescriptionItem = 0;
        descriptionItems.clear();
    }
};
