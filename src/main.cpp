#include <Arduino.h>
#include "config.h"
#include <Speaker.h>

// Create speaker instance
Speaker speaker(SPEAKER_PIN);

// Touch button state variables
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // 50ms debounce time

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting up...");
  
  // Initialize the speaker
  speaker.begin();
  
  // Initialize touch button pin
  pinMode(TOUCH_BUTTON_PIN, INPUT_PULLUP);
  
  // Test the speaker with a startup beep
  speaker.shortBeep();
  Serial.println("Touch button test ready - press the button to beep!");
}

void loop() {
  // put your main code here, to run repeatedly:
  
  // Read the current button state
  bool reading = digitalRead(TOUCH_BUTTON_PIN);
  
  // Check if the button state has changed
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // If enough time has passed since the last change, check if the button was pressed
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed and is now LOW (pressed)
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      Serial.println("Button pressed - BEEP!");
      speaker.beep(200); // Beep for 200ms
    }
    // If the button is released
    else if (reading == HIGH && buttonPressed) {
      buttonPressed = false;
      Serial.println("Button released");
    }
  }
  
  // Update the last button state
  lastButtonState = reading;
  
  // Small delay to prevent overwhelming the serial output
  delay(10);
}