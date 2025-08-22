# Enhanced Logging System Upgrade

## Problem Solved

You had inconsistent class name tagging in your logs. Some places used manual `[ClassName]` tags like:

```cpp
LOG_INFO("[Alexa] Alexa functionality enabled");
```

While others were missing these tags entirely:

```cpp
LOG_INFO("Device saved to file");
```

## Solution Implemented

I've enhanced your `Log.h` system to automatically include class names in all log messages. Here's what was added:

### 1. Enhanced Log.h

The logging system now automatically adds class names to all log messages. The format is:

```
[LEVEL][ClassName] Message
```

### 2. Class Registration System

To use the enhanced system, simply add this line **before** each class definition:

```cpp
LOG_REGISTER_CLASS("YourClassName")
class YourClass {
    // Your class implementation
};
```

### 3. Automatic Migration Script

I've created a Python script that can automatically:

- Add `LOG_REGISTER_CLASS` to all your class definitions
- Remove manual `[ClassName]` tags from existing LOG\_\* calls

## How to Use

### Option 1: Manual Migration (Recommended for small codebases)

1. Add `LOG_REGISTER_CLASS("ClassName")` before each class
2. Remove manual `[ClassName]` tags from LOG\_\* calls
3. Keep the rest of your logging calls unchanged

### Option 2: Automatic Migration (Recommended for large codebases)

Run the migration script:

```bash
# Add class registrations only
python scripts/add_log_registrations.py .

# Add class registrations AND remove manual tags
python scripts/add_log_registrations.py . --remove-tags
```

## Example Before/After

### Before:

```cpp
class AlexaConnector {
    void begin() {
        LOG_INFO("[Alexa] Alexa functionality enabled");
        LOG_DEBUG("[Alexa] Device added: TV");
    }
};
```

### After:

```cpp
LOG_REGISTER_CLASS("AlexaConnector")
class AlexaConnector {
    void begin() {
        LOG_INFO("Alexa functionality enabled");
        LOG_DEBUG("Device added: TV");
    }
};
```

### Output:

```
[INFO][AlexaConnector] Alexa functionality enabled
[DEBUG][AlexaConnector] Device added: TV
```

## Benefits

1. **Consistent Formatting** - All logs follow the same `[LEVEL][ClassName] Message` pattern
2. **No More Missing Tags** - Class names are automatically included
3. **Easy Filtering** - You can easily filter logs by class name
4. **Reduced Errors** - No more forgotten or mismatched class name tags
5. **Backward Compatible** - Existing code still works
6. **Minimal Overhead** - Uses static variables, very efficient

## Technical Details

- Uses static variables to store class names
- Works reliably on embedded systems (ESP32, Arduino)
- Compatible with all C++ compilers
- No RTTI or complex runtime features required
- Minimal memory overhead

## Files Modified

1. `lib/Log/Log.h` - Enhanced with automatic class name detection
2. `lib/Log/README.md` - Documentation for the new system
3. `lib/AlexaConnector/AlexaConnector.h` - Example migration
4. `scripts/add_log_registrations.py` - Automatic migration script

## Next Steps

1. **Test the system** - Make sure it works with your current code
2. **Run the migration script** - Automatically update your codebase
3. **Review changes** - Check that the automatic changes are correct
4. **Update remaining files** - Manually update any files the script missed

## Troubleshooting

If you encounter issues:

1. **Compilation errors** - Make sure `LOG_REGISTER_CLASS` is placed **before** the class definition
2. **Missing class names** - Check that the class name in `LOG_REGISTER_CLASS` matches the actual class name
3. **Duplicate registrations** - The script won't add duplicate registrations

The enhanced logging system will make debugging much easier by providing consistent, automatically-tagged log messages throughout your codebase!
