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
            // Draw UI showing "Ready to Record Single Code"
            // Show instructions: "Long press to start recording"
            // TODO: Implement UI drawing
        }

        void drawRecording() {
            // Draw UI showing recording in progress
            // Show animation or indicator
            // Show instructions: "Point remote and press button, long press to stop"
            // TODO: Implement UI drawing with recording indicator
        }

        void drawSuccess() {
            // Draw UI showing successful recording
            // Show success message and recorded code info
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
