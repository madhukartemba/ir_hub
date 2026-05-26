#include <Arduino.h>
#include <functional>
#include <stack>
#include "../Log/Log.h"
#include "../Screen/Screen.h"

class Router {
   private:
    std::stack<Screen*> screenStack;
    Screen* defaultScreen;

    unsigned long timeoutDuration;
    unsigned long lastScreenEnterTime;
    bool timeoutEnabled;
    bool isDefaultScreen;
    std::function<unsigned long()> activityCallback;

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

    void setActivityCallback(std::function<unsigned long()> callback) {
        activityCallback = callback;
        LOG_INFO("[Router] Activity callback set");
    }

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

    void resetTimeout() { lastScreenEnterTime = millis(); }

    void checkActivity() {
        if (activityCallback && timeoutEnabled && !isDefaultScreen) {
            unsigned long lastActivity = activityCallback();
            if (lastActivity > lastScreenEnterTime) {
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
        } else {
            LOG_INFO("[Router] Screen stack is now empty");
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
        LOG_INFO("[Router] Clearing screen stack");
        int count = screenStack.size();
        if (defaultScreen != nullptr) {
            count--;
        }
        pop(count);  // Pop all screens except the default screen
    }

    void update() {
        if (!screenStack.empty()) {
            checkActivity();

            if (screenStack.top()->isNavigationPaused()) {
                screenStack.top()->onUpdate();
                return;
            }

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

        clear();
        resetTimeout();
        isDefaultScreen = true;
    }
};
