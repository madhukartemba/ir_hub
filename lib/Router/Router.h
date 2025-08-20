#include <Arduino.h>
#include <functional>
#include <stack>
#include "../Log/Log.h"
#include "../Screen/Screen.h"

class Router {
   private:
    std::stack<Screen*> screenStack;
    Screen* defaultScreen;

    // Timeout functionality
    unsigned long timeoutDuration;      // Timeout duration in milliseconds
    unsigned long lastScreenEnterTime;  // When the current screen was entered
    bool timeoutEnabled;                // Whether timeout is enabled
    bool isDefaultScreen;               // Track if current screen is the default screen

    // Activity monitoring callback
    std::function<unsigned long()> activityCallback;  // Callback to get last activity time

   public:
    Router()
        : defaultScreen(nullptr),
          timeoutDuration(30000),
          lastScreenEnterTime(0),
          timeoutEnabled(false),
          isDefaultScreen(false),
          activityCallback(nullptr) {
        LOG_DEBUG("[Router] Router initialized with timeout enabled");
    }

    void setDefaultScreen(Screen* screen) {
        defaultScreen = screen;
        if (screenStack.empty()) {
            isDefaultScreen = true;
            push(screen);
        }
        LOG_INFO("[Router] Default screen set");
    }

    // Activity monitoring setup
    void setActivityCallback(std::function<unsigned long()> callback) {
        activityCallback = callback;
        LOG_INFO("[Router] Activity callback set");
    }

    // Timeout configuration methods
    void setTimeoutDuration(unsigned long duration) {
        timeoutDuration = duration;
        LOG_INFO("[Router] Timeout duration set to %lu ms", duration);
    }

    unsigned long getTimeoutDuration() const { return timeoutDuration; }

    void enableTimeout(bool enable) {
        timeoutEnabled = enable;
        LOG_INFO("[Router] Timeout %s", enable ? "enabled" : "disabled");
    }

    bool isTimeoutEnabled() const { return timeoutEnabled; }

    void resetTimeout() {
        lastScreenEnterTime = millis();
        LOG_DEBUG("[Router] Timeout reset");
    }

    // Check for activity and reset timeout if needed
    void checkActivity() {
        if (activityCallback && timeoutEnabled && !isDefaultScreen) {
            unsigned long lastActivity = activityCallback();
            if (lastActivity > lastScreenEnterTime) {
                LOG_DEBUG("[Router] Activity detected, resetting timeout");
                resetTimeout();
            }
        }
    }

    void push(Screen* screen) {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("[Router] Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("[Router] Exiting current screen before push");
            screenStack.top()->onExit();
        }

        screenStack.push(screen);
        LOG_INFO("[Router] Pushed new screen to stack");

        // Update timeout tracking
        lastScreenEnterTime = millis();
        isDefaultScreen = (screen == defaultScreen);

        screen->onEnter();  // Optional
    }

    void pop() { pop(1); }

    void pop(int count) {
        if (count <= 0) {
            LOG_WARN("[Router] Invalid pop count: %d", count);
            return;
        }

        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("[Router] Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        int poppedCount = 0;
        int maxPops = count;

        // Calculate how many screens we can actually pop
        // We need to keep at least the default screen
        if (defaultScreen != nullptr) {
            int nonDefaultScreens = 0;
            std::stack<Screen*> tempStack = screenStack;
            while (!tempStack.empty() && tempStack.top() != defaultScreen) {
                nonDefaultScreens++;
                tempStack.pop();
            }
            maxPops = std::min(count, nonDefaultScreens);
        }

        if (maxPops == 0) {
            LOG_WARN("[Router] No screens available to pop");
            return;
        }

        // Pop the calculated number of screens
        for (int i = 0; i < maxPops && !screenStack.empty(); i++) {
            Screen* top = screenStack.top();
            screenStack.pop();
            LOG_INFO("[Router] Popped screen %d from stack", i + 1);
            top->onExit();  // Call onExit before deleting
            delete top;
            poppedCount++;
        }

        if (poppedCount < count) {
            LOG_WARN("[Router] Requested to pop %d screens but only popped %d", count, poppedCount);
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("[Router] Screen stack not empty after pop");
            screenStack.top()->onEnter();
            // Screen is now active again
        } else {
            LOG_INFO("[Router] Screen stack is now empty");
            // Stack is empty, show default screen if available
            if (defaultScreen != nullptr) {
                LOG_DEBUG("[Router] Setting default screen after stack became empty");
                push(defaultScreen);
            } else {
                LOG_WARN("[Router] No default screen available after stack became empty");
            }
        }
    }

    void replace(Screen* screen) {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("[Router] Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("[Router] Replacing current screen");
            Screen* top = screenStack.top();
            screenStack.pop();
            top->onExit();  // Call onExit before deleting
            delete top;
        } else {
            LOG_DEBUG("[Router] Replacing empty screen stack");
        }

        push(screen);
        LOG_INFO("[Router] Screen replaced successfully");
    }

    void clear() {
        // Clear operation should always work regardless of pause state
        LOG_INFO("[Router] Clearing screen stack");
        int clearedCount = 0;
        while (!screenStack.empty()) {
            Screen* top = screenStack.top();
            screenStack.pop();
            top->onExit();  // Call onExit before deleting
            delete top;
            clearedCount++;
        }
        LOG_INFO("[Router] Cleared %d screens from stack", clearedCount);

        // Set default screen if available
        if (defaultScreen != nullptr) {
            LOG_DEBUG("[Router] Setting default screen after clear");
            push(defaultScreen);
        } else {
            LOG_WARN("[Router] No default screen available after clear");
        }
    }

    void update() {
        if (!screenStack.empty()) {
            // Check for activity and reset timeout if needed
            checkActivity();

            // Check for timeout
            if (timeoutEnabled && !isDefaultScreen && defaultScreen != nullptr) {
                unsigned long currentTime = millis();
                unsigned long timeSinceEnter = currentTime - lastScreenEnterTime;

                if (timeSinceEnter >= timeoutDuration) {
                    LOG_INFO("[Router] Timeout reached (%lu ms), returning to default screen",
                             timeoutDuration);
                    returnToDefaultScreen();
                    return;
                }
            }

            screenStack.top()->onUpdate();
        } else {
            LOG_ERROR("[Router] Attempted to update with empty screen stack");
        }
    }

    // Method to manually return to default screen
    void returnToDefaultScreen() {
        if (defaultScreen == nullptr) {
            LOG_WARN("[Router] No default screen available for timeout return");
            return;
        }

        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("[Router] Navigation blocked - current screen has paused navigation");
            return;
        }

        LOG_INFO("[Router] Returning to default screen");

        // Use clear() to remove all screens and push defaultScreen
        clear();

        // After clear(), defaultScreen should be on top if it was set
        // Reset timeout for default screen
        resetTimeout();
        isDefaultScreen = true;
    }
};
