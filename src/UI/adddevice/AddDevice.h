#pragma once
#include "../../global/Global.h"

class AddDevice : public Screen {
   private:
    enum class State {
        READY_TO_RECORD_FIRST,
        RECORDING_FIRST,
        READY_TO_RECORD_SECOND,
        RECORDING_SECOND,
        COMPARING,
        SUCCESS,
        ERROR,
    };

    State currentState;
    bool hasFirstCode;
    IRCode firstCode;
    IRCode secondCode;
    unsigned long recordingStartTime;
    const unsigned long RECORDING_TIMEOUT = 5000;  // 5 seconds timeout

   public:
    AddDevice() : currentState(State::READY_TO_RECORD_FIRST), hasFirstCode(false) {}

    void onEnter() override {
        LOG_DEBUG("AddDevice onEnter");
        currentState = State::READY_TO_RECORD_FIRST;
        hasFirstCode = false;
        firstCode = IRCode();
        secondCode = IRCode();

        button.setClickCallback([this]() {
            LOG_DEBUG("AddDevice onButtonClick");
            // Click behavior can be customized based on current state
            switch (currentState) {
                case State::READY_TO_RECORD_FIRST:
                    startRecordingFirst();
                    break;
                case State::READY_TO_RECORD_SECOND:
                    startRecordingSecond();
                    break;
                case State::SUCCESS:
                case State::ERROR:
                    // Go back to main menu
                    router.pop();
                    break;
                default:
                    // Other states might not respond to click
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("AddDevice onButtonLongPress");
            // Long press cancels operation and goes back
            if (currentState == State::RECORDING_FIRST || currentState == State::RECORDING_SECOND) {
                LOG_DEBUG("Cancelling recording via long press");
                irManager.stopCapture();
                currentState = State::ERROR;
            } else {
                // Go back to main menu
                router.pop();
            }
        });
    }

    void onUpdate() override {
        // Check for recording timeout
        if ((currentState == State::RECORDING_FIRST || currentState == State::RECORDING_SECOND) &&
            (millis() - recordingStartTime) > RECORDING_TIMEOUT) {
            LOG_DEBUG("Recording timeout reached");
            if (currentState == State::RECORDING_FIRST) {
                stopRecordingFirst();
            } else {
                stopRecordingSecond();
            }
            currentState = State::ERROR;
        }

        // Check for IR code reception during recording
        if ((currentState == State::RECORDING_FIRST || currentState == State::RECORDING_SECOND) &&
            irManager.decode()) {
            LOG_DEBUG("IR code received during recording");
            if (currentState == State::RECORDING_FIRST) {
                stopRecordingFirst();
            } else {
                stopRecordingSecond();
            }
        }

        display.clear();

        switch (currentState) {
            case State::READY_TO_RECORD_FIRST:
                drawReadyToRecordFirst();
                break;
            case State::RECORDING_FIRST:
                drawRecordingFirst();
                break;
            case State::READY_TO_RECORD_SECOND:
                drawReadyToRecordSecond();
                break;
            case State::RECORDING_SECOND:
                drawRecordingSecond();
                break;
            case State::COMPARING:
                drawComparing();
                break;
            case State::SUCCESS:
                drawSuccess();
                break;
            case State::ERROR:
                drawError();
                break;
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("AddDevice onExit");
        // Clean up any recording resources if needed
        if (currentState == State::RECORDING_FIRST || currentState == State::RECORDING_SECOND) {
            irManager.stopCapture();
        }
    }

   private:
    void startRecordingFirst() {
        LOG_DEBUG("Starting first code IR recording");
        currentState = State::RECORDING_FIRST;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingFirst() {
        LOG_DEBUG("Stopping first code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            firstCode = irManager.getLastCode();
            hasFirstCode = true;
            currentState = State::READY_TO_RECORD_SECOND;
            LOG_INFO("First code recorded successfully");
        } else {
            LOG_ERROR("Invalid first code received");
            currentState = State::ERROR;
        }
    }

    void startRecordingSecond() {
        LOG_DEBUG("Starting second code IR recording");
        currentState = State::RECORDING_SECOND;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingSecond() {
        LOG_DEBUG("Stopping second code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            secondCode = irManager.getLastCode();

            // Compare the two codes
            if (firstCode.isValid() && secondCode.isValid()) {
                currentState = State::COMPARING;

                // Add a small delay to show the comparing state
                delay(1000);

                if (firstCode == secondCode) {
                    // Codes match, save as single command device
                    try {
                        int deviceId = deviceManager.addSingleCommandDevice(firstCode);
                        if (deviceId != -1) {
                            LOG_INFO("Auto mode device saved with ID: %d", deviceId);
                            currentState = State::SUCCESS;
                        } else {
                            LOG_ERROR("Failed to save auto mode device");
                            currentState = State::ERROR;
                        }
                    } catch (const std::exception& e) {
                        LOG_ERROR("Exception while saving device: %s", e.what());
                        currentState = State::ERROR;
                    }
                } else {
                    LOG_INFO("Codes don't match - saving as dual device");
                    // Codes don't match, save as dual command device
                    try {
                        int deviceId = deviceManager.addDualCommandDevice(firstCode, secondCode);
                        if (deviceId != -1) {
                            LOG_INFO("Auto mode device saved as dual device with ID: %d", deviceId);
                            currentState = State::SUCCESS;
                        } else {
                            LOG_ERROR("Failed to save auto mode device as dual device");
                            currentState = State::ERROR;
                        }
                    } catch (const std::exception& e) {
                        LOG_ERROR("Exception while saving dual device: %s", e.what());
                        currentState = State::ERROR;
                    }
                }
            } else {
                LOG_ERROR("Invalid codes - first: %s, second: %s",
                          firstCode.isValid() ? "valid" : "invalid",
                          secondCode.isValid() ? "valid" : "invalid");
                currentState = State::ERROR;
            }
        } else {
            LOG_ERROR("Invalid second code received");
            currentState = State::ERROR;
        }
    }

    void drawReadyToRecordFirst() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record ON", 18);

        // Show progress indicator
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // Highlight ON
        display.setTextColor(0);          // Black text on white background
        display.print("ON", 24, 34);
        display.setTextColor(1);  // Reset to white text
        display.print("OFF", 90, 34);
    }

    void drawRecordingFirst() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING ON...", 18);

        // Show progress indicator with animation
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // Highlight ON (filled)
        display.setTextColor(0);
        display.print("ON", 24, 34);
        display.setTextColor(1);
        display.print("OFF", 90, 34);

        // Show timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(10, 44, 108, 6, progress, 100, false);
    }

    void drawReadyToRecordSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record OFF", 18);

        // Show progress indicator
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // ON completed
        display.setTextColor(0);
        display.print("ON", 24, 34);
        display.setTextColor(1);
        display.print("OFF", 90, 34);
        display.drawCircle(30, 44, 3);  // Checkmark for ON
    }

    void drawRecordingSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING OFF...", 18);

        // Show progress indicator with animation
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // ON completed
        display.fillRect(88, 32, 18, 8);  // OFF recording
        display.setTextColor(0);
        display.print("ON", 24, 34);
        display.print("OFF", 90, 34);
        display.setTextColor(1);

        // Show timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(10, 44, 108, 6, progress, 100, false);
    }

