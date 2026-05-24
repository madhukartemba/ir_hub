#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include "NeoRing.h"
#include "PendingOta.h"
#include "UserPrefs.h"
#include "config.h"
#include "global/Global.h"
#include "led/ClickSweepOnceAnimation.h"
#include "preferences.h"
#include "secrets.h"
#include "ui/HomeScreen.h"

#ifndef FIRMWARE_VERSION
#  define FIRMWARE_VERSION "0.0.0"
#endif
#ifndef OTA_HW_VARIANT
#  define OTA_HW_VARIANT ""
#endif

// 6-hour gap between manifest checks. Long enough to be polite to GitHub's
// edge cache, short enough that a critical patch reaches the friend overnight.
static constexpr unsigned long kOtaCheckIntervalMs = 6UL * 60UL * 60UL * 1000UL;

unsigned long lastAnimSwitch = 0;
const unsigned long animSwitchInterval = 5000;  // 5 seconds

int currentAnim = 0;  // index to track which animation is active

// Critical-failure handling: graceful recovery + boot-loop guard so init
// failures don't brick the device until a reflash.

static bool g_displayReady = false;
static bool g_ledReady = false;
static bool g_speakerReady = false;

// RTC user memory survives soft restarts but not power loss.
// Must NOT be packed: rtcUserMemoryRead/Write needs 4-byte alignment.
struct BootGuard {
    uint32_t magic;
    uint16_t failures;
    uint16_t reserved;
};
static_assert(sizeof(BootGuard) == 8, "BootGuard layout changed unexpectedly");
static_assert(alignof(BootGuard) >= 4, "BootGuard must be uint32-aligned for RTC API");
static constexpr uint32_t kBootGuardMagic = 0xCAFEF00D;
static constexpr uint32_t kBootGuardRtcOffset = 64;  // away from OTA region
static constexpr uint16_t kBootGuardSoftLimit = 3;
static constexpr unsigned long kCritDisplayMs = 5000UL;
static constexpr unsigned long kCritPersistentDisplayMs = 30000UL;

static uint16_t bootGuardReadFailures() {
    BootGuard g{};
    if (!ESP.rtcUserMemoryRead(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g))) {
        return 0;
    }
    if (g.magic != kBootGuardMagic) {
        return 0;
    }
    return g.failures;
}

static void bootGuardWriteFailures(uint16_t failures) {
    BootGuard g{kBootGuardMagic, failures, 0};
    ESP.rtcUserMemoryWrite(kBootGuardRtcOffset, reinterpret_cast<uint32_t*>(&g), sizeof(g));
}

[[noreturn]] static void criticalFailure(const char* line1, const char* line2 = nullptr) {
    LOG_ERROR("[CRITICAL] %s%s%s", line1 ? line1 : "(unknown)", line2 ? " — " : "",
              line2 ? line2 : "");

    // setup() bumped this counter on entry; we never decremented because we're failing here.
    uint16_t failures = bootGuardReadFailures();
    bool persistent = failures >= kBootGuardSoftLimit;

    if (g_ledReady) {
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
    }

    if (g_displayReady) {
        display.clear();
        display.printCentered("ERROR", 6);
        display.drawLine(0, 18, display.getWidth(), 18);
        if (line1) {
            display.printCentered(line1, 26);
        }
        if (line2) {
            display.printCentered(line2, 38);
        }
        if (persistent) {
            display.printCentered("Boot loop detected", 50);
            display.printCentered("Power off & re-flash", 58);
        } else {
            display.printCentered("Restarting...", 54);
        }
        display.update();
    }

    if (g_speakerReady) {
        speaker.errorBeep();
    }

    unsigned long hold = persistent ? kCritPersistentDisplayMs : kCritDisplayMs;
    LOG_ERROR("[CRITICAL] Holding for %lums (failures=%u)%s", hold, (unsigned)failures,
              persistent ? " — boot loop detected" : "");
    delay(hold);

    LOG_ERROR("[CRITICAL] Restarting now");
    delay(50);
    ESP.restart();
    while (true) {  // unreachable; ESP.restart() isn't marked noreturn
        delay(1000);
    }
}

