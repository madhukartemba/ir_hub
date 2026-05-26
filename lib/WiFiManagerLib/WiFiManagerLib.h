#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <cstring>
#include "Display.h"
#include "Log.h"
#include "MqttCredentials.h"
#include "NeoRing.h"
#include "Speaker.h"
#include "secrets.h"

#ifndef OTA_PASSWORD
#  define OTA_PASSWORD ""
#endif

class WiFiManagerLib {
   private:
    enum class SetupState {
        IDLE,
        CONNECTING,
        PORTAL_ACTIVE,
        CONNECTED,
        TIMED_OUT,
    };

    WiFiManager wifiManager;
    Display& display;
    NeoRing& ledRing;
    Speaker& speaker;
    bool wifiConnected;
    SetupState setupState = SetupState::IDLE;
    bool setupFlowFinished = false;
    bool hasSavedCredentials = false;
    char setupApName_[33]{};
    char stableHostname_[24]{};
    bool isOtaSetup = false;
    uint32_t otaColor = 0x0000FF;
    uint32_t otaSuccessColor = 0x00FF00;
    uint32_t otaErrorColor = 0xFF0000;

    static constexpr int kMqttHostMax = 96;
    static constexpr int kMqttPortMax = 6;
    static constexpr int kMqttFieldMax = 64;

    char mqttHostBuf_[kMqttHostMax]{};  // must outlive wifiManager (not stack locals)
    char mqttPortBuf_[kMqttPortMax]{};
    char mqttUserBuf_[kMqttFieldMax]{};
    char mqttPassBuf_[kMqttFieldMax]{};
    WiFiManagerParameter portalHeader_;
    WiFiManagerParameter mqttSectionHeader_;
    WiFiManagerParameter mqttHostParam_;
    WiFiManagerParameter mqttPortParam_;
    WiFiManagerParameter mqttUserParam_;
    WiFiManagerParameter mqttPassParam_;
    WiFiManagerParameter mqttPassToggle_;
    WiFiManagerParameter mqttFooterHint_;

    const char* computeStableHostname() {
        if (stableHostname_[0] != '\0') {
            return stableHostname_;
        }
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        String suffix = mac.length() >= 6 ? mac.substring(mac.length() - 6) : mac;
        suffix.toLowerCase();
        snprintf(stableHostname_, sizeof(stableHostname_), "ir-hub-%s", suffix.c_str());
        return stableHostname_;
    }

    void pinRadioAwakeForMulticast() {
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
    }

    void setupWiFiManager(const char* apName, int apTimeout, int connectTimeout) {
        wifiManager.setConfigPortalTimeout(apTimeout);
        wifiManager.setConnectTimeout(connectTimeout);

        wifiManager.setBreakAfterConfig(true);    // Exit after configuration
        wifiManager.setRemoveDuplicateAPs(true);  // Remove duplicate APs
        wifiManager.setMinimumSignalQuality(30);  // Minimum signal quality
        wifiManager.setConfigPortalBlocking(false);

        wifiManager.setTitle("IR Hub Wi-Fi Setup");
        wifiManager.setClass("invert");

        wifiManager.setAPCallback([apName](WiFiManager* myWiFiManager) {
            (void)myWiFiManager;
            LOG_INFO("[WiFi] Config portal active (AP: %s)", apName);
        });

        wifiManager.setSaveConfigCallback([this]() {
            const char* host = mqttHostParam_.getValue();
            const char* portStr = mqttPortParam_.getValue();
            uint16_t port = 0;
            if (portStr && *portStr) {
                long parsed = strtol(portStr, nullptr, 10);
                if (parsed > 0 && parsed <= 65535) {
                    port = (uint16_t)parsed;
                }
            }
            if (!mqttCredentialsSave(host, port, mqttUserParam_.getValue(),
                                     mqttPassParam_.getValue())) {
                LOG_WARN("[Portal] MQTT credentials not saved; check LittleFS");
            }
            display.clear();
            display.printCentered("WiFi Saved!", 20);
            display.printCentered("Connecting...", 40);
            display.update();
        });

        std::vector<const char*> menu = {"wifi", "info"};
        wifiManager.setMenu(menu);

        wifiManager.addParameter(&portalHeader_);
        wifiManager.addParameter(&mqttSectionHeader_);
        wifiManager.addParameter(&mqttHostParam_);
        wifiManager.addParameter(&mqttPortParam_);
        wifiManager.addParameter(&mqttUserParam_);
        wifiManager.addParameter(&mqttPassParam_);
        wifiManager.addParameter(&mqttPassToggle_);
        wifiManager.addParameter(&mqttFooterHint_);
    }