    void drawComparing() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("COMPARING...", 18);

        // Show progress indicator
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // ON completed
        display.fillRect(88, 32, 18, 8);  // OFF completed
        display.setTextColor(0);
        display.print("ON", 24, 34);
        display.print("OFF", 90, 34);
        display.setTextColor(1);

        // Show checkmarks
        display.drawCircle(30, 46, 3);
        display.drawCircle(98, 46, 3);

        // Show comparison indicator
        display.printCentered("Checking match...", 52);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show success status
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 18);

        // Show completed progress
        display.drawRect(20, 30, 88, 12);
        display.fillRect(22, 32, 20, 8);  // ON completed
        display.fillRect(88, 32, 18, 8);  // OFF completed
        display.setTextColor(0);
        display.print("ON", 24, 34);
        display.print("OFF", 90, 34);
        display.setTextColor(1);

        // Show checkmarks
        display.drawCircle(30, 46, 3);
        display.drawCircle(98, 46, 3);

        // Show result based on whether codes matched
        if (firstCode == secondCode) {
            display.printCentered("Codes match!", 52);
            display.printCentered("Single device saved", 60);
        } else {
            display.printCentered("Codes different!", 52);
            display.printCentered("Dual device saved", 60);
        }

        // Show protocol info
        display.setTextSize(1);
        String firstProtocol = typeToString(firstCode.getProtocol(), false);
        String secondProtocol = typeToString(secondCode.getProtocol(), false);

        if (firstCode == secondCode) {
            display.printCentered("Protocol: " + firstProtocol, 68);
        } else {
            if (firstProtocol == secondProtocol) {
                display.printCentered("Protocol: " + firstProtocol, 68);
            } else {
                display.printCentered("ON: " + firstProtocol, 68);
                display.printCentered("OFF: " + secondProtocol, 76);
            }
        }
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show error status
        display.setTextSize(1);
        display.printCentered("ERROR!", 18);

        // Show X mark
        display.drawCircle(64, 36, 8);
        display.drawLine(60, 32, 68, 40);
        display.drawLine(60, 40, 68, 32);

        // Show error message based on state
        display.setTextSize(1);
        if (currentState == State::ERROR) {
            if (!hasFirstCode) {
                display.printCentered("Failed to record", 44);
                display.printCentered("first code", 52);
            } else {
                display.printCentered("Failed to record", 44);
                display.printCentered("second code", 52);
            }
        } else {
            display.printCentered("Recording timeout", 44);
            display.printCentered("or invalid code", 52);
        }
    }
};