/// Mount LittleFS with one-shot format-and-retry recovery.
static bool mountLittleFsWithRecovery() {
    if (LittleFS.begin()) {
        return true;
    }

    LOG_WARN("[LittleFS] Mount failed — attempting recovery via format()");
    if (g_displayReady) {
        display.clear();
        display.printCentered("Storage Recovery", 6);
        display.drawLine(0, 18, display.getWidth(), 18);
        display.printCentered("Formatting...", 30);
        display.printCentered("Please wait", 44);
        display.update();
    }
    if (g_ledReady) {
        ledRing.solid(COLOR_WARNING);
        ledRing.finishTransition();
    }

    if (!LittleFS.format()) {
        LOG_ERROR("[LittleFS] Format failed");
        return false;
    }
    if (!LittleFS.begin()) {
        LOG_ERROR("[LittleFS] Mount failed even after format");
        return false;
    }

    LOG_WARN("[LittleFS] Recovered via format (all stored data wiped)");
    return true;
}

// ---- Downloader mode ------------------------------------------------------
// Runs at the very top of setup() if RTC RAM has a checksum-valid pending OTA
// slot. It initializes only display + LED ring + WiFi (no Alexa, no MQTT, no
// device cache, no IR, no router) so the entire ~32 KB heap is available for
// BearSSL's TLS handshake. That's the only reliable way to fit the binary
// download on an ESP8266.

// TLS receive buffer for the firmware download. We must use 16KB (the TLS maximum)
// because Cloudflare Pages does not reliably support MFLN (Max Fragment Length Negotiation)
// and sends full 16KB records for large files. This fits in our ~27KB downloader-mode
// heap because the BearSSL handshake transient memory (~4KB) is freed before Update.begin()
// allocates its flash buffer (~4KB).
static constexpr int kDownloaderTlsRxBuffer = 16384;
static constexpr int kDownloaderTlsTxBuffer = 512;

static U8G2* g_dlDisplay = nullptr;

static void downloaderShowStatus(const char* line1, const char* line2 = nullptr) {
    if (!g_dlDisplay) return;
    g_dlDisplay->firstPage();
    do {
        g_dlDisplay->setFont(u8g2_font_helvR08_tr);
        int w = g_dlDisplay->getStrWidth("Firmware Update");
        g_dlDisplay->drawStr((128 - w) / 2, 12, "Firmware Update");
        g_dlDisplay->drawLine(0, 18, 128, 18);
        if (line1) {
            w = g_dlDisplay->getStrWidth(line1);
            g_dlDisplay->drawStr((128 - w) / 2, 34, line1);
        }
        if (line2) {
            w = g_dlDisplay->getStrWidth(line2);
            g_dlDisplay->drawStr((128 - w) / 2, 48, line2);
        }
    } while (g_dlDisplay->nextPage());
}

static void downloaderShowProgress(const char* version, size_t cur, size_t total) {
    if (g_ledReady) ledRing.update();
    if (!g_dlDisplay) return;
    
    char title[32];
    snprintf(title, sizeof(title), "Installing v%s", version);
    char pct[8];
    unsigned p = (total > 0) ? (unsigned)(((unsigned long)cur * 100UL) / (unsigned long)total) : 0;
    snprintf(pct, sizeof(pct), "%u%%", p);

    g_dlDisplay->firstPage();
    do {
        g_dlDisplay->setFont(u8g2_font_helvR08_tr);
        int w = g_dlDisplay->getStrWidth(title);
        g_dlDisplay->drawStr((128 - w) / 2, 12, title);
        g_dlDisplay->drawLine(0, 20, 128, 20);
        
        g_dlDisplay->drawFrame(10, 32, 108, 14);
        if (total > 0) {
            int bw = (int)(104UL * cur / total);
            if (bw > 0) g_dlDisplay->drawBox(12, 34, bw, 10);
        }
        
        w = g_dlDisplay->getStrWidth(pct);
        g_dlDisplay->drawStr((128 - w) / 2, 60, pct);
    } while (g_dlDisplay->nextPage());
}

static void downloaderLogSslError(WiFiClientSecure& client, const char* phase) {
    // In downloader mode, Serial is intentionally disabled to save 512 bytes of heap
    // for the 16KB TLS buffer. So we just ignore the error logging here.
    (void)client;
    (void)phase;
}

