#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "../global/Global.h"
#include "../ui/adddevice/AddDevice.h"
#include "../ui/devices/Devices.h"
#include "../ui/settings/Settings.h"

class MainMenu : public Screen {
   private:
    enum class State { DEVICES, ADD_DEVICE, SETTINGS, STATUS };

    State currentState;
    unsigned long lastActivityTime;
    const unsigned long INACTIVITY_TIMEOUT = 30000;  // 30 seconds in milliseconds

   public:
    void onEnter() override {
        LOG_DEBUG("MainMenu onEnter");
        currentState = State::DEVICES;
        lastActivityTime = millis();

        // Change button behavior
        button.setClickCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            // Switch to next state using mod operator
            LOG_DEBUG("MainMenu onButtonClick");
            currentState = static_cast<State>((static_cast<int>(currentState) + 1) % 4);
        });

        // Change button long press behavior
        button.setLongPressCallback([this]() {
            // Reset activity timer
            lastActivityTime = millis();

            LOG_DEBUG("MainMenu onButtonLongPress");
            if (currentState == State::DEVICES) {
                router.push(new Devices());
            } else if (currentState == State::ADD_DEVICE) {
                router.push(new AddDevice());
            } else if (currentState == State::SETTINGS) {
                router.push(new Settings());
            }
            // STATUS state doesn't have a long press action
        });
    }

    void onUpdate() override {
        // Check for inactivity timeout
        if (currentState != State::STATUS && (millis() - lastActivityTime) > INACTIVITY_TIMEOUT) {
            currentState = State::STATUS;
            LOG_DEBUG("Switching to STATUS due to inactivity");
        }

        // Update display based on current state
        display.clear();

        if (currentState == State::STATUS) {
            showStatusScreen();
        } else {
            showMenuScreen();
        }

        display.update();
    }

    void onExit() override { LOG_DEBUG("MainMenu onExit"); }

   private:
    void showMenuScreen() {
        // Show title
        display.setTextSize(1);
        display.printCentered("IR Hub - Main Menu", 0);

        // Draw horizontal line
        display.drawLine(0, 12, display.getWidth(), 12);

        // Show menu options with selection indicator
        const char* menuItems[] = {"Devices", "Add Device", "Settings"};
        int startY = 20;

        for (int i = 0; i < 3; i++) {
            bool isSelected = (i == static_cast<int>(currentState));
            display.drawMenuItem(menuItems[i], i, 3, isSelected, startY);
        }
    }

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