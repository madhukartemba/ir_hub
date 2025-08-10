#include <Arduino.h>
#include <ArduinoJson.h>
#include "../../../lib/IRManager/IRManager.h"
#include "../../config.h"
#include "../../global/Global.h"

class TestMenu : public Screen {
   private:
    enum class State {
        IR_RECORD,
        IR_SEND,
        IR_STATUS,
    };

    State currentState;
    IRManager irManager;
    bool isRecording = false;
    IRData recordedData;
    bool hasRecordedData = false;
    JsonDocument tempJsonDoc;  // Temporary JSON document for testing
    bool hasJsonData = false;

   public:
    void onEnter() override {
        LOG_DEBUG("TestMenu onEnter");
        currentState = State::IR_RECORD;

        // Initialize IR Manager
        irManager.begin(IR_RECEIVER_PIN, IR_EMITTER_PIN, idGen);

        // Change button behavior
        button.setClickCallback([this]() {
            LOG_DEBUG("TestMenu onButtonClick");
            // Switch to next state using mod operator
            currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 3);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            LOG_DEBUG("TestMenu onButtonLongPress");
            if (currentState == State::IR_RECORD) {
                if (!isRecording) {
                    // Start recording
                    irManager.startRecording();
                    isRecording = true;
                    LOG_DEBUG("Started IR recording");
                } else {
                    // Stop recording and check for data
                    if (irManager.record()) {
                        if (irManager.isRecordedDataValid()) {
                            recordedData = irManager.getRecordedData();
                            hasRecordedData = true;
                            LOG_DEBUG("IR data recorded successfully");

                            // Test JSON serialization
                            tempJsonDoc = irManager.exportRecordedDataToJson();
                            if (!tempJsonDoc.isNull()) {
                                hasJsonData = true;
                                LOG_DEBUG("JSON serialization successful");
                            } else {
                                LOG_DEBUG("JSON serialization failed");
                            }
                        } else {
                            LOG_DEBUG("Invalid IR data recorded");
                        }
                    }
                    isRecording = false;
                }
            } else if (currentState == State::IR_SEND) {
                if (hasRecordedData) {
                    if (hasJsonData) {
                        // Send the JSON document
                        irManager.sendIRData(tempJsonDoc);
                        LOG_DEBUG("Sent IR data via JSON document");
                    } else {
                        // Send the recorded IR data directly
                        irManager.sendIRData(recordedData);
                        LOG_DEBUG("Sent IR data directly");
                    }
                }
            } else if (currentState == State::IR_STATUS) {
                // Clear recorded data
                hasRecordedData = false;
                hasJsonData = false;
                tempJsonDoc.clear();
                LOG_DEBUG("Cleared recorded IR data and JSON");
            }
        });
    }

    void onUpdate() override {
        // Update display based on current state
        display.clear();

        // Show title
        display.setTextSize(1);
        display.printCentered("IR Hub - Test Menu", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"IR Record", "IR Send", "IR Status"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == static_cast<int>(currentState));
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show status information
        display.setTextSize(1);

        if (currentState == State::IR_RECORD) {
            if (isRecording) {
                display.print("Recording...", 0, 50);
            } else {
                display.print("Press long to start", 0, 50);
                if (hasJsonData) {
                    display.print("JSON: OK", 0, 60);
                }
            }
        } else if (currentState == State::IR_SEND) {
            if (hasRecordedData) {
                display.print("Press long to send", 0, 50);
            } else {
                display.print("No data to send", 0, 50);
            }
        } else if (currentState == State::IR_STATUS) {
            if (hasRecordedData) {
                display.print("Data recorded", 0, 50);
                display.print("Press long to clear", 0, 60);
                if (hasJsonData) {
                    display.print("JSON stored", 0, 70);
                }
            } else {
                display.print("No data recorded", 0, 50);
            }
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("TestMenu onExit");
        // Stop recording if active
        if (isRecording) {
            isRecording = false;
        }
    }
};