/// Streams the firmware binary directly into the Update region.
static bool downloaderStreamUpdate(WiFiClientSecure& client, const char* initialUrl,
                                   const char* version, size_t expectedSize,
                                   const char* md5Hex) {
    char currentUrl[256];
    strncpy(currentUrl, initialUrl, sizeof(currentUrl) - 1);
    currentUrl[sizeof(currentUrl) - 1] = ' ';

    int redirects = 0;
    const int MAX_REDIRECTS = 3;

    while (redirects <= MAX_REDIRECTS) {
        const char* urlPath = currentUrl;
        if (strncmp(urlPath, "https://", 8) == 0) urlPath += 8;
        else if (strncmp(urlPath, "http://", 7) == 0) urlPath += 7;
        
        const char* slash = strchr(urlPath, '/');
        if (!slash) slash = "/";
        
        char host[64];
        size_t hostLen = slash > urlPath ? slash - urlPath : 0;
        if (hostLen >= sizeof(host)) hostLen = sizeof(host) - 1;
        strncpy(host, urlPath, hostLen);
        host[hostLen] = ' ';

        if (!client.connect(host, 443)) {
            return false;
        }

        client.print("GET ");
        client.print(slash);
        client.print(" HTTP/1.0\r\nHost: ");
        client.print(host);
        client.print("\r\nUser-Agent: IRHub-OTA/");
        client.print(version);
        client.print("\r\nConnection: close\r\n\r\n");

        unsigned long timeout = millis() + 10000UL;
        char lineBuf[256];
        size_t lineLen = 0;
        int statusCode = 0;
        bool isFirstLine = true;
        bool foundLocation = false;
        bool endOfHeaders = false;

        while (client.connected() || client.available()) {
            if (millis() > timeout) return false;
            if (!client.available()) { delay(1); continue; }
            
            char c = client.read();
            if (c == '\n') {
                lineBuf[lineLen] = '\0';
                
                if (lineLen == 0) {
                    endOfHeaders = true;
                    break;
                }
                
                if (isFirstLine) {
                    isFirstLine = false;
                    if (strncmp(lineBuf, "HTTP/1.", 7) == 0 && lineLen >= 12) {
                        statusCode = (lineBuf[9] - '0') * 100 + (lineBuf[10] - '0') * 10 + (lineBuf[11] - '0');
                    }
                } else if (statusCode == 301 || statusCode == 302) {
                    if (strncasecmp(lineBuf, "Location: ", 10) == 0) {
                        const char* loc = lineBuf + 10;
                        while (*loc == ' ') loc++;
                        strncpy(currentUrl, loc, sizeof(currentUrl) - 1);
                        currentUrl[sizeof(currentUrl) - 1] = '\0';
                        foundLocation = true;
                    }
                }
                
                lineLen = 0;
            } else if (c != '\r') {
                if (lineLen < sizeof(lineBuf) - 1) {
                    lineBuf[lineLen++] = c;
                }
            }
        }

        if (!endOfHeaders) return false;

        if (statusCode == 301 || statusCode == 302) {
            client.stop();
            if (!foundLocation) return false;
            redirects++;
            continue;
        } else if (statusCode != 200) {
            client.stop();
            return false;
        }

        if (!Update.begin(expectedSize, U_FLASH)) {
            return false;
        }
        if (md5Hex && *md5Hex) {
            Update.setMD5(md5Hex);
        }

        uint8_t buf[512];
        size_t written = 0;
        unsigned long lastProgress = millis();
        unsigned long lastByteAt = millis();
        constexpr unsigned long kStreamIdleTimeoutMs = 20000UL;

        while (written < expectedSize) {
            int avail = client.available();
            if (avail <= 0) {
                if (!client.connected() && client.available() <= 0) break;
                if (millis() - lastByteAt > kStreamIdleTimeoutMs) break;
                delay(1);
                continue;
            }
            size_t toRead = (size_t)avail < sizeof(buf) ? (size_t)avail : sizeof(buf);
            if (toRead > expectedSize - written) {
                toRead = expectedSize - written;
            }
            int n = client.readBytes(buf, toRead);
            if (n <= 0) break;
            if (Update.write(buf, (size_t)n) != (size_t)n) {
                Update.end(false);
                return false;
            }
            written += (size_t)n;
            lastByteAt = millis();
            if (millis() - lastProgress > 250) {
                downloaderShowProgress(version, written, expectedSize);
                lastProgress = millis();
            }
        }

        if (written != expectedSize) {
            Update.end(false);
            return false;
        }

        if (!Update.end(true)) {
            return false;
        }
        downloaderShowProgress(version, expectedSize, expectedSize);
        return true;
    }
    return false;
}

