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
        ring.breathe(5, COLOR_SUCCESS_DARK);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonClick");
            if (currentPage == 1 || currentPage == 2) {
                // On description pages, navigate through items
                if (!descriptionItems.empty()) {
                    selectedDescriptionItem =
                        (selectedDescriptionItem + 1) % descriptionItems.size();
                }
            } else {
                // On device details page, do nothing (or could be used for other purposes)
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("DeviceDetails onButtonLongPress");
            if (currentPage == 2) {
                // On last page (OFF command), exit
                router.pop();
            } else {
                // Go to next page
                currentPage = (currentPage + 1) % 3;
                // Reset description items for new page
                selectedDescriptionItem = 0;
                descriptionItems.clear();
            }
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
            LOG_DEBUG("On command description: %s", description.c_str());
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
        ring.off();
        selectedDescriptionItem = 0;
        descriptionItems.clear();
    }
};
