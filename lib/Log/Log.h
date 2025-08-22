#pragma once

// Set to 0 to disable all logs globally
#define LOGGING_ENABLED 1

// Log levels - set the minimum level to display
// DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

// Set the minimum log level to display (change this to filter logs)
#define MIN_LOG_LEVEL LOG_LEVEL_DEBUG

#if LOGGING_ENABLED
  // Debug level logging
  #if MIN_LOG_LEVEL <= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...) Serial.printf((String("[DEBUG] ") + fmt + "\n").c_str(), ##__VA_ARGS__)
  #else
    #define LOG_DEBUG(msg)
  #endif

  // Info level logging
  #if MIN_LOG_LEVEL <= LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...) Serial.printf((String("[INFO] ") + fmt + "\n").c_str(), ##__VA_ARGS__)
  #else
    #define LOG_INFO(msg)
  #endif

  // Warning level logging
  #if MIN_LOG_LEVEL <= LOG_LEVEL_WARN
    #define LOG_WARN(fmt, ...) Serial.printf((String("[WARN] ") + fmt + "\n").c_str(), ##__VA_ARGS__)
  #else
    #define LOG_WARN(msg)
  #endif

  // Error level logging
  #if MIN_LOG_LEVEL <= LOG_LEVEL_ERROR
    #define LOG_ERROR(fmt, ...) Serial.printf((String("[ERROR] ") + fmt + "\n").c_str(), ##__VA_ARGS__)
  #else
    #define LOG_ERROR(msg)
  #endif

  // Legacy macros for backward compatibility
  #define LOG(msg) LOG_INFO(msg)
  #define LOGF(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
  #define LOG_LINE() Serial.println(F("-------------"))
#else
  // When logging is disabled, all macros do nothing
  #define LOG_DEBUG(msg)
  #define LOG_INFO(msg)
  #define LOG_WARN(msg)
  #define LOG_ERROR(msg)
  #define LOG(msg)
  #define LOG_LINE()
#endif
