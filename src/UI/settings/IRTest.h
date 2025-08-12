#pragma once
#include <ArduinoJson.h>
#include "../../global/Global.h"

class IRTest : public Screen {
   private:
    enum class State {
        RECORD,
        REPLAY,
        BACK,
    };

    enum class RecordState { IDLE, RECORDING, RECORDED };

    State currentState;
    RecordState recordState;
    JsonDocument storedCode;
    bool hasStoredCode;
    unsigned long recordingStartTime;
    const unsigned long RECORDING_TIMEOUT = 10000;  // 10 seconds timeout

   public:
    IRTest()
        : currentState(State::RECORD),
          recordState(RecordState::IDLE),
          hasStoredCode(false),
          recordingStartTime(0) {}

    void onEnter() override {
        LOG_DEBUG("IRTest onEnter");
        currentState = State::RECORD;
        recordState = RecordState::IDLE;

        button.setClickCallback([this]() {
            LOG_DEBUG("IRTest onButtonClick");
            switch (currentState) {
                case State::RECORD:
                    currentState = State::REPLAY;
                    break;
                case State::REPLAY:
                    currentState = State::BACK;
                    break;
                case State::BACK:
                    currentState = State::RECORD;
                    break;
            }
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("IRTest onButtonLongPress");
            switch (currentState) {
                case State::RECORD:
                    if (recordState == RecordState::IDLE) {
                        startRecording();
                    }
                    break;
                case State::REPLAY:
                    if (hasStoredCode) {
                        replayCode();
                    }
                    break;
                case State::BACK:
                    LOG_DEBUG("Going back to Settings");
                    router.pop();
                    break;
            }
        });
    }

    void onUpdate() override {
        // Check for IR codes during recording
        if (recordState == RecordState::RECORDING) {
            if (irManager.decode()) {
                if (irManager.isValid()) {
                    // Store the valid code
                    storedCode.clear();
                    irManager.saveLastCodeToJson(storedCode);
                    hasStoredCode = true;
                    recordState = RecordState::RECORDED;
                    LOG_DEBUG("IR code recorded successfully");
                    irManager.resumeIRReceiver();
                } else {
                    LOG_DEBUG("Invalid IR code received, continuing recording");
                    irManager.resumeIRReceiver();
                }
            }

            // Check for timeout
            if (millis() - recordingStartTime > RECORDING_TIMEOUT) {
                recordState = RecordState::IDLE;
                LOG_DEBUG("Recording timeout");
            }
        }

        display.clear();

        switch (currentState) {
            case State::RECORD:
                drawRecord();
                break;
            case State::REPLAY:
                drawReplay();
                break;
            case State::BACK:
                drawBack();
                break;
        }

        display.update();
    }

    void onExit() override {
        LOG_DEBUG("IRTest onExit");
        // Stop recording if active
        if (recordState == RecordState::RECORDING) {
            recordState = RecordState::IDLE;
        }
    }

   private:
    void startRecording() {
        LOG_DEBUG("Starting IR recording");
        recordState = RecordState::RECORDING;
        recordingStartTime = millis();
        storedCode.clear();
        hasStoredCode = false;
    }

    void replayCode() {
        if (hasStoredCode) {
            LOG_DEBUG("Replaying stored IR code");
            irManager.sendProtocol(IRCode::fromJson(storedCode));
        } else {
            LOG_DEBUG("No stored code to replay");
        }
    }

    void drawRecord() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Test", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Record", "Replay", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 0);  // Record is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show recording status
        if (recordState == RecordState::RECORDING) {
            display.setTextSize(1);
            display.print("Recording...", 0, 50);
            display.print("Point IR remote", 0, 60);
        } else if (recordState == RecordState::RECORDED) {
            display.setTextSize(1);
            display.print("Code recorded!", 0, 50);
        }
    }

    void drawReplay() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Test", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Record", "Replay", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 1);  // Replay is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }

        // Show replay status
        if (hasStoredCode) {
            display.setTextSize(1);
            display.print("Code ready", 0, 50);
            display.print("Long press to send", 0, 60);
        } else {
            display.setTextSize(1);
            display.print("No code stored", 0, 50);
            display.print("Record first", 0, 60);
        }
    }

    void drawBack() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("IR Test", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Record", "Replay", "Back"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == 2);  // Back is selected
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }
};
