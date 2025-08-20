#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <FastLED.h>

// IR Code Colors
#define SEND_ON_COMMAND_COLOR CRGB::Green
#define SEND_OFF_COMMAND_COLOR CRGB::OrangeRed

// Success/Positive States
#define COLOR_SUCCESS CRGB::Green
#define COLOR_SUCCESS_ALT CRGB::LightGreen
#define COLOR_SUCCESS_DARK CRGB::DarkGreen

// Warning/Neutral States
#define COLOR_WARNING CRGB::Orange
#define COLOR_WARNING_DARK CRGB::DarkOrange
#define COLOR_SETTINGS CRGB::Yellow

// Error/Danger States
#define COLOR_ERROR CRGB::Red
#define COLOR_ERROR_DARK CRGB::DarkRed

// Home Screen Colors
#define COLOR_HOME_SCREEN_WIFI_CONNECTED CRGB::White
#define COLOR_HOME_SCREEN_WIFI_DISCONNECTED CRGB::Red

// Information/Status States
#define COLOR_INFO CRGB::Blue
#define COLOR_INFO_DARK CRGB::DarkBlue
#define COLOR_INFO_LIGHT CRGB::CornflowerBlue
#define COLOR_INFO_ROYAL CRGB::RoyalBlue

// Default/Neutral States
#define COLOR_DEFAULT CRGB::White
#define COLOR_OFF CRGB::Black

// Legacy definitions for backward compatibility
#define ON_COMMAND_LED_COLOR COLOR_SUCCESS
#define OFF_COMMAND_LED_COLOR COLOR_WARNING_DARK

// Timeout duration
#define TIMEOUT_DURATION 20000

#endif  // PREFERENCES_H