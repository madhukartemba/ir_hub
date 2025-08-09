#pragma once
#include "../../global/Global.h"

class DualModeIRLearn : public Screen {
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
    // TODO: Add variables to store ON and OFF codes

   public:
    DualModeIRLearn() : currentState(State::READY_TO_RECORD_ON), hasOnCode(false) {}

    void onEnter() override {
        LOG_DEBUG("DualModeIRLearn onEnter");
        currentState = State::READY_TO_RECORD_ON;
        hasOnCode = false;

        button.setClickCallback([this]() {
            LOG_DEBUG("DualModeIRLearn onButtonClick");
            // Click behavior can be customized based on current state
            switch (currentState) {
                case State::SUCCESS:
                case State::ERROR:
                    // Go back to main IR Learn menu
                    router.pop();
                    break;
                default:
                    // Other states might not respond to click
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("DualModeIRLearn onButtonLongPress");
            switch (currentState) {
                case State::READY_TO_RECORD_ON:
                    startRecordingOn();
                    break;
                case State::RECORDING_ON:
                    stopRecordingOn();
                    break;
                case State::READY_TO_RECORD_OFF:
                    startRecordingOff();
                    break;
                case State::RECORDING_OFF:
                    stopRecordingOff();
                    break;
                case State::SUCCESS:
                case State::ERROR:
                    // Go back to main IR Learn menu
                    router.pop();
                    break;
            }
        });
    }

    void onUpdate() override {
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
        LOG_DEBUG("DualModeIRLearn onExit");
        // Clean up any recording resources if needed
        if (currentState == State::RECORDING_ON || currentState == State::RECORDING_OFF) {
            // TODO: Stop any active recording
        }
    }

   private:
    void startRecordingOn() {
        LOG_DEBUG("Starting ON code IR recording");
        currentState = State::RECORDING_ON;
        // TODO: Initialize IR receiver for ON code recording
    }

    void stopRecordingOn() {
        LOG_DEBUG("Stopping ON code IR recording");
        // TODO: Stop IR receiver and save ON code
        hasOnCode = true;
        currentState = State::READY_TO_RECORD_OFF;
    }

    void startRecordingOff() {
        LOG_DEBUG("Starting OFF code IR recording");
        currentState = State::RECORDING_OFF;
        // TODO: Initialize IR receiver for OFF code recording
    }

    void stopRecordingOff() {
        LOG_DEBUG("Stopping OFF code IR recording");
        // TODO: Stop IR receiver and save OFF code
        // TODO: Validate and save both codes
        currentState = State::SUCCESS;
    }

    void drawReadyToRecordOn() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Ready for ON Code", 18);

        // Show progress indicator
        display.drawRect(20, 28, 88, 12);
        display.print("ON", 24, 32);
        display.print("OFF", 90, 32);
        display.fillRect(22, 30, 20, 8);  // Highlight ON

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Long press to start", 45);
        display.printCentered("recording ON code", 53);
    }

    void drawRecordingOn() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

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

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Press ON on remote", 52);
        display.printCentered("Hold to stop", 60);
    }

    void drawReadyToRecordOff() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Ready for OFF Code", 18);

        // Show progress indicator
        display.drawRect(20, 28, 88, 12);
        display.fillRect(22, 30, 20, 8);  // ON completed
        display.setTextColor(0);
        display.print("ON", 24, 32);
        display.setTextColor(1);
        display.print("OFF", 90, 32);
        display.drawCircle(30, 42, 3);  // Checkmark for ON

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Long press to start", 48);
        display.printCentered("recording OFF code", 56);
    }

    void drawRecordingOff() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

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

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Press OFF on remote", 52);
        display.printCentered("Hold to stop", 60);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

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

        // Show code info
        display.setTextSize(1);
        display.printCentered("Both codes learned", 52);
        display.print("Press to continue", 8, 60);
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Dual Mode IR", 0);

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
        display.printCentered("IR codes", 52);

        // Show instructions
        display.setTextSize(1);
        display.print("Press to retry", 14, 60);
    }
};
