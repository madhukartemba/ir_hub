# Enhanced Logging System

This enhanced logging system automatically adds class names to log messages, making debugging much easier.

## How to Use

### 1. Register Your Class

Add this line **before** your class definition:

```cpp
LOG_REGISTER_CLASS("YourClassName")
class YourClass {
    // Your class implementation
};
```

### 2. Use Logging Macros

Simply use the logging macros as before, but without manual class name tags:

```cpp
// Instead of:
LOG_INFO("[MyClass] Some message");

// Just use:
LOG_INFO("Some message");
```

The class name will be automatically added to the output.

### 3. Example Output

With the enhanced system, your logs will look like:

```
[INFO][AlexaConnector] Alexa functionality enabled
[DEBUG][DeviceManager] Device added: TV (ID: 1)
[ERROR][IRManager] Invalid command received
[WARN][Router] Navigation blocked
```

## Available Macros

- `LOG_DEBUG(fmt, ...)` - Debug level messages
- `LOG_INFO(fmt, ...)` - Info level messages
- `LOG_WARN(fmt, ...)` - Warning level messages
- `LOG_ERROR(fmt, ...)` - Error level messages

## Migration Guide

To migrate existing code:

1. Add `LOG_REGISTER_CLASS("ClassName")` before each class
2. Remove manual `[ClassName]` tags from all LOG\_\* calls
3. Keep the rest of your logging calls unchanged

## Benefits

- **Automatic class identification** - No need to manually add class names
- **Consistent formatting** - All logs follow the same pattern
- **Easy filtering** - You can easily filter logs by class name
- **Reduced errors** - No more forgotten or mismatched class name tags
- **Backward compatible** - Existing code still works

## Class Name Registration

The `LOG_REGISTER_CLASS` macro creates a static variable that stores the class name. This approach:

- Works reliably on embedded systems
- Has minimal memory overhead
- Is compatible with all C++ compilers
- Doesn't require RTTI or complex runtime features
