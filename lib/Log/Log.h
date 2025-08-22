#pragma once

// Set to 0 to disable all logs globally
#define LOGGING_ENABLED 1

// Log levels - set the minimum level to display
// DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3

// Set the minimum log level to display (change this to filter logs)
#define MIN_LOG_LEVEL LOG_LEVEL_DEBUG

// Class name registration - use this at the top of your class
#define LOG_REGISTER_CLASS(name) static const char* LOG_CLASS_NAME __attribute__((unused)) = name;

// Default class name for when not registered
#define LOG_DEFAULT_CLASS_NAME "Unknown"

// Get current class name (will be "Unknown" if not registered)
// Fixed version that handles undefined LOG_CLASS_NAME
#ifdef LOG_CLASS_NAME
#    define LOG_GET_CLASS_NAME() LOG_CLASS_NAME
#else
#    define LOG_GET_CLASS_NAME() LOG_DEFAULT_CLASS_NAME
#endif

// Enhanced logging macros with automatic class name detection
#if LOGGING_ENABLED
// Debug level logging
#    if MIN_LOG_LEVEL <= LOG_LEVEL_DEBUG
#        define LOG_DEBUG(fmt, ...)                                                                \
            Serial.printf((String("[DEBUG][") + LOG_GET_CLASS_NAME() + "] " + fmt + "\n").c_str(), \
                          ##__VA_ARGS__)
#    else
#        define LOG_DEBUG(fmt, ...)
#    endif

// Info level logging
#    if MIN_LOG_LEVEL <= LOG_LEVEL_INFO
#        define LOG_INFO(fmt, ...)                                                                \
            Serial.printf((String("[INFO][") + LOG_GET_CLASS_NAME() + "] " + fmt + "\n").c_str(), \
                          ##__VA_ARGS__)
#    else
#        define LOG_INFO(fmt, ...)
#    endif

// Warning level logging
#    if MIN_LOG_LEVEL <= LOG_LEVEL_WARN
#        define LOG_WARN(fmt, ...)                                                                \
            Serial.printf((String("[WARN][") + LOG_GET_CLASS_NAME() + "] " + fmt + "\n").c_str(), \
                          ##__VA_ARGS__)
#    else
#        define LOG_WARN(fmt, ...)
#    endif

// Error level logging
#    if MIN_LOG_LEVEL <= LOG_LEVEL_ERROR
#        define LOG_ERROR(fmt, ...)                                                                \
            Serial.printf((String("[ERROR][") + LOG_GET_CLASS_NAME() + "] " + fmt + "\n").c_str(), \
                          ##__VA_ARGS__)
#    else
#        define LOG_ERROR(fmt, ...)
#    endif

// Legacy macros for backward compatibility
#    define LOG(msg) LOG_INFO(msg)
#    define LOGF(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
#    define LOG_LINE() Serial.println(F("-------------"))
#else
// When logging is disabled, all macros do nothing
#    define LOG_DEBUG(fmt, ...)
#    define LOG_INFO(fmt, ...)
#    define LOG_WARN(fmt, ...)
#    define LOG_ERROR(fmt, ...)
#    define LOG(msg)
#    define LOG_LINE()
#endif