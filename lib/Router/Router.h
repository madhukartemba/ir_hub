#include <Arduino.h>
#include <stack>
#include "../Log/Log.h"
#include "../Screen/Screen.h"

class Router {
   private:
    std::stack<Screen*> screenStack;
    Screen* defaultScreen;

   public:
    Router() : defaultScreen(nullptr) { LOG_DEBUG("Router initialized"); }

    void setDefaultScreen(Screen* screen) {
        defaultScreen = screen;
        if (screenStack.empty()) {
            push(screen);
        }
        LOG_INFO("Default screen set");
    }

    void push(Screen* screen) {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("Exiting current screen before push");
            screenStack.top()->onExit();
        }

        screenStack.push(screen);
        LOG_INFO("Pushed new screen to stack");
        screen->onEnter();  // Optional
    }

    void pop() {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            Screen* top = screenStack.top();

            // Don't pop the default screen
            if (top == defaultScreen) {
                LOG_WARN("Attempted to pop default screen - operation blocked");
                return;
            }

            screenStack.pop();
            LOG_INFO("Popped screen from stack");
            delete top;
        } else {
            LOG_WARN("Attempted to pop from empty screen stack");
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("Screen stack not empty after pop");
            screenStack.top()->onEnter();
            // Screen is now active again
        } else {
            LOG_INFO("Screen stack is now empty");
            // Stack is empty, could set default screen here if needed
        }
    }

    void replace(Screen* screen) {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("Replacing current screen");
            delete screenStack.top();
            screenStack.pop();
        } else {
            LOG_DEBUG("Replacing empty screen stack");
        }

        push(screen);
        LOG_INFO("Screen replaced successfully");
    }

    void clear() {
        // Clear operation should always work regardless of pause state
        LOG_INFO("Clearing screen stack");
        int clearedCount = 0;
        while (!screenStack.empty()) {
            delete screenStack.top();
            screenStack.pop();
            clearedCount++;
        }
        LOG_INFO("Cleared %d screens from stack", clearedCount);

        // Set default screen if available
        if (defaultScreen != nullptr) {
            LOG_DEBUG("Setting default screen after clear");
            push(defaultScreen);
        } else {
            LOG_WARN("No default screen available after clear");
        }
    }

    void update() {
        if (!screenStack.empty()) {
            screenStack.top()->onUpdate();
        } else {
            LOG_ERROR("Attempted to update with empty screen stack");
        }
    }
};
