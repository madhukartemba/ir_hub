#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../preferences.h"
#include "../ui/MainMenu.h"

#ifndef FIRMWARE_VERSION
#  define FIRMWARE_VERSION "0.0.0"
#endif

class HomeScreen : public Screen {
   private:
    unsigned long lastActivityTime;
    const unsigned long STATUS_BLANK_TIMEOUT = 10000;  // 10 seconds for status screen blanking
    const unsigned long LED_BLANK_REFRESH_INTERVAL_MS = 600000;  // re-blank NeoRing when pixels unchanged
    bool isBlanked = false;
    unsigned long lastLedBlankRefresh = 0;
    unsigned long animationTimer = 0;
    int animationFrame = 0;

   public:
    void onEnter() override {
        LOG_DEBUG("[HomeScreen] onEnter");
        lastActivityTime = millis();
        isBlanked = false;
        animationTimer = millis();
        animationFrame = 0;
        ringColor();

        display.setFPS(1);

        button.setClickCallback([this]() {
            lastActivityTime = millis();

            if (isBlanked) {
                display.turnOn();
                ringColor();
                isBlanked = false;
                LOG_DEBUG("[HomeScreen] Screen turned back on from blanked state");
                return;
            }
        });

        button.setLongPressCallback([this]() {
            lastActivityTime = millis();

            // Long press could be used for system functions in the future
            LOG_DEBUG("[HomeScreen] onButtonLongPress");

            if (isBlanked) {
                display.turnOn();
                ringColor();
                isBlanked = false;
                LOG_DEBUG("[HomeScreen] Screen turned back on from blanked state");
                return;
            }

            LOG_DEBUG("[HomeScreen] onButtonClick - navigating to MainMenu");
            router.push(new MainMenu());
        });
    }

    void onUpdate() override {
        if (!isBlanked && (millis() - lastActivityTime) > STATUS_BLANK_TIMEOUT) {
            isBlanked = true;
            display.turnOff();
            ledRing.blank();
            lastLedBlankRefresh = millis();
            LOG_DEBUG("[HomeScreen] Screen blanked due to inactivity on status screen");
            return;  // Don't update display when blanked
        }

        if (isBlanked) {
            if (millis() - lastLedBlankRefresh >= LED_BLANK_REFRESH_INTERVAL_MS) {
                lastLedBlankRefresh = millis();
                ledRing.blank();
            }
            return;
        }

        if (millis() - animationTimer >= 1000) {
            animationFrame = (animationFrame + 1) % 4;
            animationTimer = millis();
        }

        display.clear();
        showBeautifulStatusScreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[HomeScreen] onExit");
        // Keep current ring color when leaving Home so the next screen can
        // transition smoothly instead of flashing through black.
        if (isBlanked) {
            ledRing.blank();
        }
        display.resetFPS();
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
        drawHeader();

        drawCenteredIP();

        drawUptime();
    }

    void drawHeader() {
        display.setTextSize(1);
        display.setTextColor(1);
        char title[20];
        snprintf(title, sizeof(title), "IR Hub v%s", FIRMWARE_VERSION);
        display.print(title, 2, 0);

        drawSignalBar();
    }

    void drawSignalBar() {
        bool connected = WiFi.status() == WL_CONNECTED;

        if (connected) {
            int rssi = WiFi.RSSI();
            int signalStrength = map(rssi, -100, -30, 1, 5);  // Map RSSI to 1-5 bars
            signalStrength = constrain(signalStrength, 1, 5);

            for (int i = 0; i < signalStrength; i++) {
                int barHeight = (i + 1) * 2;
                int barWidth = 3;
                int x = 108 + (i * 4);
                int y = 12 - barHeight;
                display.fillRect(x, y, barWidth, barHeight);
            }

        } else {
            display.setTextSize(1);
            display.setTextColor(1);
            display.print("X", 115, 4);
        }
    }

    void drawCenteredIP() {
        int cardWidth = 96;
        int cardHeight = 16;
        int cardX = (128 - cardWidth) / 2;  // Center the box horizontally
        int cardY = 30;                     // Moved down from 20 to 24

        display.drawRect(cardX, cardY, cardWidth, cardHeight);

        int tabWidth = 28;
        int tabHeight = 10;
        display.fillRect(cardX + 2, cardY - tabHeight, tabWidth, tabHeight);

        display.setTextSize(1);
        display.setTextColor(0);  // Black text on filled tab
        display.print("IP", cardX + 10, cardY - tabHeight);

        display.setTextSize(1);
        display.setTextColor(1);

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

        display.drawCircle(startX + 4, iconY, 4);
        display.fillCircle(startX + 4, iconY, 1);

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
