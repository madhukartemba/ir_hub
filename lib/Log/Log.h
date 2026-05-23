#pragma once

#include <Arduino.h>

// Logging: stack-only formatting (no heap allocs) + compile-time level gate.
// Override the level with -DMIN_LOG_LEVEL=LOG_LEVEL_DEBUG from build flags.

// Set to 0 to disable all logs globally
#ifndef LOGGING_ENABLED
#  define LOGGING_ENABLED 1
#endif

// Log levels - set the minimum level to display
// DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

#ifndef MIN_LOG_LEVEL
#  define MIN_LOG_LEVEL LOG_LEVEL_INFO
#endif

#if LOGGING_ENABLED

namespace ir_hub_log {

// 192 byte stack buffer covers our longest lines (MQTT topics + state).
// Over-long lines truncate rather than allocating.
inline void emit(const char* level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

inline void emit(const char* level, const char* fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(level);
    if (n < 0) {
        Serial.println(F("<log fmt error>"));
        return;
    }
    Serial.write(reinterpret_cast<const uint8_t*>(buf),
                 (size_t)n < sizeof(buf) - 1 ? (size_t)n : sizeof(buf) - 1);
    Serial.print('\n');
}

}  // namespace ir_hub_log

#  if MIN_LOG_LEVEL <= LOG_LEVEL_DEBUG
#    define LOG_DEBUG(fmt, ...) ::ir_hub_log::emit("[DEBUG] ", fmt, ##__VA_ARGS__)
#  else
#    define LOG_DEBUG(...) ((void)0)
#  endif

#  if MIN_LOG_LEVEL <= LOG_LEVEL_INFO
#    define LOG_INFO(fmt, ...)  ::ir_hub_log::emit("[INFO] ",  fmt, ##__VA_ARGS__)
#  else
#    define LOG_INFO(...) ((void)0)
#  endif

#  if MIN_LOG_LEVEL <= LOG_LEVEL_WARN
#    define LOG_WARN(fmt, ...)  ::ir_hub_log::emit("[WARN] ",  fmt, ##__VA_ARGS__)
#  else
#    define LOG_WARN(...) ((void)0)
#  endif

#  if MIN_LOG_LEVEL <= LOG_LEVEL_ERROR
#    define LOG_ERROR(fmt, ...) ::ir_hub_log::emit("[ERROR] ", fmt, ##__VA_ARGS__)
#  else
#    define LOG_ERROR(...) ((void)0)
#  endif

// Legacy macros for backward compatibility
#  define LOG(msg) LOG_INFO(msg)
#  define LOGF(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
#  define LOG_LINE() Serial.println(F("-------------"))

#else  // LOGGING_ENABLED == 0

#  define LOG_DEBUG(...) ((void)0)
#  define LOG_INFO(...)  ((void)0)
#  define LOG_WARN(...)  ((void)0)
#  define LOG_ERROR(...) ((void)0)
#  define LOG(msg)       ((void)0)
#  define LOGF(...)      ((void)0)
#  define LOG_LINE()     ((void)0)

#endif  // LOGGING_ENABLED
