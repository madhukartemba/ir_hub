# Utils Directory

This directory contains utility classes and functions that can be reused across different parts of the application.

## MenuUtils

The `MenuUtils` class provides a reusable scrollable menu implementation that can be used across different screens in the application.

### Features

- **Scrollable Menus**: Automatically handles scrolling when there are more menu items than can fit on screen
- **Multiple Input Types**: Supports different input formats (vector of strings, array of const char\*, etc.)
- **Consistent UI**: Provides a consistent look and feel across all menus
- **Easy Integration**: Simple to integrate into existing screens

### Usage

```cpp
#include "../utils/MenuUtils.h"

// For const char* arrays
const char* menuItems[] = {"Option 1", "Option 2", "Option 3", "Back"};
MenuUtils::drawScrollableMenu(menuItems, 4, selectedIndex, 3, 20);

// For vector of strings
std::vector<std::string> items = {"Option 1", "Option 2", "Option 3"};
MenuUtils::drawScrollableMenu(items, selectedIndex, 3, 20);
```

### Parameters

- `items`: Array or vector of menu items
- `selectedIndex`: Currently selected item index
- `maxVisibleItems`: Maximum number of items to show at once (default: 3)
- `startY`: Y-coordinate to start drawing the menu (default: 20)

### Integration

To use this utility in a new screen:

1. Include the header: `#include "../utils/MenuUtils.h"`
2. Add a `selectedIndex` member variable to track selection
3. Update the button click handler to increment `selectedIndex`
4. Replace manual menu drawing with `MenuUtils::drawScrollableMenu()`

### Examples

See the following files for examples of how to use MenuUtils:

- `src/ui/settings/Settings.h`
- `src/ui/devices/Devices.h`
- `src/ui/devices/DeviceActions.h`
- `src/ui/MainMenu.h`
