#pragma once

#include <qrcode.h>
#include "../../global/Global.h"
#include "UserPrefs.h"
#include "../../preferences.h"

class SetupOnboardingScreen : public Screen {
   private:
    enum class Page {
        CONNECT_AP_TEXT,
        CONNECT_AP_QR,
        COMPLETE_SETUP_TEXT,
        HELP_QR_FULL,
        COUNT,
    };

    static constexpr const char* kHelpUrl = "https://ir-hub.pages.dev/help";
    static constexpr uint8_t kQrVersion = 3;
    static constexpr uint8_t kQrQuietZone = 1;
    static constexpr unsigned long kPageRotateMs = 9000;

    Page currentPage = Page::CONNECT_AP_TEXT;
    unsigned long pageStartedAt = 0;
    bool autoAdvanceEnabled = true;
    bool timeoutVisualStateApplied = false;
    bool continueOfflineSelected = true;  // default: continue without Wi-Fi
    bool finishing = false;
    unsigned long finishingAt = 0;

   public:
    void onEnter() override {
        LOG_DEBUG("[SetupOnboarding] onEnter");
        setNavigationPaused(true);
        finishing = false;
        currentPage = Page::CONNECT_AP_TEXT;
        pageStartedAt = millis();
        autoAdvanceEnabled = true;
        timeoutVisualStateApplied = false;
        continueOfflineSelected = true;
        // Slow spinner to keep onboarding calmer and less distracting.
        ledRing.spinner(8, COLOR_INFO, 0.30f);

        button.setClickCallback([this]() {
            if (!wifiManager.isSetupInProgress()) {
                continueOfflineSelected = !continueOfflineSelected;
                return;
            }
            autoAdvanceEnabled = false;
            advancePage();
        });
        button.setLongPressCallback([this]() {
            // Allow explicit exit only after setup flow has ended without Wi-Fi.
            if (wifiManager.didSetupTimeout()) {
                if (continueOfflineSelected) {
                    proceedWithoutWiFiAndRestart();
                } else {
                    retrySetupAndRestart();
                }
                return;
            }
            autoAdvanceEnabled = false;
            advancePage();
        });
    }

    void onUpdate() override {
        if (wifiManager.isConnected()) {
            if (!finishing) {
                finishing = true;
                finishingAt = millis();
                userPrefsSetSkipWiFiSetup(false);
                ledRing.solid(COLOR_SUCCESS);
                speaker.successBeep();
            }
            drawConnected();
            display.update();
            if (millis() - finishingAt > 1200) {
                leaveScreen();
            }
            return;
        }

        if (!wifiManager.isSetupInProgress()) {
            if (!timeoutVisualStateApplied) {
                // Apply once; repeatedly queuing breathe() every frame can fragment heap.
                ledRing.breathe(COLOR_WARNING);
                timeoutVisualStateApplied = true;
            }
            drawTimedOut();
            display.update();
            return;
        }

        timeoutVisualStateApplied = false;

        if (autoAdvanceEnabled && millis() - pageStartedAt >= kPageRotateMs) {
            advancePage();
        }

        display.clear();
        switch (currentPage) {
            case Page::CONNECT_AP_TEXT:
                drawConnectApText();
                break;
            case Page::CONNECT_AP_QR:
                drawConnectApQr();
                break;
            case Page::COMPLETE_SETUP_TEXT:
                drawCompleteSetupText();
                break;
            case Page::HELP_QR_FULL:
                drawHelpQrFull();
                break;
            case Page::COUNT:
                break;
        }
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[SetupOnboarding] onExit");
        setNavigationPaused(false);
    }

   private:
    void leaveScreen() {
        setNavigationPaused(false);
        router.pop();
    }

    void advancePage() {
        int next = (static_cast<int>(currentPage) + 1) % static_cast<int>(Page::COUNT);
        currentPage = static_cast<Page>(next);
        pageStartedAt = millis();
    }

