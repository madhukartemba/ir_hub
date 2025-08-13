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
    bool isTimeoutError;
    IRCode firstCode;
    IRCode secondCode;
    unsigned long recordingStartTime;
    const unsigned long RECORDING_TIMEOUT = 5000;  // 5 seconds timeout

   public:
    AddDevice()
        : currentState(State::READY_TO_RECORD_FIRST), hasFirstCode(false), isTimeoutError(false) {}

    void onEnter() override {
        LOG_DEBUG("AddDevice onEnter");
        currentState = State::READY_TO_RECORD_FIRST;
        hasFirstCode = false;
        isTimeoutError = false;
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
            isTimeoutError = true;
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
    void drawPowerSymbol(int centerX, int centerY, bool inverted = false) {
        // Draw power symbol (circle with line at top)
        int radius = 3;

        if (inverted) {
            // For filled circles - draw in black (inverted)
            display.setTextColor(0);
            // Draw partial circle (power symbol)
            for (int angle = 45; angle <= 315; angle += 15) {
                int x = centerX + (radius - 1) * cos(angle * PI / 180);
                int y = centerY + (radius - 1) * sin(angle * PI / 180);
                display.drawPixel(x, y);
            }
            // Draw vertical line in center
            display.drawLine(centerX, centerY - 2, centerX, centerY + 1);
            display.setTextColor(1);  // Reset to white
        } else {
            // For outline circles - draw in white
            // Draw partial circle (power symbol)
            for (int angle = 45; angle <= 315; angle += 15) {
                int x = centerX + (radius - 1) * cos(angle * PI / 180);
                int y = centerY + (radius - 1) * sin(angle * PI / 180);
                display.drawPixel(x, y);
            }
            // Draw vertical line in center
            display.drawLine(centerX, centerY - 2, centerX, centerY + 1);
        }
    }

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
        display.printCentered("Press to record ON", 16);

        // LED-style indicators with power symbols
        // Power ON indicator (active - filled circle)
        display.fillCircle(32, 32, 6);  // Active indicator
        drawPowerSymbol(32, 32, true);  // Power symbol inside
        display.print("ON", 20, 42);

        // Power OFF indicator (inactive - outline circle)
        display.drawCircle(96, 32, 6);  // Inactive indicator (empty)
        display.print("OFF", 84, 42);
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

        // LED-style indicators with recording animation
        // Power ON indicator with pulsing animation
        unsigned long pulse = (millis() / 300) % 2;
        if (pulse) {
            // Pulsing filled circle with larger radius
            display.fillCircle(32, 32, 7);
            drawPowerSymbol(32, 32, true);
        } else {
            // Normal filled circle
            display.fillCircle(32, 32, 6);
            drawPowerSymbol(32, 32, true);
        }
        display.print("ON", 20, 42);

        // Power OFF indicator (inactive - outline circle)
        display.drawCircle(96, 32, 6);
        drawPowerSymbol(96, 32, false);
        display.print("OFF", 84, 42);

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 52, 98, 6, progress, 100, false);
    }

    void drawReadyToRecordSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("Press to record OFF", 16);

        // LED-style indicators
        // Power ON indicator (completed - filled circle with checkmark)
        display.fillCircle(32, 32, 6);
        drawPowerSymbol(32, 32, true);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 20, 42);

        // Power OFF indicator (active - filled circle, ready to record)
        display.fillCircle(96, 32, 6);
        drawPowerSymbol(96, 32, true);
        display.print("OFF", 84, 42);
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

        // LED-style indicators
        // Power ON indicator (completed - filled circle with checkmark)
        display.fillCircle(32, 32, 6);
        drawPowerSymbol(32, 32, true);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 20, 42);

        // Power OFF indicator with pulsing animation
        unsigned long pulse = (millis() / 300) % 2;
        if (pulse) {
            // Pulsing filled circle with larger radius
            display.fillCircle(96, 32, 7);
            drawPowerSymbol(96, 32, true);
        } else {
            // Normal filled circle
            display.fillCircle(96, 32, 6);
            drawPowerSymbol(96, 32, true);
        }
        display.print("OFF", 84, 42);

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 52, 98, 6, progress, 100, false);
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

        // LED-style indicators - both completed
        // Power ON indicator (completed - filled circle with checkmark)
        display.fillCircle(32, 32, 6);
        drawPowerSymbol(32, 32, true);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 20, 42);

        // Power OFF indicator (completed - filled circle with checkmark)
        display.fillCircle(96, 32, 6);
        drawPowerSymbol(96, 32, true);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(93, 33, 95, 35);
        display.drawLine(95, 35, 99, 31);
        display.setTextColor(1);  // Reset to white
        display.print("OFF", 84, 42);

        // Show comparison indicator
        display.printCentered("Checking match...", 52);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Better centered content layout
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 18);

        // Show result based on whether codes matched
        if (firstCode == secondCode) {
            display.printCentered("Single device saved", 30);
        } else {
            display.printCentered("Dual device saved", 30);
        }

        // Show protocol info with better centering
        display.setTextSize(1);
        String firstProtocol = typeToString(firstCode.getProtocol(), false);
        String secondProtocol = typeToString(secondCode.getProtocol(), false);

        if (firstCode == secondCode) {
            // Single device - show centered success icon and protocol
            display.drawCircle(64, 44, 8);
            display.drawLine(60, 45, 62, 47);
            display.drawLine(62, 47, 68, 41);
            display.printCentered("Protocol: " + firstProtocol, 56);
        } else {
            if (firstProtocol == secondProtocol) {
                // Same protocol - show centered success icon and protocol
                display.drawCircle(64, 44, 8);
                display.drawLine(60, 45, 62, 47);
                display.drawLine(62, 47, 68, 41);
                display.printCentered("Protocol: " + firstProtocol, 56);
            } else {
                // Different protocols - center the protocol info vertically
                display.printCentered("ON: " + firstProtocol, 42);
                display.printCentered("OFF: " + secondProtocol, 54);
            }
        }
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Better centered content layout
        display.setTextSize(1);
        display.printCentered("ERROR!", 20);

        // Centered error icon with better spacing
        display.drawCircle(64, 38, 8);
        display.drawLine(60, 34, 68, 42);
        display.drawLine(60, 42, 68, 34);

        // Show specific error cause with better centering
        display.setTextSize(1);
        if (isTimeoutError) {
            display.printCentered("Timeout", 52);
        } else if (currentState == State::ERROR) {
            if (!hasFirstCode) {
                // Show protocol info for first code failure
                String protocol = typeToString(firstCode.getProtocol(), false);
                if (protocol == "Unknown") {
                    display.printCentered("No signal detected", 52);
                } else {
                    display.printCentered("Protocol: " + protocol, 52);
                }
            } else {
                // Show protocol info for second code failure
                String protocol = typeToString(secondCode.getProtocol(), false);
                if (protocol == "Unknown") {
                    display.printCentered("No signal detected", 52);
                } else {
                    display.printCentered("Protocol: " + protocol, 52);
                }
            }
        }
    }
};
