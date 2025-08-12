#pragma once
#include "../../global/Global.h"

class DualModeAddDevice : public Screen {
   private:
    enum class State {
        READY_TO_RECORD_ON,
        RECORDING_ON,
        READY_TO_RECORD_OFF,
        RECORDING_OFF,
        SUCCESS,
        ERROR,
    };

    State currentState;
    bool hasOnCode;
    IRCode onCode;
    IRCode offCode;
    unsigned long recordingStartTime;
    const unsigned long RECORDING_TIMEOUT = 5000;  // 5 seconds timeout

   public:
    DualModeAddDevice() : currentState(State::READY_TO_RECORD_ON), hasOnCode(false) {}

    void onEnter() override {
        LOG_DEBUG("DualModeAddDevice onEnter");
        currentState = State::READY_TO_RECORD_ON;
        hasOnCode = false;
        onCode = IRCode();
        offCode = IRCode();

        button.setClickCallback([this]() {
            LOG_DEBUG("DualModeAddDevice onButtonClick");
            // Click behavior can be customized based on current state
            switch (currentState) {
                case State::READY_TO_RECORD_ON:
                    startRecordingOn();
                    break;
                case State::READY_TO_RECORD_OFF:
                    startRecordingOff();
                    break;
                case State::SUCCESS:
                case State::ERROR:
                    // Go back to main Add Device menu
                    router.pop();
                    break;
                default:
                    // Other states might not respond to click
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("DualModeAddDevice onButtonLongPress");
            // Long press cancels operation and goes back
            if (currentState == State::RECORDING_ON || currentState == State::RECORDING_OFF) {
                LOG_DEBUG("Cancelling recording via long press");
                irManager.stopCapture();
                currentState = State::ERROR;
            } else {
                // Go back to main Add Device menu
                router.pop();
            }
        });
    }

    void onUpdate() override {
        // Check for recording timeout
        if ((currentState == State::RECORDING_ON || currentState == State::RECORDING_OFF) &&
            (millis() - recordingStartTime) > RECORDING_TIMEOUT) {
            LOG_DEBUG("Recording timeout reached");
            if (currentState == State::RECORDING_ON) {
                stopRecordingOn();
            } else {
                stopRecordingOff();
            }
            currentState = State::ERROR;
        }

        // Check for IR code reception during recording
        if ((currentState == State::RECORDING_ON || currentState == State::RECORDING_OFF) &&
            irManager.decode()) {
            LOG_DEBUG("IR code received during recording");
            if (currentState == State::RECORDING_ON) {
                stopRecordingOn();
            } else {
                stopRecordingOff();
            }
        }

        display.clear();

        switch (currentState) {
            case State::READY_TO_RECORD_ON:
                drawReadyToRecordOn();
                break;
            case State::RECORDING_ON:
                drawRecordingOn();
                break;
            case State::READY_TO_RECORD_OFF:
                drawReadyToRecordOff();
                break;
            case State::RECORDING_OFF:
                drawRecordingOff();
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
        LOG_DEBUG("DualModeAddDevice onExit");
        // Clean up any recording resources if needed
        if (currentState == State::RECORDING_ON || currentState == State::RECORDING_OFF) {
            irManager.stopCapture();
        }
    }

   private:
    void startRecordingOn() {
        LOG_DEBUG("Starting ON code IR recording");
        currentState = State::RECORDING_ON;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingOn() {
        LOG_DEBUG("Stopping ON code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            onCode = irManager.getLastCode();
            hasOnCode = true;
            currentState = State::READY_TO_RECORD_OFF;
            LOG_INFO("ON code recorded successfully");
        } else {
            LOG_ERROR("Invalid ON code received");
            currentState = State::ERROR;
        }
    }

    void startRecordingOff() {
        LOG_DEBUG("Starting OFF code IR recording");
        currentState = State::RECORDING_OFF;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingOff() {
        LOG_DEBUG("Stopping OFF code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            offCode = irManager.getLastCode();

            // Validate both codes and save device
            if (onCode.isValid() && offCode.isValid()) {
                try {
                    int deviceId = deviceManager.addDualCommandDevice(onCode, offCode);
                    if (deviceId != -1) {
                        LOG_INFO("Dual command device saved with ID: %d", deviceId);
                        currentState = State::SUCCESS;
                    } else {
                        LOG_ERROR("Failed to save dual command device");
                        currentState = State::ERROR;
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception while saving device: %s", e.what());
                    currentState = State::ERROR;
                }
            } else {
                LOG_ERROR("Invalid codes - ON: %s, OFF: %s", onCode.isValid() ? "valid" : "invalid",
                          offCode.isValid() ? "valid" : "invalid");
                currentState = State::ERROR;
            }
        } else {
            LOG_ERROR("Invalid OFF code received");
            currentState = State::ERROR;
        }
    }

    void drawReadyToRecordOn() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record ON", 18);

        // Show progress indicator
        display.drawRect(20, 28, 88, 12);
        display.print("ON", 24, 32);
        display.print("OFF", 90, 32);
        display.fillRect(22, 30, 20, 8);  // Highlight ON
    }

    void drawRecordingOn() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING ON...", 18);

        // Show progress indicator with animation
        display.drawRect(20, 28, 88, 12);
        display.fillRect(22, 30, 20, 8);  // Highlight ON (filled)
        display.setTextColor(0);
        display.print("ON", 24, 32);
        display.setTextColor(1);
        display.print("OFF", 90, 32);

        // Show progress bar
        display.drawProgressBar(10, 42, 108, 6, 50, 100, false);
    }

    void drawReadyToRecordOff() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record OFF", 18);

        // Show progress indicator
        display.drawRect(20, 28, 88, 12);
        display.fillRect(22, 30, 20, 8);  // ON completed
        display.setTextColor(0);
        display.print("ON", 24, 32);
        display.setTextColor(1);
        display.print("OFF", 90, 32);
        display.drawCircle(30, 42, 3);  // Checkmark for ON
    }

    void drawRecordingOff() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING OFF...", 18);

        // Show progress indicator with animation
        display.drawRect(20, 28, 88, 12);
        display.fillRect(22, 30, 20, 8);  // ON completed
        display.fillRect(88, 30, 18, 8);  // OFF recording
        display.setTextColor(0);
        display.print("ON", 24, 32);
        display.print("OFF", 90, 32);
        display.setTextColor(1);

        // Show progress bar
        display.drawProgressBar(10, 42, 108, 6, 75, 100, false);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show success status
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 18);

        // Show completed progress
        display.drawRect(20, 28, 88, 12);
        display.fillRect(22, 30, 20, 8);  // ON completed
        display.fillRect(88, 30, 18, 8);  // OFF completed
        display.setTextColor(0);
        display.print("ON", 24, 32);
        display.print("OFF", 90, 32);
        display.setTextColor(1);

        // Show checkmarks
        display.drawCircle(30, 44, 3);
        display.drawCircle(98, 44, 3);

        // Show protocol info
        display.setTextSize(1);
        String onProtocol = typeToString(onCode.getProtocol(), false);
        String offProtocol = typeToString(offCode.getProtocol(), false);

        if (onProtocol == offProtocol) {
            display.printCentered("Protocol: " + onProtocol, 52);
        } else {
            display.printCentered("ON: " + onProtocol, 52);
            display.printCentered("OFF: " + offProtocol, 60);
        }
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show error status
        display.setTextSize(1);
        display.printCentered("ERROR!", 18);

        // Show X mark
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 28, 68, 36);
        display.drawLine(60, 36, 68, 28);

        // Show error message based on state
        display.setTextSize(1);
        if (currentState == State::ERROR) {
            if (!hasOnCode) {
                display.printCentered("Failed to record", 44);
                display.printCentered("ON code", 52);
            } else {
                display.printCentered("Failed to record", 44);
                display.printCentered("OFF code", 52);
            }
        } else {
            display.printCentered("Recording timeout", 44);
            display.printCentered("or invalid code", 52);
        }
    }
};
