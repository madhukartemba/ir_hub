#pragma once
#include "../../global/Global.h"
#include "../../preferences.h"

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
    const unsigned long RECORDING_TIMEOUT = 10000;  // 10 seconds timeout

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
        ledRing.breathe(COLOR_INFO_ROYAL);

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
            // Long press always means "back". If we're mid-recording we just
            // need to stop the IR capture before navigating away — we
            // explicitly do NOT drop into the ERROR state, because that would
            // show a misleading "Unknown protocol / Try again" screen
            // (firstCode is still the default-constructed UNKNOWN value) when
            // the user only wanted to abort.
            if (currentState == State::RECORDING_FIRST || currentState == State::RECORDING_SECOND) {
                LOG_DEBUG("Cancelling recording via long press");
                irManager.stopCapture();
            }
            router.pop();
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
            setLedRingError();
            speaker.errorBeep();
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
    // LED Ring Methods for different states
    void setLedRingRecording() {
        // Solid blue color during recording
        ledRing.solid(COLOR_INFO_ROYAL);
        ledRing.finishTransition();
    }

    void setLedRingSuccess() {
        // Green pulse for success
        ledRing.rainbow();
    }

    void setLedRingError() {
        // Red pulse for errors
        ledRing.breathe(COLOR_ERROR);
    }

    void setLedRingFirstCodeSuccess() {
        // Green pulse for first code success
        ledRing.breathe(COLOR_SUCCESS);
    }

    void setLedRingSecondCodeSuccess() {
        // Green pulse with more pulses for second code success
        ledRing.breathe(COLOR_SUCCESS);
    }

    void startRecordingFirst() {
        LOG_DEBUG("Starting first code IR recording");
        currentState = State::RECORDING_FIRST;
        setLedRingRecording();
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingFirst() {
        LOG_DEBUG("Stopping first code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            setLedRingFirstCodeSuccess();
            firstCode = irManager.getLastCode();
            hasFirstCode = true;
            currentState = State::READY_TO_RECORD_SECOND;
            LOG_INFO("First code recorded successfully");
        } else {
            setLedRingError();
            LOG_ERROR("Invalid first code received");
            currentState = State::ERROR;
            speaker.errorBeep();
        }
    }

    void startRecordingSecond() {
        LOG_DEBUG("Starting second code IR recording");
        setLedRingRecording();
        currentState = State::RECORDING_SECOND;
        recordingStartTime = millis();
        irManager.startCapture();
    }

    void stopRecordingSecond() {
        LOG_DEBUG("Stopping second code IR recording");
        irManager.stopCapture();

        if (irManager.isValid()) {
            setLedRingSecondCodeSuccess();
            secondCode = irManager.getLastCode();

            // Compare the two codes
            if (firstCode.isValid() && secondCode.isValid()) {
                currentState = State::COMPARING;

                if (firstCode == secondCode) {
                    // Codes match, save as single command device
                    int deviceId = deviceManager.addSingleCommandDevice(firstCode);
                    if (deviceId != -1) {
                        LOG_INFO("Auto mode device saved with ID: %d", deviceId);
                        currentState = State::SUCCESS;
                        setLedRingSuccess();
                        speaker.successBeep();
                    } else {
                        LOG_ERROR("Failed to save auto mode device");
                        currentState = State::ERROR;
                        setLedRingError();
                        speaker.errorBeep();
                    }
                } else {
                    LOG_INFO("Codes don't match - saving as dual device");
                    // Codes don't match, save as dual command device
                    int deviceId = deviceManager.addDualCommandDevice(firstCode, secondCode);
                    if (deviceId != -1) {
                        LOG_INFO("Auto mode device saved as dual device with ID: %d", deviceId);
                        currentState = State::SUCCESS;
                        setLedRingSuccess();
                        speaker.successBeep();
                    } else {
                        LOG_ERROR("Failed to save auto mode device as dual device");
                        currentState = State::ERROR;
                        setLedRingError();
                        speaker.errorBeep();
                    }
                }
            } else {
                setLedRingError();
                LOG_ERROR("Invalid codes - first: %s, second: %s",
                          firstCode.isValid() ? "valid" : "invalid",
                          secondCode.isValid() ? "valid" : "invalid");
                currentState = State::ERROR;
                speaker.errorBeep();
            }
        } else {
            LOG_ERROR("Invalid second code received");
            setLedRingError();
            currentState = State::ERROR;
            speaker.errorBeep();
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
        display.printCentered("Click to begin", 53);

        // LED-style indicators with power symbols
        // Power ON indicator (active - filled circle)
        display.fillCircle(32, 32, 6);  // Active indicator
        display.print("ON", 24, 42);

        // Power OFF indicator (inactive - outline circle)
        display.drawCircle(96, 32, 6);  // Inactive indicator (empty)
        display.print("OFF", 88, 42);
    }

    void drawRecordingFirst() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING ON...", 12);

        // LED-style indicators with recording animation
        // Power ON indicator with pulsing animation
        unsigned long pulse = (millis() / 300) % 2;
        if (pulse) {
            // Pulsing filled circle with larger radius
            display.fillCircle(32, 32, 7);
        } else {
            // Normal filled circle
            display.fillCircle(32, 32, 6);
        }
        display.print("ON", 24, 42);

        // Power OFF indicator (inactive - outline circle)
        display.drawCircle(96, 32, 6);
        display.print("OFF", 88, 42);

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 56, 98, 6, progress, 100, false);
    }

    void drawReadyToRecordSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("Click to begin", 53);

        // LED-style indicators
        // Power ON indicator (completed - filled circle with checkmark)
        display.fillCircle(32, 32, 6);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 24, 42);

        // Power OFF indicator (active - filled circle, ready to record)
        display.fillCircle(96, 32, 6);
        display.print("OFF", 88, 42);
    }

    void drawRecordingSecond() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        // Show status
        display.setTextSize(1);
        display.printCentered("RECORDING OFF...", 12);

        // LED-style indicators
        // Power ON indicator (completed - filled circle with checkmark)
        display.fillCircle(32, 32, 6);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 24, 42);

        // Power OFF indicator with pulsing animation
        unsigned long pulse = (millis() / 300) % 2;
        if (pulse) {
            // Pulsing filled circle with larger radius
            display.fillCircle(96, 32, 7);
        } else {
            // Normal filled circle
            display.fillCircle(96, 32, 6);
        }
        display.print("OFF", 88, 42);

        // Compact timeout progress bar
        unsigned long elapsed = millis() - recordingStartTime;
        int progress = (elapsed * 100) / RECORDING_TIMEOUT;
        progress = constrain(progress, 0, 100);
        display.drawProgressBar(15, 56, 98, 6, progress, 100, false);
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
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(29, 33, 31, 35);
        display.drawLine(31, 35, 35, 31);
        display.setTextColor(1);  // Reset to white
        display.print("ON", 24, 42);

        // Power OFF indicator (completed - filled circle with checkmark)
        display.fillCircle(96, 32, 6);
        // Checkmark overlay
        display.setTextColor(0);  // Black for visibility on filled circle
        display.drawLine(93, 33, 95, 35);
        display.drawLine(95, 35, 99, 31);
        display.setTextColor(1);  // Reset to white
        display.print("OFF", 88, 42);

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

        // Show result based on whether codes matched
        if (firstCode != secondCode) {
            display.printCentered("Device saved", 14);
        }

        // Show protocol info with better centering
        display.setTextSize(1);
        String firstProtocol = typeToString(firstCode.getProtocol(), false);
        String secondProtocol = typeToString(secondCode.getProtocol(), false);

        if (firstCode == secondCode) {
            // Single device - show centered success icon and protocol
            display.drawCircle(64, 37, 8);
            display.drawLine(60, 38, 62, 40);
            display.drawLine(62, 40, 68, 34);
            display.printCentered("Protocol: " + firstProtocol, 54);
        } else {
            if (firstProtocol == secondProtocol) {
                // Same protocol - show centered success icon and protocol
                display.drawCircle(64, 37, 8);
                display.drawLine(60, 38, 62, 40);
                display.drawLine(62, 40, 68, 34);
                display.printCentered("Protocol: " + firstProtocol, 49);
            } else {
                // Different protocols - center the protocol info vertically
                display.printCentered("ON: " + firstProtocol, 32);
                display.printCentered("OFF: " + secondProtocol, 49);
            }
        }
    }

    void drawError() {
        // Draw title
        display.setTextSize(1);
        display.printCentered("Add Device", 0);

        // Draw horizontal line
        display.drawLine(0, 10, display.getWidth(), 10);

        display.setTextSize(1);
        display.printCentered("ERROR!", 16);

        // Compact X icon. Shrunk + lifted so the two-line reason below has
        // enough headroom to render its descenders without clipping the
        // bottom of the 64-pixel display.
        display.drawCircle(64, 32, 5);
        display.drawLine(61, 29, 67, 35);
        display.drawLine(61, 35, 67, 29);

        // Pick the right error explanation. There are three distinct cases:
        //   * recording timed out without any IR pulses → "No signal"
        //   * IR was received but the IRrecv library returned the UNKNOWN
        //     protocol → tell the user the *protocol* couldn't be classified,
        //     not that the receiver didn't see anything (the old message
        //     misled people into thinking the IR receiver was broken)
        //   * IR was received and classified, but somehow doesn't pass
        //     isValid() (e.g. zero bits) → show the protocol so we can debug
        const IRCode& failedCode = hasFirstCode ? secondCode : firstCode;
        String protocol = typeToString(failedCode.getProtocol(), false);
        bool unknownProtocol = (protocol == "Unknown");

        display.setTextSize(1);
        // Line 1 baseline lands at y~49, Line 2 baseline at y~59 (descenders
        // ~y=61) — both safely above the y=63 screen edge.
        if (isTimeoutError) {
            display.printCentered("Timeout", 43);
            display.printCentered("No signal received", 53);
        } else if (unknownProtocol) {
            display.printCentered("Unknown protocol", 43);
            display.printCentered("Try again", 53);
        } else {
            // Decoded but invalid for some other reason — surface the protocol
            // so the user (or we) can see what was actually captured.
            char line[24];
            snprintf(line, sizeof(line), "Got: %s", protocol.c_str());
            display.printCentered(line, 43);
            display.printCentered("Try again", 53);
        }
    }
};