[[noreturn]] static void runDownloaderMode(const pending_ota::Slot& slot) {
    // We need every byte of heap for the 16KB TLS buffer.
    // Keep Serial disabled to save 512 bytes of RX/TX buffers.
    
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    
    // Use a local 1-page (128 bytes) display buffer instead of the global 1024-byte one
    if (DISPLAY_TYPE == DisplayType::SH1106) {
        static U8G2_SH1106_128X64_NONAME_1_HW_I2C sh1106(U8G2_R0, U8X8_PIN_NONE);
        g_dlDisplay = &sh1106;
    } else {
        static U8G2_SSD1306_128X64_NONAME_1_HW_I2C ssd1306(U8G2_R0, U8X8_PIN_NONE);
        g_dlDisplay = &ssd1306;
    }
    g_dlDisplay->begin();
    g_dlDisplay->setBusClock(400000);

    char installing[32];
    snprintf(installing, sizeof(installing), "Installing v%s", slot.version);
    downloaderShowStatus(installing, "Connecting Wi-Fi");

    ledRing.begin(NUM_LEDS, NEOPIXEL_PIN, DISPLAY_DRIVER);
    g_ledReady = true;
    ledRing.spinner(COLOR_INFO);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin();  // uses stored SSID/PSK from flash

    unsigned long wifiDeadline = millis() + 30000UL;
    while (WiFi.status() != WL_CONNECTED && millis() < wifiDeadline) {
        delay(200);
        ledRing.update();
    }
    if (WiFi.status() != WL_CONNECTED) {
        downloaderShowStatus("Wi-Fi failed", "Try again later");
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
        delay(2500);
        ESP.restart();
    }

    downloaderShowStatus(installing, "Downloading...");

    WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(kDownloaderTlsRxBuffer, kDownloaderTlsTxBuffer);

    bool ok = downloaderStreamUpdate(client, slot.url, slot.version,
                                     (size_t)slot.expected_size, slot.md5_hex);

    if (ok) {
        downloaderShowStatus("Success!", "Restarting...");
        ledRing.solid(COLOR_SUCCESS);
        ledRing.finishTransition();
        delay(1500);
    } else {
        downloaderShowStatus("Update failed", "Will retry later");
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
        delay(3000);
    }
    ESP.restart();
    while (true) delay(1000);  // unreachable
}

// Heap supervisor: logs trend + proactively restarts before fragmentation
// causes a mid-MQTT/OTA crash. ESP8266 has ~30 KB usable heap.
static constexpr unsigned long kHeapLogIntervalMs = 60UL * 1000UL;
static constexpr uint32_t kHeapFreePanicBytes = 4096;
static constexpr uint16_t kHeapBlockPanicBytes = 2048;
static constexpr uint8_t kHeapFragPanicPct = 80;
static unsigned long lastHeapLog = 0;

static void superviseHeap() {
    unsigned long now = millis();
    if (now - lastHeapLog < kHeapLogIntervalMs) {
        return;
    }
    lastHeapLog = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint16_t maxBlock = ESP.getMaxFreeBlockSize();
    uint8_t frag = ESP.getHeapFragmentation();
    LOG_INFO("[Heap] free=%u max_block=%u frag=%u%%", (unsigned)freeHeap,
             (unsigned)maxBlock, (unsigned)frag);

    if (freeHeap < kHeapFreePanicBytes || maxBlock < kHeapBlockPanicBytes ||
        frag > kHeapFragPanicPct) {
        LOG_ERROR("[Heap] Below safe limits (free=%u, max_block=%u, frag=%u%%) — restarting",
                  (unsigned)freeHeap, (unsigned)maxBlock, (unsigned)frag);
        delay(50);
        ESP.restart();
    }
}

