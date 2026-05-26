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
    enum BadgeState { BADGE_OFF, BADGE_PENDING, BADGE_ACTIVE, BADGE_OFFLINE };
    static constexpr bool FORCE_ALEXA_RETRY_DEMO = true;

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

        display.setFPS(2);

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
        const bool wifiConnected = WiFi.status() == WL_CONNECTED;
        const bool mqttConnected = mqttConnector.isConnected();
        const bool mqttEnabled = mqttConnector.isEnabled();

        BadgeState alexaState = BADGE_OFF;
        BadgeState mqttState = BADGE_OFF;

        if (!wifiConnected) {
            alexaState = BADGE_OFFLINE;
            mqttState = BADGE_OFFLINE;
        } else {
            alexaState = alexaConnector.isEnabled() ? BADGE_ACTIVE : BADGE_OFF;
            mqttState = mqttConnected ? BADGE_ACTIVE : (mqttEnabled ? BADGE_PENDING : BADGE_OFF);
            if (FORCE_ALEXA_RETRY_DEMO) {
                alexaState = BADGE_PENDING;  // demo override requested by user
            }
        }

        drawBadge(3, 14, 60, 36, "ALEXA", alexaState,
                  alexaState == BADGE_ACTIVE   ? "ONLINE"
                  : alexaState == BADGE_PENDING ? "RETRY"
                  : alexaState == BADGE_OFFLINE ? "OFFLINE"
                                                : "OFF");
        drawBadge(65, 14, 60, 36, "MQTT", mqttState,
                  mqttState == BADGE_ACTIVE   ? "ONLINE"
                  : mqttState == BADGE_PENDING ? "RETRY"
                  : mqttState == BADGE_OFFLINE ? "OFFLINE"
                                               : "OFF");
    }

    void drawBadge(int x, int y, int w, int h, const char* label, BadgeState state,
                   const char* value) {
        display.drawRect(x, y, w, h);

        display.setTextSize(1);
        display.setTextColor(1);
        display.print(label, x + 4, y + 4);

        int dotX = x + w - 8;
        int dotY = y + 6;
        // Home screen renders at 1 FPS, so animation steps must be >=1s to remain visible.
        bool blinkOn = ((millis() / 1000) % 2) == 0;
        if (state == BADGE_ACTIVE) {
            display.fillCircle(dotX, dotY, 2);
        } else if (state == BADGE_PENDING) {
            // Clear blink cue while reconnecting: alternate filled and outline dot.
            if (blinkOn) {
                display.fillCircle(dotX, dotY, 2);
            } else {
                display.drawCircle(dotX, dotY, 2);
            }
        } else if (state == BADGE_OFFLINE) {
            display.fillRect(dotX - 2, dotY - 1, 5, 3);
        } else {
            display.drawCircle(dotX, dotY, 2);
        }

        int valueX = x + (w - display.getTextWidth(value)) / 2;
        display.print(value, valueX, y + 18);

        drawCardActivityBar(x, y, w, h, state);
    }

    void drawCardActivityBar(int x, int y, int w, int h, BadgeState state) {
        int trackX = x + 3;
        int trackY = y + h - 5;
        int trackW = w - 6;
        int trackH = 3;

        display.drawRect(trackX, trackY, trackW, trackH);

        int fillX = trackX + 1;
        int fillY = trackY + 1;
        int fillMaxW = trackW - 2;
        if (fillMaxW <= 0) {
            return;
        }

        if (state == BADGE_ACTIVE) {
            display.fillRect(fillX, fillY, fillMaxW, 1);
        } else if (state == BADGE_PENDING) {
            // Tunnel-loop loader: exits right edge, disappears briefly, then emerges from left.
            int segmentW = fillMaxW / 3;
            if (segmentW < 2) segmentW = 2;
            if (segmentW > fillMaxW) segmentW = fillMaxW;

            int gapW = segmentW / 2;
            if (gapW < 2) gapW = 2;

            int cycle = fillMaxW + segmentW + gapW;
            int phase = (millis() / 90) % cycle;
            int segStart = phase - segmentW;  // enters from left off-screen
            int segEnd = segStart + segmentW;

            if (segEnd <= 0 || segStart >= fillMaxW) {
                return;  // fully inside "tunnel"
            }

            int visibleStart = segStart;
            if (visibleStart < 0) visibleStart = 0;
            int visibleEnd = segEnd;
            if (visibleEnd > fillMaxW) visibleEnd = fillMaxW;
            int visibleW = visibleEnd - visibleStart;
            if (visibleW <= 0) {
                return;
            }
            display.fillRect(fillX + visibleStart, fillY, visibleW, 1);
        } else if (state == BADGE_OFFLINE) {
            display.fillRect(fillX, fillY, fillMaxW, 1);
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
