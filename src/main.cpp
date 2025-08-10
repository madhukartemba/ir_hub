#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "global/Global.h"
#include "ui/MainMenu.h"

void setup() {
    Serial.begin(9600);

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        LOG_ERROR("Failed to mount LittleFS");
        while (1) {
            delay(100);
        }
    }

    // Initialize IdGen
    idGen.begin();

    // Initialize display
    display.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Initializing...", 40);
    display.update();

    delay(3000);

    LOG_DEBUG("Starting speaker setup");
    speaker.begin(SPEAKER_PIN);
    LOG_DEBUG("Speaker initialized");

    // Initialize button
    LOG_DEBUG("Starting button setup");
    button.begin(TOUCH_BUTTON_PIN, INPUT);
    button.setSpeaker(speaker);
    LOG_DEBUG("Button initialized on pin");
    // Set up button callbacks if needed

    LOG_DEBUG("Starting LED ring setup");
    ring.begin(NEOPIXEL_PIN, NUM_LEDS);
    ring.setCenterLed(CENTER_LED);
    LOG_DEBUG("LED ring initialized on pin");

    // Initialize the global router
    router.setDefaultScreen(new MainMenu());  // Replace with your actual default screen object

    // Show ready message on display
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Ready!", 40);
    display.update();
    delay(500);

    Serial.println("IR Hub: System Ready");
}

void loop() {
    router.update();  // Main app logic now handled by router
    button.update();
}
