#ifndef MENU_UTILS_H
#define MENU_UTILS_H

#include <string>
#include <vector>
#include "../global/Global.h"

class MenuUtils {
   public:
    struct MenuItem {
        std::string text;
        int id;

        MenuItem(const std::string& t, int i) : text(t), id(i) {}
    };

    static void drawScrollableMenu(const std::vector<MenuItem>& items, int selectedIndex,
                                   int maxVisibleItems = 3, int startY = 20) {
        if (items.empty()) return;

        int totalItems = items.size();
        int visibleItems = std::min(maxVisibleItems, totalItems);

        // Calculate scroll offset to keep selected item visible
        int scrollOffset = 0;
        if (selectedIndex >= scrollOffset + visibleItems) {
            scrollOffset = selectedIndex - visibleItems + 1;
        } else if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        }

        // Ensure scroll offset doesn't go negative
        scrollOffset = std::max(0, scrollOffset);

        // Draw visible items
        for (int i = 0; i < visibleItems; i++) {
            int itemIndex = scrollOffset + i;
            if (itemIndex < totalItems) {
                bool isSelected = (itemIndex == selectedIndex);
                display.drawMenuItem(items[itemIndex].text.c_str(), i, visibleItems, isSelected,
                                     startY);
            }
        }
    }

    static void drawScrollableMenu(const std::vector<std::string>& items, int selectedIndex,
                                   int maxVisibleItems = 3, int startY = 20) {
        if (items.empty()) return;

        int totalItems = items.size();
        int visibleItems = std::min(maxVisibleItems, totalItems);

        // Calculate scroll offset to keep selected item visible
        int scrollOffset = 0;
        if (selectedIndex >= scrollOffset + visibleItems) {
            scrollOffset = selectedIndex - visibleItems + 1;
        } else if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        }

        // Ensure scroll offset doesn't go negative
        scrollOffset = std::max(0, scrollOffset);

        // Draw visible items
        for (int i = 0; i < visibleItems; i++) {
            int itemIndex = scrollOffset + i;
            if (itemIndex < totalItems) {
                bool isSelected = (itemIndex == selectedIndex);
                display.drawMenuItem(items[itemIndex].c_str(), i, visibleItems, isSelected, startY);
            }
        }
    }

    static void drawScrollableMenu(const char* items[], int totalItems, int selectedIndex,
                                   int maxVisibleItems = 3, int startY = 20) {
        if (totalItems <= 0) return;

        int visibleItems = std::min(maxVisibleItems, totalItems);

        // Calculate scroll offset to keep selected item visible
        int scrollOffset = 0;
        if (selectedIndex >= scrollOffset + visibleItems) {
            scrollOffset = selectedIndex - visibleItems + 1;
        } else if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        }

        // Ensure scroll offset doesn't go negative
        scrollOffset = std::max(0, scrollOffset);

        // Draw visible items
        for (int i = 0; i < visibleItems; i++) {
            int itemIndex = scrollOffset + i;
            if (itemIndex < totalItems) {
                bool isSelected = (itemIndex == selectedIndex);
                display.drawMenuItem(items[itemIndex], i, visibleItems, isSelected, startY);
            }
        }
    }
};

#endif  // MENU_UTILS_H