   public:
    WiFiManagerLib(Display& display, NeoRing& ledRing, Speaker& speaker)
        : display(display),
          ledRing(ledRing),
          speaker(speaker),
          wifiConnected(false),
          portalHeader_(
              "<style>"
              "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;}"
              ".irhub-card{background:#0f172a;color:#f1f5f9;border-radius:10px;"
              "padding:14px 16px;margin:10px 0 16px;border:1px solid #1e293b;}"
              ".irhub-card h2{margin:0 0 6px;font-size:18px;}"
              ".irhub-card p{margin:0;font-size:13px;line-height:1.4;opacity:0.85;}"
              ".irhub-section{margin:18px 0 6px;padding:8px 10px;"
              "background:#1e293b;color:#f1f5f9;border-radius:8px;"
              "font-weight:600;font-size:14px;letter-spacing:0.02em;}"
              ".irhub-section small{display:block;font-weight:400;font-size:12px;"
              "opacity:0.75;margin-top:2px;}"
              ".irhub-hint{font-size:12px;color:#475569;margin:6px 2px 14px;}"
              ".irhub-toggle{display:block;margin:6px 0 14px;padding:6px 10px;"
              "background:#e2e8f0;border:1px solid #cbd5e1;border-radius:6px;"
              "color:#0f172a;cursor:pointer;width:100%;}"
              "</style>"
              "<div class=\"irhub-card\">"
              "<h2>IR Hub setup</h2>"
              "<p>Pick your Wi-Fi network below. Optionally fill in the MQTT "
              "section to connect this hub to Home Assistant.</p>"
              "</div>"),
          mqttSectionHeader_(
              "<div class=\"irhub-section\">MQTT / Home Assistant "
              "<small>Optional &mdash; leave the host blank to skip.</small></div>"),
          mqttHostParam_("mqtt_host", "Broker host (e.g. homeassistant.local)",
                         mqttHostBuf_, kMqttHostMax - 1,
                         "placeholder=\"homeassistant.local\" autocomplete=\"off\""),
          mqttPortParam_("mqtt_port", "Broker port", mqttPortBuf_, kMqttPortMax - 1,
                         "type=\"number\" min=\"1\" max=\"65535\" "
                         "placeholder=\"1883\" autocomplete=\"off\""),
          mqttUserParam_("mqtt_user", "MQTT username", mqttUserBuf_, kMqttFieldMax - 1,
                         "autocomplete=\"off\""),
          mqttPassParam_("mqtt_pass", "MQTT password", mqttPassBuf_, kMqttFieldMax - 1,
                         "type=\"password\" autocomplete=\"new-password\""),
          mqttPassToggle_(
              "<button type=\"button\" class=\"irhub-toggle\" onclick=\""
              "var e=document.getElementById('mqtt_pass');"
              "if(e){e.type=e.type==='password'?'text':'password';"
              "this.textContent=e.type==='password'?'Show MQTT password':'Hide MQTT password';}"
              "\">Show MQTT password</button>"),
          mqttFooterHint_(
              "<p class=\"irhub-hint\">Tip: if you are not using Home Assistant, "
              "leave all four MQTT fields blank. You can change these later by "
              "wiping Wi-Fi from the device's Settings menu.</p>") {}

