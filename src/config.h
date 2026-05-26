#ifndef CONFIG_H
#define CONFIG_H

#ifdef USE_VERSION_0
#    define DISPLAY_TYPE DisplayType::SSD1306
#    define DISPLAY_FLIPPED true

#    define TOUCH_BUTTON_PIN D0
#    define OLED_SCL_PIN D1
#    define OLED_SDA_PIN D2
#    define SPEAKER_PIN D3
#    define IR_RX_PIN D5
#    define IR_TX_PIN D6
#    define NEOPIXEL_PIN D7

#    define NUM_LEDS 13  // Number of LEDs in the circular strip
#    define DISPLAY_DRIVER DisplayDriver::FASTLED

#    define WIFI_AP_NAME "IRHub V0 Setup"
#    define WIFI_AP_TIMEOUT 180  // 3 minutes timeout

#endif  // USE_VERSION_0

#ifdef USE_VERSION_1

#    define DISPLAY_TYPE DisplayType::SH1106
#    define DISPLAY_FLIPPED false

#    define TOUCH_BUTTON_PIN D0
#    define OLED_SCL_PIN D1
#    define OLED_SDA_PIN D2
#    define SPEAKER_PIN D3
#    define IR_RX_PIN D5
#    define IR_TX_PIN D6
#    define NEOPIXEL_PIN D7

#    define NUM_LEDS 27  // Number of LEDs in the circular strip
#    define DISPLAY_DRIVER DisplayDriver::NEOPIXEL

#    define WIFI_AP_NAME "IRHub V1 Setup"
#    define WIFI_AP_TIMEOUT 180  // 3 minutes timeout

#endif  // USE_VERSION_1

#ifdef USE_VERSION_3

#    define DISPLAY_TYPE DisplayType::SH1106
#    define DISPLAY_FLIPPED false

#    define TOUCH_BUTTON_PIN D0
#    define OLED_SCL_PIN D1
#    define OLED_SDA_PIN D2
#    define SPEAKER_PIN D3
#    define IR_RX_PIN D5
#    define IR_TX_PIN D6
#    define NEOPIXEL_PIN D7

#    define NUM_LEDS 30  // Number of LEDs in the circular strip
#    define DISPLAY_DRIVER DisplayDriver::NEOPIXEL

#    define WIFI_AP_NAME "IRHub V3 Setup"
#    define WIFI_AP_TIMEOUT 180  // 3 minutes timeout

#endif  // USE_VERSION_3

#endif  // CONFIG_H
