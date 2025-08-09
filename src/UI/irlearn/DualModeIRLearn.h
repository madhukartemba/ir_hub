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
            // Draw UI showing "Ready to Record ON Code"
            // Show instructions: "Long press to start recording ON command"
            // TODO: Implement UI drawing
        }

        void drawRecordingOn() {
            // Draw UI showing ON code recording in progress
            // Show animation or indicator
            // Show instructions: "Point remote and press ON button, long press to stop"
            // TODO: Implement UI drawing with recording indicator
        }

        void drawReadyToRecordOff() {
            // Draw UI showing "Ready to Record OFF Code"
            // Show that ON code is already recorded
            // Show instructions: "Long press to start recording OFF command"
            // TODO: Implement UI drawing
        }

        void drawRecordingOff() {
            // Draw UI showing OFF code recording in progress
            // Show animation or indicator
            // Show instructions: "Point remote and press OFF button, long press to stop"
            // TODO: Implement UI drawing with recording indicator
        }

        void drawSuccess() {
            // Draw UI showing successful recording of both codes
            // Show success message and both codes info
            // Show instructions: "Click or long press to continue"
            // TODO: Implement UI drawing
        }

        void drawError() {
            // Draw UI showing recording error
            // Show error message
            // Show instructions: "Click or long press to retry"
            // TODO: Implement UI drawing
        }
};