    bool begin(const char* apName = "IRHub Setup", int apTimeout = 180, int connectTimeout = 60) {
        mqttCredentialsLoad();
        strncpy(setupApName_, apName ? apName : "IRHub Setup", sizeof(setupApName_) - 1);
        setupApName_[sizeof(setupApName_) - 1] = '\0';

        memset(mqttHostBuf_, 0, sizeof(mqttHostBuf_));
        memset(mqttPortBuf_, 0, sizeof(mqttPortBuf_));
        memset(mqttUserBuf_, 0, sizeof(mqttUserBuf_));
        memset(mqttPassBuf_, 0, sizeof(mqttPassBuf_));
        strncpy(mqttHostBuf_, mqttCredentialsHost(), sizeof(mqttHostBuf_) - 1);
        snprintf(mqttPortBuf_, sizeof(mqttPortBuf_), "%u",
                 (unsigned)mqttCredentialsPort());
        strncpy(mqttUserBuf_, mqttCredentialsUser(), sizeof(mqttUserBuf_) - 1);
        strncpy(mqttPassBuf_, mqttCredentialsPass(), sizeof(mqttPassBuf_) - 1);
        mqttHostParam_.setValue(mqttHostBuf_, kMqttHostMax - 1);  // arg is maxlength, not strlen
        mqttPortParam_.setValue(mqttPortBuf_, kMqttPortMax - 1);
        mqttUserParam_.setValue(mqttUserBuf_, kMqttFieldMax - 1);
        mqttPassParam_.setValue(mqttPassBuf_, kMqttFieldMax - 1);

        setupWiFiManager(apName, apTimeout, connectTimeout);

        const char* hostname = computeStableHostname();  // before autoConnect()/WiFi.begin()
        WiFi.hostname(hostname);
        LOG_INFO("[WiFi] Hostname: %s", hostname);

        hasSavedCredentials = wifiManager.getWiFiIsSaved();

        if (hasSavedCredentials) {
            display.clear();
            display.printCentered("IR Hub", 20);
            display.printCentered("Connecting...", 40);
            display.update();

            LOG_INFO("[WiFi] Found saved credentials, attempting connection...");
            LOG_DEBUG("[WiFi] Saved SSID: %s", WiFi.SSID().c_str());
        }

        LOG_INFO("[WiFi] Starting connection attempt...");
        LOG_DEBUG("[WiFi] Timeout set to %d seconds", connectTimeout);

        setupFlowFinished = false;
        setupState = SetupState::CONNECTING;
        wifiConnected = wifiManager.autoConnect(setupApName_);

        if (wifiConnected) {
            setupState = SetupState::CONNECTED;
            setupFlowFinished = true;
            pinRadioAwakeForMulticast();
            LOG_INFO("[WiFi] Connection successful!");
            LOG_DEBUG("[WiFi] IP Address: %s", WiFi.localIP().toString().c_str());
            LOG_DEBUG("[WiFi] SSID: %s", WiFi.SSID().c_str());
            LOG_DEBUG("[WiFi] RSSI: %d dBm", WiFi.RSSI());

            display.clear();
            display.printCentered("IR Hub", 20);
            display.printCentered("WiFi Connected!", 40);
            display.update();
        } else {
            if (wifiManager.getConfigPortalActive()) {
                setupState = SetupState::PORTAL_ACTIVE;
                LOG_INFO("[WiFi] Config portal active (AP: %s)", setupApName_);
            } else {
                setupState = SetupState::TIMED_OUT;
                setupFlowFinished = true;
                LOG_WARN("[WiFi] Connection failed - continuing in offline mode");
                LOG_DEBUG("[WiFi] Status: %d", WiFi.status());
                LOG_DEBUG("[WiFi] No saved credentials or connection timeout");
            }
        }

        return wifiConnected;
    }

