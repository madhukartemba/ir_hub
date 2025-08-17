#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../ui/MainMenu.h"

class HomeScreen : public Screen {
   private:
    unsigned long lastActivityTime;
    const unsigned long INACTIVITY_TIMEOUT = 30000;    // 30 seconds to show status
    const unsigned long STATUS_BLANK_TIMEOUT = 10000;  // 10 seconds for status screen blanking
    bool isBlanked = false;
    unsigned long animationTimer = 0;
    int animationFrame = 0;

   public:
    void onEnter() override {
        LOG_DEBUG("HomeScreen onEnter");
        lastActivityTime = millis();
        isBlanked = false;
        animationTimer = millis();
        animationFrame = 0;

        // Change button behavior
        button.setClickCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // If screen is blanked, turn it back on
            if (isBlanked) {
                display.turnOn();
                isBlanked = false;
                LOG_DEBUG("Screen turned back on from blanked state");
                return;
            }

            // Navigate to main menu
            LOG_DEBUG("HomeScreen onButtonClick - navigating to MainMenu");
            router.push(new MainMenu());
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // If screen is blanked, turn it back on
            if (isBlanked) {
                display.turnOn();
                isBlanked = false;
                LOG_DEBUG("Screen turned back on from blanked state");
                return;
            }

            // Long press could be used for system functions in the future
            LOG_DEBUG("HomeScreen onButtonLongPress");
        });
    }

    void onUpdate() override {
        // Check for status screen blanking timeout
        if (!isBlanked && (millis() - lastActivityTime) > STATUS_BLANK_TIMEOUT) {
            isBlanked = true;
            display.turnOff();
            LOG_DEBUG("Screen blanked due to inactivity on status screen");
            return;  // Don't update display when blanked
        }

        // Don't update display when blanked
        if (isBlanked) {
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
        // Make sure display is turned on when exiting
        display.turnOn();
    }

   private:
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
        display.print("IR Hub", 2, 4);

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
            display.drawRect(108, 2, 20, 8);
            display.drawLine(110, 4, 126, 10);
            display.drawLine(110, 10, 126, 4);
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
        int tabHeight = 8;
        display.fillRect(cardX + 2, cardY - tabHeight, tabWidth, tabHeight);

        // Draw "IP" text in the tab
        display.setTextSize(1);
        display.setTextColor(0);  // Black text on filled tab
        display.print("IP", cardX + 6, cardY - tabHeight + 1);

        if (WiFi.status() == WL_CONNECTED) {
            String ip = WiFi.localIP().toString();

            // Draw IP address perfectly centered within the card (both horizontally and vertically)
            display.setTextSize(1);
            display.setTextColor(1);  // White text on card
            int textWidth = display.getTextWidth(ip);
            int textHeight = display.getTextHeight();
            int textX = cardX + (cardWidth - textWidth) / 2;
            int textY = cardY + (cardHeight - textHeight) / 2;
            display.print(ip, textX, textY);

        } else {
            // Show disconnected message perfectly centered within the card (both horizontally and
            // vertically)
            display.setTextSize(1);
            display.setTextColor(1);
            String message = "Not Connected";
            int textWidth = display.getTextWidth(message);
            int textHeight = display.getTextHeight();
            int textX = cardX + (cardWidth - textWidth) / 2;
            int textY = cardY + (cardHeight - textHeight) / 2;
            display.print(message, textX, textY);
        }
    }

    void drawUptime() {
        // Draw uptime at bottom
        String uptimeStr = getFormattedUptime();
        String fullText = "Uptime " + uptimeStr;

        // Calculate text width (approximately 6 pixels per character for text size 1)
        int textWidth = fullText.length() * 6;

        // Calculate center position for the entire uptime section
        // Clock icon is 8x8 pixels, plus some spacing
        int totalWidth = 8 + 4 + textWidth;   // icon + spacing + text
        int startX = (128 - totalWidth) / 2;  // Center the entire section

        // Position at the very bottom (64 - 16 = 48 for text baseline, 50 for icon center)
        int iconY = 58;  // Bottom area for icon
        int textY = 56;  // Bottom area for text baseline

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

        // Draw uptime text
        display.setTextSize(1);
        display.print(fullText, startX + 12, textY);
    }

    String getFormattedUptime() {
        unsigned long uptime = millis() / 1000;  // Convert to seconds
        unsigned long hours = uptime / 3600;
        unsigned long minutes = (uptime % 3600) / 60;
        unsigned long seconds = uptime % 60;

        return String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" +
               (seconds < 10 ? "0" : "") + String(seconds);
    }
};
