#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <Color.h>

// IR Code Colors
#define SEND_ON_COMMAND_COLOR Color::Green
#define SEND_OFF_COMMAND_COLOR Color::Red

// Success/Positive States
#define COLOR_SUCCESS Color::Green
#define COLOR_SUCCESS_ALT Color::LightGreen
#define COLOR_SUCCESS_DARK Color::DarkGreen

// Warning/Neutral States
#define COLOR_WARNING Color::Orange
#define COLOR_WARNING_DARK Color::DarkOrange
#define COLOR_SETTINGS Color::Yellow

// Error/Danger States
#define COLOR_ERROR Color::Red
#define COLOR_ERROR_DARK Color::DarkRed

// Home Screen Colors
#define COLOR_HOME_SCREEN_WIFI_CONNECTED Color::White
#define COLOR_HOME_SCREEN_WIFI_DISCONNECTED Color::Red

// Wi-Fi
#define COLOR_WIFI_INIT Color::Blue

// Information/Status States
#define COLOR_INFO Color::Blue
#define COLOR_INFO_DARK Color::DarkBlue
#define COLOR_INFO_LIGHT Color::CornflowerBlue
#define COLOR_INFO_ROYAL Color::RoyalBlue

// Default/Neutral States
#define COLOR_DEFAULT Color::White
#define COLOR_OFF Color::Black

// Timeout duration
#define TIMEOUT_DURATION 20000

#endif  // PREFERENCES_H