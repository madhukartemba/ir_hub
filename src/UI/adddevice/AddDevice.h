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
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record ON", 16);

        // Step indicator with icons
        display.drawCircle(32, 32, 8);  // Step 1 circle
        display.drawCircle(96, 32, 8);  // Step 2 circle

        // Power icon for ON (filled circle with line)
        display.fillCircle(32, 32, 4);
        display.drawLine(32, 28, 32, 36);
        display.drawLine(28, 32, 36, 32);

        // Power icon for OFF (outline circle with line)
        display.drawCircle(96, 32, 4);
        display.drawLine(96, 28, 96, 36);

        // Step numbers
        display.setTextSize(1);
        display.print("1", 30, 36);
        display.print("2", 94, 36);

        // Progress indicator
        display.drawLine(40, 32, 88, 32);
        display.fillCircle(40, 32, 2);  // Current step
    }

    void drawRecordingFirst() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING ON...", 16);

        // Step indicator with recording animation
        display.drawCircle(32, 32, 8);  // Step 1 circle
        display.drawCircle(96, 32, 8);  // Step 2 circle

        // Animated power icon for ON (pulsing effect)
        unsigned long pulse = (millis() / 200) % 2;
        if (pulse) {
            display.fillCircle(32, 32, 4);
        } else {
            display.drawCircle(32, 32, 4);
        }
        display.drawLine(32, 28, 32, 36);
        display.drawLine(28, 32, 36, 32);

        // Power icon for OFF (outline)
        display.drawCircle(96, 32, 4);
        display.drawLine(96, 28, 96, 36);

        // Step numbers
        display.setTextSize(1);
        display.print("1", 30, 36);
        display.print("2", 94, 36);

        // Progress indicator
        display.drawLine(40, 32, 88, 32);
        display.fillCircle(40, 32, 2);  // Current step

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 48, 98, 6, progress, 100, false);
    }

    void drawReadyToRecordSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record OFF", 16);

        // Step indicator with completed step 1
        display.drawCircle(32, 32, 8);  // Step 1 circle
        display.drawCircle(96, 32, 8);  // Step 2 circle

        // Completed power icon for ON (filled with checkmark)
        display.fillCircle(32, 32, 4);
        display.drawLine(32, 28, 32, 36);
        display.drawLine(28, 32, 36, 32);
        // Checkmark
        display.drawLine(30, 32, 32, 34);
        display.drawLine(32, 34, 35, 31);

        // Power icon for OFF (outline)
        display.drawCircle(96, 32, 4);
        display.drawLine(96, 28, 96, 36);

        // Step numbers
        display.setTextSize(1);
        display.print("1", 30, 36);
        display.print("2", 94, 36);

        // Progress indicator
        display.drawLine(40, 32, 88, 32);
        display.fillCircle(40, 32, 2);  // Step 1 complete
        display.fillCircle(88, 32, 2);  // Current step
    }

    void drawRecordingSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING OFF...", 16);

        // Step indicator with recording animation
        display.drawCircle(32, 32, 8);  // Step 1 circle
        display.drawCircle(96, 32, 8);  // Step 2 circle

        // Completed power icon for ON
        display.fillCircle(32, 32, 4);
        display.drawLine(32, 28, 32, 36);
        display.drawLine(28, 32, 36, 32);
        // Checkmark
        display.drawLine(30, 32, 32, 34);
        display.drawLine(32, 34, 35, 31);

        // Animated power icon for OFF (pulsing effect)
        unsigned long pulse = (millis() / 200) % 2;
        if (pulse) {
            display.fillCircle(96, 32, 4);
        } else {
            display.drawCircle(96, 32, 4);
        }
        display.drawLine(96, 28, 96, 36);

        // Step numbers
        display.setTextSize(1);
        display.print("1", 30, 36);
        display.print("2", 94, 36);

        // Progress indicator
        display.drawLine(40, 32, 88, 32);
        display.fillCircle(40, 32, 2);  // Step 1 complete
        display.fillCircle(88, 32, 2);  // Current step

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 48, 98, 6, progress, 100, false);
    }

    void drawComparing() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("COMPARING...", 16);

        // Step indicator with both steps completed
        display.drawCircle(32, 32, 8);  // Step 1 circle
        display.drawCircle(96, 32, 8);  // Step 2 circle

        // Completed power icon for ON
        display.fillCircle(32, 32, 4);
        display.drawLine(32, 28, 32, 36);
        display.drawLine(28, 32, 36, 32);
        // Checkmark
        display.drawLine(30, 32, 32, 34);
        display.drawLine(32, 34, 35, 31);

        // Completed power icon for OFF
        display.fillCircle(96, 32, 4);
        display.drawLine(96, 28, 96, 36);
        // Checkmark
        display.drawLine(94, 32, 96, 34);
        display.drawLine(96, 34, 99, 31);

        // Step numbers
        display.setTextSize(1);
        display.print("1", 30, 36);
        display.print("2", 94, 36);

        // Progress indicator
        display.drawLine(40, 32, 88, 32);
        display.fillCircle(40, 32, 2);  // Step 1 complete
        display.fillCircle(88, 32, 2);  // Step 2 complete

        // Show comparison indicator
        display.printCentered("Checking match...", 48);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show success status
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 16);

        // Success icon (checkmark in circle)
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 32, 62, 34);
        display.drawLine(62, 34, 68, 28);

        // Show result based on whether codes matched (compact)
        if (firstCode == secondCode) {
            display.printCentered("Single device saved", 48);
        } else {
            display.printCentered("Dual device saved", 48);
        }

        // Show protocol info (compact)
        display.setTextSize(1);
        String firstProtocol = typeToString(firstCode.getProtocol(), false);
        String secondProtocol = typeToString(secondCode.getProtocol(), false);

        if (firstCode == secondCode) {
            display.printCentered("Protocol: " + firstProtocol, 56);
        } else {
            if (firstProtocol == secondProtocol) {
                display.printCentered("Protocol: " + firstProtocol, 56);
            } else {
                // Show both protocols in one line
                display.printCentered("ON:" + firstProtocol + " OFF:" + secondProtocol, 56);
            }
        }
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show error status
        display.setTextSize(1);
        display.printCentered("ERROR!", 16);

        // Show X mark (smaller)
        display.drawCircle(64, 32, 6);
        display.drawLine(61, 29, 67, 35);
        display.drawLine(61, 35, 67, 29);

        // Show error message based on state (compact)
        display.setTextSize(1);
        if (currentState == State::ERROR) {
            if (!hasFirstCode) {
                display.printCentered("Failed to record first code", 44);
            } else {
                display.printCentered("Failed to record second code", 44);
            }
        } else {
            display.printCentered("Recording timeout or invalid code", 44);
        }
    }
};