void setup() {
    // OTA downloader-mode trampoline: if the previous boot's normal-mode
    // firmware armed a pending OTA, hand the entire heap to the TLS download
    // by skipping all non-essential subsystem init. We clear the slot first so
    // a crash during download falls back to normal boot on the next restart
    // (preventing an OTA-induced boot loop).
    pending_ota::Slot pending{};
    bool hasPending = pending_ota::peek(pending);
    if (hasPending) {
        pending_ota::clear();
        runDownloaderMode(pending);  // [[noreturn]]
    }

    Serial.begin(115200);

    // Bump boot-loop counter; cleared at end of setup() when system ready.
    uint16_t bootFailures = bootGuardReadFailures();
    bootGuardWriteFailures(bootFailures + 1);
    if (bootFailures > 0) {
        LOG_WARN("[Boot] Recovering from previous failed boot (count=%u)",
                 (unsigned)bootFailures);
    }

    // Initialize display first so subsequent failures can be shown on-screen.
    g_displayReady = display.begin(OLED_SDA_PIN, OLED_SCL_PIN, DISPLAY_TYPE, DISPLAY_FLIPPED);
    if (!g_displayReady) {
        LOG_ERROR("[Boot] Failed to initialize display");
        criticalFailure("Display", "init failed");
    }

    display.clear();
    display.printCentered("IR Hub", 20);
    display.printCentered("Initializing...", 40);
    display.update();

    // Probe only; defer calibration until after we know the user's haptics
    // preference (so a muted boot doesn't buzz the LRA).
    if (!haptics.probe()) {
        LOG_WARN("[Haptics] DRV2605 not found — tactile feedback disabled");
    }

    // Initialize NeoRing
    LOG_DEBUG("[Boot] Starting LED ring setup");
    ledRing.begin(NUM_LEDS, NEOPIXEL_PIN, DISPLAY_DRIVER);
    ledRing.solid(Color::RoyalBlue);
    ledRing.finishTransition();
    g_ledReady = true;
    LOG_DEBUG("[Boot] LED ring initialized");

    // Initialize LittleFS, with a one-shot format-and-retry recovery.
    if (!mountLittleFsWithRecovery()) {
        criticalFailure("LittleFS", "mount failed");
    }

    // Load prefs before any subsystem that reacts to them.
    userPrefsLoad();
    if (userPrefsHapticsEnabled() && haptics.isPresent()) {
        if (!haptics.begin()) {
            LOG_WARN("[Haptics] DRV2605 calibration failed — tactile feedback disabled");
        }
    }
    haptics.setMuted(!userPrefsHapticsEnabled());

    // Initialize IdGen
    if (!idGen.begin()) {
        criticalFailure("IdGen", "init failed");
    }

    // Initialize IRManager
    if (!irManager.begin(IR_RX_PIN, IR_TX_PIN)) {
        criticalFailure("IR Manager", "check IR pins");
    }

    // Initialize DeviceManager
    if (!deviceManager.begin()) {
        criticalFailure("Device Manager", "storage error");
    }

    // Initialize speaker
    LOG_DEBUG("[Boot] Starting speaker setup");
    if (!speaker.begin(SPEAKER_PIN)) {
        criticalFailure("Speaker", "check speaker pin");
    }
    speaker.setMuted(!userPrefsSoundEnabled());
    g_speakerReady = true;
    LOG_DEBUG("[Speaker] Initialized (muted=%s)", speaker.isMuted() ? "yes" : "no");

    // Initialize button
    LOG_DEBUG("[Boot] Starting button setup");
    if (!button.begin(TOUCH_BUTTON_PIN, INPUT)) {
        criticalFailure("Button", "check button pin");
    }
    button.setSpeaker(speaker);
    button.setHaptics(haptics);
    LOG_DEBUG("[Button] Initialized");

    // Successful core initialization now setup wireless connection
    speaker.playStartupSound();

    // Initialize WiFi Manager
    bool wifiConnected = wifiManager.begin(WIFI_AP_NAME, WIFI_AP_TIMEOUT, WIFI_CONNECT_TIMEOUT);
    wifiManager.setupOTA(COLOR_INFO, COLOR_SUCCESS, COLOR_ERROR);

    // Device add/remove: both Alexa and MQTT must stay in sync (single callback on DeviceManager)
    deviceManager.setOnDeviceAdded([](const Device& device) {
        LOG_DEBUG("[Hub] Device added: %s (ID: %d)", device.name.c_str(), device.id);
        alexaConnector.registerDevice(device);
        mqttConnector.registerDevice(device);
    });
    deviceManager.setOnDeviceRemoved([](const Device& device) {
        LOG_DEBUG("[Hub] Device removed: %s (ID: %d)", device.name.c_str(), device.id);
        alexaConnector.unregisterDevice(device);
        mqttConnector.unregisterDevice(device);
    });

    // Initialize AlexaConnector (will handle WiFi status internally)
    alexaConnector.begin();
    // Initialize MQTT (Home Assistant discovery + commands)
    mqttConnector.begin();

    auto onIrRemoteStateChange = [](const Device& device, bool state) {
        LOG_DEBUG("[Hub] Remote state: %s %s", device.name.c_str(), state ? "ON" : "OFF");
        if (state) {
            speaker.beep();
            ledRing.addAnimation(
                std::make_unique<ClickSweepOnceAnimation>(NUM_LEDS, SEND_ON_COMMAND_COLOR));
        } else {
            speaker.beep();
            ledRing.addAnimation(
                std::make_unique<ClickSweepOnceAnimation>(NUM_LEDS, SEND_OFF_COMMAND_COLOR));
        }
    };
    alexaConnector.setOnStateChangeCallback(onIrRemoteStateChange);
    mqttConnector.setOnStateChangeCallback(onIrRemoteStateChange);

    otaUpdater.begin(OTA_MANIFEST_URL, FIRMWARE_VERSION, OTA_HW_VARIANT, kOtaCheckIntervalMs);
    otaUpdater.setOnUpdatePending([](const char* version, const char* /*url*/) {
        // Flash a brief message before the reboot trampoline picks the OTA up
        // in downloader mode. The actual download UI lives in runDownloaderMode().
        if (!display.isDisplayOn()) display.turnOn();
        ledRing.solid(COLOR_INFO);
        ledRing.finishTransition();
        display.clear();
        display.printCentered("Update found", 10);
        char line[24];
        snprintf(line, sizeof(line), "v%s", version);
        display.printCentered(line, 26);
        display.printCentered("Restarting to", 42);
        display.printCentered("install...", 54);
        display.update();
        // Best-effort tidy-up of network sockets before reboot.
        mqttConnector.shutdown();
    });
    mqttConnector.setOnOtaCheckCallback([]() { otaUpdater.checkNow(); });

    // Show ready message on display
    delay(1000);
    display.clear();
    display.printCentered("IR Hub", 20);
    if (wifiConnected) {
        display.printCentered("Ready!", 40);
        speaker.successBeep();
        ledRing.solid(COLOR_SUCCESS);
        ledRing.finishTransition();
    } else {
        display.printCentered("Ready! (Offline)", 40);
        speaker.errorBeep();
        ledRing.solid(COLOR_ERROR);
        ledRing.finishTransition();
    }
    display.update();
    delay(500);

    // Initialize the global router
    router.setDefaultScreen(new HomeScreen());  // Status screen is now the default
    router.setTimeoutDuration(TIMEOUT_DURATION);
    router.enableTimeout(true);

    // Set up activity callback to reset timeout on button interactions
    router.setActivityCallback([]() -> unsigned long { return button.getLastInteractionTime(); });

    bootGuardWriteFailures(0);  // system ready, clear boot-loop counter

    LOG_INFO("[Boot] IR Hub: System Ready");
    LOG_INFO("[Heap] startup free=%u max_block=%u frag=%u%%",
             (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize(),
             (unsigned)ESP.getHeapFragmentation());
    lastHeapLog = millis();
}

void loop() {
    // ledRing is serviced multiple times to absorb the OLED transfer stall.
    // NeoRing's internal 60 fps gate makes extra calls free.
    ledRing.update();
    wifiManager.update();
    router.update();
    ledRing.update();
    button.update();
    alexaConnector.update();
    mqttConnector.update();
    otaUpdater.update();
    ledRing.update();
    superviseHeap();
}
