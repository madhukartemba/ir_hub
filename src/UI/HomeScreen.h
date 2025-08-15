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

   public:
    void onEnter() override {
        LOG_DEBUG("HomeScreen onEnter");
        lastActivityTime = millis();
        isBlanked = false;

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

        // Update display
        display.clear();
        showStatusScreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("HomeScreen onExit");
        // Make sure display is turned on when exiting
        display.turnOn();
    }

   private:
    void showStatusScreen() {
        // Show title
        display.setTextSize(1);
        display.printCentered("IR Hub - Status", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show WiFi status
        display.print("WiFi: ", 2, 20);
        if (WiFi.status() == WL_CONNECTED) {
            display.print("Connected", 30, 20);
        } else {
            display.print("Disconnected", 30, 20);
        }

        // Show IP address
        display.print("IP: ", 2, 32);
        if (WiFi.status() == WL_CONNECTED) {
            String ip = WiFi.localIP().toString();
            display.print(ip, 20, 32);
        } else {
            display.print("N/A", 20, 32);
        }

        // Show RSSI (signal strength)
        display.print("Signal: ", 2, 44);
        if (WiFi.status() == WL_CONNECTED) {
            int rssi = WiFi.RSSI();
            String signalStr = String(rssi) + " dBm";
            display.print(signalStr, 45, 44);
        } else {
            display.print("N/A", 45, 44);
        }

        // Show uptime
        unsigned long uptime = millis() / 1000;  // Convert to seconds
        unsigned long hours = uptime / 3600;
        unsigned long minutes = (uptime % 3600) / 60;
        unsigned long seconds = uptime % 60;

        display.print("Uptime: ", 2, 56);
        String uptimeStr = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" +
                           (seconds < 10 ? "0" : "") + String(seconds);
        display.print(uptimeStr, 45, 56);
    }
};
