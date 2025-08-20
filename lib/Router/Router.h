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

    void pop() { pop(1); }

    void pop(int count) {
        if (count <= 0) {
            LOG_WARN("Invalid pop count: %d", count);
            return;
        }

        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("Navigation blocked - current screen has paused navigation");
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
            LOG_WARN("No screens available to pop");
            return;
        }

        // Pop the calculated number of screens
        for (int i = 0; i < maxPops && !screenStack.empty(); i++) {
            Screen* top = screenStack.top();
            screenStack.pop();
            LOG_INFO("Popped screen %d from stack", i + 1);
            top->onExit();  // Call onExit before deleting
            delete top;
            poppedCount++;
        }

        if (poppedCount < count) {
            LOG_WARN("Requested to pop %d screens but only popped %d", count, poppedCount);
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("Screen stack not empty after pop");
            screenStack.top()->onEnter();
            // Screen is now active again
        } else {
            LOG_INFO("Screen stack is now empty");
            // Stack is empty, show default screen if available
            if (defaultScreen != nullptr) {
                LOG_DEBUG("Setting default screen after stack became empty");
                push(defaultScreen);
            } else {
                LOG_WARN("No default screen available after stack became empty");
            }
        }
    }

    void replace(Screen* screen) {
        if (!screenStack.empty() && screenStack.top()->isNavigationPaused()) {
            LOG_WARN("Navigation blocked - current screen has paused navigation");
            return;  // Don't navigate if current screen has paused navigation
        }

        if (!screenStack.empty()) {
            LOG_DEBUG("Replacing current screen");
            Screen* top = screenStack.top();
            screenStack.pop();
            top->onExit();  // Call onExit before deleting
            delete top;
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
            Screen* top = screenStack.top();
            screenStack.pop();
            top->onExit();  // Call onExit before deleting
            delete top;
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
