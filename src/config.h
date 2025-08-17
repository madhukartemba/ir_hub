#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions
#define TOUCH_BUTTON_PIN D0
#define OLED_SCL_PIN D1
#define OLED_SDA_PIN D2
#define SPEAKER_PIN D3
#define IR_RX_PIN D5
#define IR_TX_PIN D6
#define NEOPIXEL_PIN D7

// LED configuration
#define NUM_LEDS 13  // Number of LEDs in the circular strip
#define CENTER_LED 8

// WiFi configuration
#define WIFI_AP_NAME "IRHub Setup"
#define WIFI_AP_TIMEOUT 60       // 1 minute timeout
#define WIFI_CONNECT_TIMEOUT 30  // 30 seconds timeout

#endif  // CONFIG_H
