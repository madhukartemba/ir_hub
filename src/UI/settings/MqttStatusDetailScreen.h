#pragma once

#include "../../global/Global.h"

class MqttStatusDetailScreen : public Screen {
   public:
    void onEnter() override {
        LOG_DEBUG("[MqttStatusDetailScreen] onEnter");
        ledRing.breathe(COLOR_INFO_DARK);
        button.setClickCallback([this]() { router.pop(); });
        button.setLongPressCallback([this]() { router.pop(); });
    }

    void onUpdate() override {
        display.clear();
        display.setTextSize(1);
        display.printCentered("MQTT Status", 0);
        display.drawLine(0, 12, display.getWidth(), 12);

        const char* status = "Disabled";
        const char* reason = "No broker config";

        if (!wifiManager.isConnected()) {
            status = "Offline";
            reason = "Wi-Fi disconnected";
        } else if (mqttConnector.isConnected()) {
            status = "Connected";
            reason = "Broker reachable";
        } else if (!mqttConnector.isEnabled()) {
            status = "Disabled";
            reason = "Configure Wi-Fi/MQTT";
        } else if (mqttConnector.hasError()) {
            status = "Error";
            reason = mqttConnector.lastConnectStateShort();
        } else {
            status = "Connecting";
            reason = mqttConnector.lastConnectStateShort();
        }

        display.printCentered(status, 24);
        display.printCentered(reason, 38);
        display.update();
    }

    void onExit() override { LOG_DEBUG("[MqttStatusDetailScreen] onExit"); }
};
