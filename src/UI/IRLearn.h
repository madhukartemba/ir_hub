#include "global/Global.h"

class IRLearn : public Screen {

    private:
        enum class State {
            SINGLE_CODE,
            DUAL_CODE,
            RECORDING,
            SUCCESS,
            BACK,
        };

        enum class RecordingMode {
            SINGLE,
            DUAL,
        };

        State currentState;
        RecordingMode recordingMode;

    public:
        void onEnter() override {
            LOG_DEBUG("IRLearn onEnter");
            button.setClickCallback([this]() {
                LOG_DEBUG("IRLearn onButtonClick");
                switch (currentState) {
                    case State::SINGLE_CODE:
                        currentState = State::DUAL_CODE;
                        break;
                    case State::DUAL_CODE:
                        currentState = State::BACK;
                        break;
                    case State::BACK:
                        currentState = State::SINGLE_CODE;
                        break;
                }
            });
            button.setLongPressCallback([this]() {
                LOG_DEBUG("IRLearn onButtonLongPress");
                switch (currentState) {
                    case State::SINGLE_CODE:
                        recordingMode = RecordingMode::SINGLE;
                        currentState = State::RECORDING;
                        break;
                    case State::DUAL_CODE:
                        recordingMode = RecordingMode::DUAL;
                        currentState = State::RECORDING;
                        break;
                    case State::RECORDING:
                        currentState = State::BACK;
                        break;
                    case State::SUCCESS:
                        router.pop();
                        break;
                    case State::BACK:
                        router.pop();
                        break;
                }
            });
        }

        void onUpdate() override {
            switch (currentState) {
                case State::SINGLE_CODE:
                    drawSingleCode();
                    break;
                case State::DUAL_CODE:
                    drawDualCode();
                    break;
                case State::RECORDING:
                    drawRecording();
                    break;
                case State::SUCCESS:
                    drawSuccess();
                    break;
                case State::BACK:
                    drawBack();
                    break;
            }
        }

        void onExit() override {
            LOG_DEBUG("IRLearn onExit");
        }

        void drawSingleCode() {
            
        }

        void drawDualCode() {
            
        }

        void drawRecording() {
            
        }

        void drawSuccess() {
            
        }

        void drawBack() {
            
        }
};