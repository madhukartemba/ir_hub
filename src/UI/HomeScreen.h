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
    enum BadgeState { BADGE_OFF, BADGE_PENDING, BADGE_ACTIVE };

    unsigned long lastActivityTime;
    const unsigned long STATUS_BLANK_TIMEOUT = 10000;  // 10 seconds for status screen blanking
    const unsigned long LED_BLANK_REFRESH_INTERVAL_MS = 600000;  // re-blank NeoRing when pixels unchanged
    bool isBlanked = false;
    unsigned long lastLedBlankRefresh = 0;

   public:
    void onEnter() override {
        LOG_DEBUG("[HomeScreen] onEnter");
        lastActivityTime = millis();
        isBlanked = false;
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
        drawServiceCards();
        drawQuickStatsRow();
    }

    void drawHeader() {
        display.setTextSize(1);
        display.setTextColor(1);
        display.print("IR Hub", 2, 0);
        drawVersionTag();
        drawSignalBar();
        // Keep a guaranteed 1px vertical gap below the tallest signal bar.
        display.drawLine(0, 12, 127, 12);
    }

    void drawVersionTag() {
        char version[12];
        snprintf(version, sizeof(version), "v%s", FIRMWARE_VERSION);
        int w = display.getTextWidth(version);
        int x = (128 - w) / 2;
        display.print(version, x, 0);
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
                int y = 11 - barHeight;
                display.fillRect(x, y, barWidth, barHeight);
            }

        } else {
            display.setTextSize(1);
            display.setTextColor(1);
            display.print("X", 115, 3);
        }
    }

    void drawServiceCards() {
        const bool mqttConnected = mqttConnector.isConnected();
        const bool mqttEnabled = mqttConnector.isEnabled();

        BadgeState alexaState = alexaConnector.isEnabled() ? BADGE_ACTIVE : BADGE_OFF;
        BadgeState mqttState = mqttConnected ? BADGE_ACTIVE : (mqttEnabled ? BADGE_PENDING : BADGE_OFF);

        drawBadge(3, 14, 60, 36, "ALEXA", alexaState,
                  alexaState == BADGE_ACTIVE ? "ONLINE" : "OFF");
        drawBadge(65, 14, 60, 36, "MQTT", mqttState,
                  mqttState == BADGE_ACTIVE ? "ONLINE"
                  : (mqttState == BADGE_PENDING ? "RETRY" : "OFF"));
    }

    void drawBadge(int x, int y, int w, int h, const char* label, BadgeState state,
                   const char* value) {
        display.drawRect(x, y, w, h);

        display.setTextSize(1);
        display.setTextColor(1);
        display.print(label, x + 4, y + 4);

        int dotX = x + w - 8;
        int dotY = y + 6;
        if (state == BADGE_ACTIVE) {
            display.fillCircle(dotX, dotY, 2);
        } else {
            display.drawCircle(dotX, dotY, 2);
            if (state == BADGE_PENDING && ((millis() / 400) % 2 == 0)) {
                display.fillCircle(dotX, dotY, 1);
            }
        }

        int valueX = x + (w - display.getTextWidth(value)) / 2;
        display.print(value, valueX, y + 18);

        drawCardActivityBar(x, y, w, h, state);
    }

    void drawCardActivityBar(int x, int y, int w, int h, BadgeState state) {
        int barX = x + 3;
        int barY = y + h - 4;
        int barW = w - 6;

        if (state == BADGE_ACTIVE) {
            display.fillRect(barX, barY, barW, 2);
        } else if (state == BADGE_PENDING) {
            int pulse = ((millis() / 250) % 4) + 1;
            int pulseW = (barW * pulse) / 4;
            display.drawRect(barX, barY, barW, 2);
            display.fillRect(barX, barY, pulseW, 2);
        } else {
            display.drawRect(barX, barY, barW, 2);
        }
    }

    void drawQuickStatsRow() {
        const bool wifiConnected = WiFi.status() == WL_CONNECTED;
        const char* wifiLabel = wifiConnected ? "WiFi: OK" : "WiFi: NC";
        display.drawLine(0, 51, 127, 51);
        display.print(wifiLabel, 2, 54);

        char deviceLabel[24];
        snprintf(deviceLabel, sizeof(deviceLabel), "Devices: %u", (unsigned)deviceManager.deviceCount());
        int devX = 126 - display.getTextWidth(deviceLabel);
        display.print(deviceLabel, devX, 54);
    }
};