    void drawHeader(const char* title) {
        display.setTextSize(1);
        display.printCentered(title, 0);
        display.drawLine(0, 12, display.getWidth(), 12);
    }

    void drawConnectApText() {
        drawHeader("Setup");
        display.printCentered("Join IR Hub AP", 20);
        display.printCentered(wifiManager.setupApName(), 32);
        display.printCentered("Next page has WiFi QR", 52);
    }

    String wifiApQrPayload() const {
        // Wi-Fi QR for open AP (no password). Captive portal should auto-open after join.
        String payload = "WIFI:T:nopass;S:";
        payload += wifiManager.setupApName();
        payload += ";;";
        return payload;
    }

    void drawConnectApQr() {
        drawQrCodeFullscreen(wifiApQrPayload().c_str());
        display.setTextSize(1);
        display.print("WIFI", 2, 0);
    }

    void drawCompleteSetupText() {
        drawHeader("Complete Setup");
        display.printCentered("Open: 192.168.4.1", 20);
        display.printCentered("Pick your home Wi-Fi", 32);
        display.printCentered("MQTT is optional", 52);
    }

    void drawHelpQrFull() {
        drawQrCodeFullscreen(kHelpUrl);
        display.setTextSize(1);
        display.print("HELP", 2, 0);
    }

    void drawTimedOut() {
        display.clear();
        drawHeader("Setup Paused");
        display.printCentered("Proceed without Wi-Fi?", 14);
        display.printCenteredSelectable("Try setup again", 30, !continueOfflineSelected, 10, 4, 2,
                                        1);
        display.printCenteredSelectable("Continue offline", 49, continueOfflineSelected, 10, 4, 2,
                                        1);
    }

    void proceedWithoutWiFiAndRestart() {
        userPrefsSetSkipWiFiSetup(true);
        display.drawBrandStatus("Continuing offline");
        display.update();
        speaker.shortBeep();
        delay(600);
        ESP.restart();
    }

    void retrySetupAndRestart() {
        userPrefsSetSkipWiFiSetup(false);
        display.drawBrandStatus("Restarting setup");
        display.update();
        speaker.shortBeep();
        delay(600);
        ESP.restart();
    }

    void drawConnected() {
        display.drawBrandStatus2("Wi-Fi connected!", "Finishing setup...");
    }

    void drawQrCodeFullscreen(const char* text) {
        uint8_t qrcodeData[qrcode_getBufferSize(kQrVersion)];
        QRCode qrcode;
        int8_t rc = qrcode_initText(&qrcode, qrcodeData, kQrVersion, ECC_LOW, text);
        if (rc != 0) {
            display.clear();
            display.printCentered("QR encode failed", 24);
            return;
        }

        display.clear();
        int matrix = qrcode.size;
        int side = matrix + (kQrQuietZone * 2);
        int drawSize = display.getHeight();  // full-height square like Help/Contact menu screens
        int originX = (display.getWidth() - drawSize) / 2;
        int originY = 0;

        for (int y = -kQrQuietZone; y < matrix + kQrQuietZone; y++) {
            for (int x = -kQrQuietZone; x < matrix + kQrQuietZone; x++) {
                bool black = false;
                if (x >= 0 && y >= 0 && x < matrix && y < matrix) {
                    black = qrcode_getModule(&qrcode, x, y);
                }
                if (!black) {
                    continue;
                }
                int gx0 = x + kQrQuietZone;
                int gy0 = y + kQrQuietZone;
                int gx1 = gx0 + 1;
                int gy1 = gy0 + 1;

                int px0 = originX + (gx0 * drawSize) / side;
                int py0 = originY + (gy0 * drawSize) / side;
                int px1 = originX + (gx1 * drawSize) / side;
                int py1 = originY + (gy1 * drawSize) / side;

                int w = px1 - px0;
                int h = py1 - py0;
                if (w < 1) {
                    w = 1;
                }
                if (h < 1) {
                    h = 1;
                }
                display.fillRect(px0, py0, w, h);
            }
        }
    }

};
