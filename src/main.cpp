#include <Arduino.h>
#include "config.h"
#include <Speaker.h>

// Create speaker instance
Speaker speaker(SPEAKER_PIN);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting up...");
  
  // Initialize the speaker
  speaker.begin();
  
  // Test the speaker with a startup beep
  speaker.shortBeep();
}

void loop() {
  // put your main code here, to run repeatedly:
}