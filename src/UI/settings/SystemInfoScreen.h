#pragma once

#include <ESP.h>
#include <ESP8266WiFi.h>
#include "../../global/Global.h"

#ifndef FIRMWARE_VERSION
#  define FIRMWARE_VERSION "0.0.0"
#endif

class SystemInfoScreen : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[SystemInfoScreen] onEnter");
        ledRing.breathe(COLOR_INFO_DARK);

        button.setClickCallback([this]() { router.pop(); });
        button.setLongPressCallback([this]() { router.pop(); });
    }

    void onUpdate() override {
        display.clear();
        display.setTextSize(1);
        display.printCentered("Info", 0);
        display.drawLine(0, 12, display.getWidth(), 12);

        char line[32];
        snprintf(line, sizeof(line), "FW: v%s", FIRMWARE_VERSION);
        display.print(line, 0, 18);

        if (WiFi.status() == WL_CONNECTED) {
            snprintf(line, sizeof(line), "WiFi: %d dBm", WiFi.RSSI());
        } else {
            snprintf(line, sizeof(line), "WiFi: Disconnected");
        }
        display.print(line, 0, 28);

        snprintf(line, sizeof(line), "MQTT: %s",
                 mqttConnector.isConnected() ? "Connected"
                                             : (mqttConnector.isEnabled() ? "Connecting"
                                                                          : "Disabled"));
        display.print(line, 0, 38);

        snprintf(line, sizeof(line), "Heap: %lu", (unsigned long)ESP.getFreeHeap());
        display.print(line, 0, 48);

        display.printCentered("Click/hold: Back", 56);
        display.update();
    }

    void onExit() override { LOG_DEBUG("[SystemInfoScreen] onExit"); }
};
