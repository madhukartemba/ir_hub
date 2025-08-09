#pragma once
#include "../../global/Global.h"

class SingleModeIRLearn : public Screen {
   private:
    enum class State {
        READY_TO_RECORD,
        RECORDING,
        SUCCESS,
        ERROR,
    };

    State currentState;

   public:
    SingleModeIRLearn() : currentState(State::READY_TO_RECORD) {}

    void onEnter() override {
        LOG_DEBUG("SingleModeIRLearn onEnter");
        currentState = State::READY_TO_RECORD;

        button.setClickCallback([this]() {
            LOG_DEBUG("SingleModeIRLearn onButtonClick");
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
            LOG_DEBUG("SingleModeIRLearn onButtonLongPress");
            switch (currentState) {
                case State::READY_TO_RECORD:
                    startRecording();
                    break;
                case State::RECORDING:
                    stopRecording();
                    router.pop();
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
        LOG_DEBUG("SingleModeIRLearn onExit");
        // Clean up any recording resources if needed
        if (currentState == State::RECORDING) {
            stopRecording();
        }
    }

   private:
    void startRecording() {
        LOG_DEBUG("Starting single code IR recording");
        currentState = State::RECORDING;
        // TODO: Initialize IR receiver for single code recording
        // TODO: Set up recording timeout
    }

    void stopRecording() {
        LOG_DEBUG("Stopping single code IR recording");
        // TODO: Stop IR receiver
        // TODO: Process recorded data
        // For now, simulate success
        currentState = State::SUCCESS;
    }

    void drawReadyToRecord() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status
        display.setTextSize(1);
        display.printCentered("Ready to Record", 20);

        // Show IR icon/indicator
        display.drawRect(54, 30, 20, 8);
        display.print("IR", 58, 32);

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Long press to start", 45);
        display.printCentered("recording IR code", 53);
    }

    void drawRecording() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show status with blinking indicator
        display.setTextSize(1);
        display.printCentered("RECORDING...", 20);

        // Show animated IR indicator
        display.fillRect(54, 30, 20, 8);
        display.setTextColor(0);  // Black text on white background
        display.print("IR", 58, 32);
        display.setTextColor(1);  // Reset to white text

        // Show progress bar
        display.drawProgressBar(10, 40, 108, 6, 50, 100, false);

        // Show instructions
        display.setTextSize(1);
        display.printCentered("Point remote at IR", 50);
        display.printCentered("Long press to stop", 58);
    }

    void drawSuccess() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show success status
        display.setTextSize(1);
        display.printCentered("SUCCESS!", 20);

        // Show checkmark
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 32, 62, 34);
        display.drawLine(62, 34, 68, 28);

        // Show code info
        display.setTextSize(1);
        display.printCentered("IR Code Learned", 44);
        display.printCentered("Protocol: NEC", 52);

        // Show instructions
        display.setTextSize(1);
        display.print("Press to continue", 8, 58);
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Single Mode IR", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show error status
        display.setTextSize(1);
        display.printCentered("ERROR!", 20);

        // Show X mark
        display.drawCircle(64, 32, 8);
        display.drawLine(60, 28, 68, 36);
        display.drawLine(60, 36, 68, 28);

        // Show error message
        display.setTextSize(1);
        display.printCentered("Failed to record", 44);
        display.printCentered("IR code", 52);

        // Show instructions
        display.setTextSize(1);
        display.print("Press to retry", 14, 58);
    }
};