    void setupOTA(uint32_t otaColor = 0x0000FF, uint32_t otaSuccessColor = 0x00FF00,
                  uint32_t otaErrorColor = 0xFF0000) {
        this->otaColor = otaColor;
        this->otaSuccessColor = otaSuccessColor;
        this->otaErrorColor = otaErrorColor;

        LOG_DEBUG("[OTA] Setting up OTA...");
        display.clear();
        display.printCentered("IR Hub", 20);
        display.printCentered("Setting up OTA...", 40);
        display.update();

        const char* otaHost = computeStableHostname();
        ArduinoOTA.setHostname(otaHost);
        LOG_INFO("[OTA] Hostname: %s", otaHost);
        if (OTA_PASSWORD[0] != '\0') {
            ArduinoOTA.setPassword(OTA_PASSWORD);
            LOG_INFO("[OTA] LAN push protected by password");
        } else {
            LOG_WARN("[OTA] No OTA_PASSWORD set — LAN push is unauthenticated");
        }

        ArduinoOTA.onStart([this]() {
            const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            LOG_INFO("[OTA] Start updating %s", type);
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            ledRing.spinner(this->otaColor);
            display.clear();
            display.printCentered("OTA Update", 10);
            display.printCentered("Starting...", 30);
            display.update();
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            ledRing.update();
            display.clear();
            display.printCentered("OTA Update", 4);
            display.drawLine(0, 20, display.getWidth(), 20);
            display.drawProgressBar(10, 32, 108, 14, (int)progress, (int)total, true);

            display.update();
        });

        ArduinoOTA.onEnd([this]() {
            LOG_INFO("[OTA] Update Complete");
            ledRing.solid(this->otaSuccessColor);
            ledRing.finishTransition();
            display.clear();
            display.printCentered("OTA Complete", 20);
            display.printCentered("Restarting...", 40);
            display.update();
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            if (!display.isDisplayOn()) {
                display.turnOn();
            }
            ledRing.solid(this->otaErrorColor);
            ledRing.finishTransition();
            LOG_ERROR("[OTA] Error: %u", (unsigned)error);
            display.clear();
            display.printCentered("OTA Error", 10);
            display.printCentered("Error: " + String(error), 30);
            display.update();
        });

        ArduinoOTA.begin();
        isOtaSetup = true;
        LOG_INFO("[OTA] Ready");

        display.clear();
        display.printCentered("IR Hub", 10);
        display.printCentered("IP: " + WiFi.localIP().toString(), 25);
        display.printCentered("OTA: Ready", 40);
        display.update();
        delay(2000);
    }

    bool isConnected() const { return wifiConnected && WiFi.status() == WL_CONNECTED; }
    bool isOtaReady() const { return isOtaSetup; }

    void reapplyNoSleep() { WiFi.setSleepMode(WIFI_NONE_SLEEP); }

    bool isSetupFlowFinished() const { return setupFlowFinished; }

    bool isSetupPortalActive() { return wifiManager.getConfigPortalActive(); }

    bool hasSavedWiFiCredentials() const { return hasSavedCredentials; }

    bool didSetupTimeout() const { return setupState == SetupState::TIMED_OUT; }

    bool isSetupInProgress() const {
        return !setupFlowFinished &&
               (setupState == SetupState::CONNECTING || setupState == SetupState::PORTAL_ACTIVE);
    }

    const char* setupApName() const { return setupApName_; }

    void skipSetupFlow() {
        wifiConnected = false;
        setupState = SetupState::IDLE;
        setupFlowFinished = true;
    }

    void update() {
        if (!setupFlowFinished) {
            wifiManager.process();

            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                setupState = SetupState::CONNECTED;
                setupFlowFinished = true;
                pinRadioAwakeForMulticast();
                LOG_INFO("[WiFi] Connected while processing setup flow");
                LOG_DEBUG("[WiFi] IP Address: %s", WiFi.localIP().toString().c_str());
            } else if (wifiManager.getConfigPortalActive()) {
                setupState = SetupState::PORTAL_ACTIVE;
            } else if (setupState == SetupState::PORTAL_ACTIVE ||
                       setupState == SetupState::CONNECTING) {
                setupState = SetupState::TIMED_OUT;
                setupFlowFinished = true;
                LOG_WARN("[WiFi] Setup portal closed without connection (offline mode)");
            }
        }

        if (isConnected()) {
            if (isOtaSetup) {
                ArduinoOTA.handle();  // Handle OTA updates only if WiFi is connected
            }
        }
    }

    void resetWiFi() {
        wifiManager.resetSettings();
        LOG_INFO("[WiFi] Settings reset");
    }
};

#endif  // WIFI_MANAGER_H
