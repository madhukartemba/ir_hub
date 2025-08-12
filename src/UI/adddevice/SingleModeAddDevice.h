#pragma once
#include "../../global/Global.h"

class SingleModeAddDevice : public Screen {
   private:
    enum class State {
        READY_TO_RECORD,
        RECORDING,
        SUCCESS,
        ERROR,
    };

    State currentState;
    IRCode recordedCode;
    unsigned long recordingStartTime;
    const unsigned long RECORDING_TIMEOUT = 5000;  // 5 seconds timeout

   public:
    SingleModeAddDevice() : currentState(State::READY_TO_RECORD) {}

    void onEnter() override {
        LOG_DEBUG("SingleModeAddDevice onEnter");
        currentState = State::READY_TO_RECORD;
        recordedCode = IRCode();

        button.setClickCallback([this]() {
            LOG_DEBUG("SingleModeAddDevice onButtonClick");
            // Click behavior can be customized based on current state
            switch (currentState) {
                case State::READY_TO_RECORD:
                    startRecording();
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
            LOG_DEBUG("SingleModeAddDevice onButtonLongPress");
            // Long press cancels operation and goes back
            if (currentState == State::RECORDING) {
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
        if (currentState == State::RECORDING &&
            (millis() - recordingStartTime) > RECORDING_TIMEOUT) {
            LOG_DEBUG("Recording timeout reached");
            stopRecording();
            currentState = State::ERROR;
        }

        // Check for IR code reception during recording
        if (currentState == State::RECORDING && irManager.decode()) {
            LOG_DEBUG("IR code received during recording");
            stopRecording();
        }

        display.clear();

        switch (currentState) {
            case State::READY_TO_RECORD:
                drawReadyToRecord();
                break;
            case State::RECORDING:
                drawRecording();
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
        LOG_DEBUG("SingleModeAddDevice onExit");
        // Clean up any recording resources if needed
        if (currentState == State::RECORDING) {
            irManager.stopCapture();
        }
    }

   private:
    void startRecording() {
        LOG_DEBUG("Starting single code IR recording");
        currentState = State::RECORDING;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecording() {
        LOG_DEBUG("Stopping single code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            recordedCode = irManager.getLastCode();

            // Validate code and save device
            if (recordedCode.isValid()) {
                try {
                    int deviceId = deviceManager.addSingleCommandDevice(recordedCode);
                    if (deviceId != -1) {
                        LOG_INFO("Single command device saved with ID: %d", deviceId);
                        currentState = State::SUCCESS;
                    } else {
                        LOG_ERROR("Failed to save single command device");
                        currentState = State::ERROR;
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception while saving device: %s", e.what());
                    currentState = State::ERROR;
                }
            } else {
                LOG_ERROR("Invalid code received");
                currentState = State::ERROR;
            }
        } else {
            LOG_ERROR("Invalid code received");
            currentState = State::ERROR;
        }
    }

    void drawReadyToRecord() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to record", 18);

        // Show IR icon/indicator
        display.drawRect(54, 30, 20, 8);
        display.print("IR", 58, 32);

        // Show instruction
        display.setTextSize(1);
        display.printCentered("Point remote & click", 44);
    }

    void drawRecording() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status with blinking indicator
        display.setTextSize(1);
        display.printCentered("RECORDING...", 18);

        // Show animated IR indicator
        display.fillRect(54, 30, 20, 8);
        display.setTextColor(0);  // Black text on white background
        display.print("IR", 58, 32);
        display.setTextColor(1);  // Reset to white text

        // Show progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(10, 42, 108, 6, progress, 100, false);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show success status
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 18);

        // Show checkmark
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 32, 62, 34);
        display.drawLine(62, 34, 68, 28);

        // Show code info
        display.setTextSize(1);
        display.printCentered("Device Added", 44);

        String protocol = typeToString(recordedCode.getProtocol(), false);
        display.printCentered("Protocol: " + protocol, 52);
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode Device", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show error status
        display.setTextSize(1);
        display.printCentered("ERROR!", 18);

        // Show X mark
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 28, 68, 36);
        display.drawLine(60, 36, 68, 28);

        // Show error message
        display.setTextSize(1);
        display.printCentered("Failed to record", 44);
        display.printCentered("IR code", 52);
    }
};
