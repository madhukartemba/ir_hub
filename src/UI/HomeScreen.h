#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../preferences.h"
#include "../ui/MainMenu.h"

class HomeScreen : public Screen {
   private:
    unsigned long lastActivityTime;
    const unsigned long STATUS_BLANK_TIMEOUT = 10000;  // 10 seconds for status screen blanking
    /// Re-send black while blanked: NeoRing only pushes pixels to the strip when the frame
    /// changes; static black stops updating after the first frame, so flaky LEDs can stay lit.
    /// Interval must exceed the library fade duration (~1s) so we do not queue many animations.
    const unsigned long LED_BLANK_REFRESH_INTERVAL_MS = 600000;
    bool isBlanked = false;
    unsigned long lastLedBlankRefresh = 0;
    unsigned long animationTimer = 0;
    int animationFrame = 0;

   public:
    void onEnter() override {
        LOG_DEBUG("HomeScreen onEnter");
        lastActivityTime = millis();
        isBlanked = false;
        animationTimer = millis();
        animationFrame = 0;
        ringColor();

        // Change refresh rate to 1 FPS for status screen
        display.setFPS(1);

        // Change button behavior
        button.setClickCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // If screen is blanked, turn it back on
            if (isBlanked) {
                display.turnOn();
                ringColor();
                isBlanked = false;
                LOG_DEBUG("Screen turned back on from blanked state");
                return;
            }
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // Long press could be used for system functions in the future
            LOG_DEBUG("HomeScreen onButtonLongPress");

            // If screen is blanked, turn it back on
            if (isBlanked) {
                display.turnOn();
                ringColor();
                isBlanked = false;
                LOG_DEBUG("Screen turned back on from blanked state");
                return;
            }

            // Navigate to main menu
            LOG_DEBUG("HomeScreen onButtonClick - navigating to MainMenu");
            router.push(new MainMenu());
        });
    }

    void onUpdate() override {
        // Check for status screen blanking timeout
        if (!isBlanked && (millis() - lastActivityTime) > STATUS_BLANK_TIMEOUT) {
            isBlanked = true;
            display.turnOff();
            ledRing.blank();
            lastLedBlankRefresh = millis();
            LOG_DEBUG("Screen blanked due to inactivity on status screen");
            return;  // Don't update display when blanked
        }

        // Don't update display when blanked; keep re-driving the strip with black for bad pixels
        if (isBlanked) {
            if (millis() - lastLedBlankRefresh >= LED_BLANK_REFRESH_INTERVAL_MS) {
                lastLedBlankRefresh = millis();
                ledRing.blank();
            }
            return;
        }

        // Update animation frame every 500ms
        if (millis() - animationTimer > 500) {
            animationFrame = (animationFrame + 1) % 4;
            animationTimer = millis();
        }

        // Update display
        display.clear();
        showBeautifulStatusScreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("HomeScreen onExit");
        ledRing.blank();
        // Restore display FPS to default
        display.resetFPS();
        // Make sure display is turned on when exiting
        display.turnOn();
    }

   private:
    void ringColor() {
        if (wifiManager.isConnected()) {
            ledRing.wave(COLOR_HOME_SCREEN_WIFI_CONNECTED);
        } else {
            ledRing.wave(COLOR_HOME_SCREEN_WIFI_DISCONNECTED);
        }
    }

    void showBeautifulStatusScreen() {
        // Draw clean header with signal bar
        drawHeader();

        // Draw centered IP address
        drawCenteredIP();

        // Draw uptime at bottom
        drawUptime();
    }

    void drawHeader() {
        // Draw title in top left
        display.setTextSize(1);
        display.setTextColor(1);  // White text
        display.print("IR Hub", 2, 0);

        // Draw top signal bar
        drawSignalBar();
    }

    void drawSignalBar() {
        bool connected = WiFi.status() == WL_CONNECTED;

        if (connected) {
            int rssi = WiFi.RSSI();
            int signalStrength = map(rssi, -100, -30, 1, 5);  // Map RSSI to 1-5 bars
            signalStrength = constrain(signalStrength, 1, 5);

            // Draw signal bars on the right
            for (int i = 0; i < signalStrength; i++) {
                int barHeight = (i + 1) * 2;
                int barWidth = 3;
                int x = 108 + (i * 4);
                int y = 12 - barHeight;
                display.fillRect(x, y, barWidth, barHeight);
            }

        } else {
            // Draw disconnected state on the right
            display.setTextSize(1);
            display.setTextColor(1);
            display.print("X", 115, 4);
        }
    }

    void drawCenteredIP() {
        // Draw smaller folder-like card with IP information
        int cardWidth = 96;
        int cardHeight = 16;
        int cardX = (128 - cardWidth) / 2;  // Center the box horizontally
        int cardY = 30;                     // Moved down from 20 to 24

        // Draw card outline
        display.drawRect(cardX, cardY, cardWidth, cardHeight);

        // Draw modern folder tab with "IP" text inside
        int tabWidth = 28;
        int tabHeight = 10;
        display.fillRect(cardX + 2, cardY - tabHeight, tabWidth, tabHeight);

        // Draw "IP" text in the tab
        display.setTextSize(1);
        display.setTextColor(0);  // Black text on filled tab
        display.print("IP", cardX + 10, cardY - tabHeight);

        display.setTextSize(1);
        display.setTextColor(1);

        // Format directly into a stack buffer to avoid the per-frame `String`
        // heap allocations that `WiFi.localIP().toString()` would cause.
        char body[20];
        if (WiFi.status() == WL_CONNECTED) {
            IPAddress ip = WiFi.localIP();
            snprintf(body, sizeof(body), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        } else {
            strncpy(body, "Not Connected", sizeof(body));
            body[sizeof(body) - 1] = '\0';
        }

        int textWidth = display.getTextWidth(body);
        int textHeight = display.getTextHeight();
        int textX = cardX + (cardWidth - textWidth) / 2;
        int textY = cardY + (cardHeight - textHeight) / 2 + 1;
        display.print(body, textX, textY);
    }

    void drawUptime() {
        int startX = 22;
        // Position at the very bottom (64 - 16 = 48 for text baseline, 50 for icon center)
        int iconY = 58;  // Bottom area for icon
        int textY = 53;  // Bottom area for text baseline

        // Draw clock icon
        display.drawCircle(startX + 4, iconY, 4);
        display.fillCircle(startX + 4, iconY, 1);

        // Animate clock hands through all 4 positions
        switch (animationFrame) {
            case 0:  // 12 o'clock
                display.drawLine(startX + 4, iconY, startX + 4, iconY - 4);
                break;
            case 1:  // 3 o'clock
                display.drawLine(startX + 4, iconY, startX + 8, iconY);
                break;
            case 2:  // 6 o'clock
                display.drawLine(startX + 4, iconY, startX + 4, iconY + 4);
                break;
            case 3:  // 9 o'clock
                display.drawLine(startX + 4, iconY, startX, iconY);
                break;
        }

        // Format the uptime directly into a fixed stack buffer to avoid the
        // ~7 `String` heap allocations the previous implementation made every
        // second. Over days that was a large source of heap fragmentation.
        char buf[24];
        unsigned long uptime = millis() / 1000;
        unsigned long hours = uptime / 3600;
        unsigned long minutes = (uptime % 3600) / 60;
        unsigned long seconds = uptime % 60;
        snprintf(buf, sizeof(buf), "Uptime %lu:%02lu:%02lu", hours, minutes, seconds);

        display.setTextSize(1);
        display.print(buf, startX + 12, textY);
    }
};
