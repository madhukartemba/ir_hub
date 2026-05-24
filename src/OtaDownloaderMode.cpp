#include "OtaDownloaderMode.h"

#include <ESP8266WiFi.h>
#include <Updater.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include "BootSafety.h"
#include "Global/Global.h"
#include "config.h"
#include "preferences.h"

namespace ota_downloader {

namespace {

// TLS receive buffer for the firmware download. We must use 16KB (the TLS maximum)
// because Cloudflare Pages does not reliably support MFLN (Max Fragment Length Negotiation)
// and sends full 16KB records for large files. This fits in our ~27KB downloader-mode
// heap because the BearSSL handshake transient memory (~4KB) is freed before Update.begin()
// allocates its flash buffer (~4KB).
constexpr int kDownloaderTlsRxBuffer = 16384;
constexpr int kDownloaderTlsTxBuffer = 512;

U8G2* g_dlDisplay = nullptr;

void downloaderShowStatus(const char* line1, const char* line2 = nullptr) {
    if (!g_dlDisplay) {
        return;
    }
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

void downloaderShowProgress(const char* version, size_t cur, size_t total) {
    if (!g_dlDisplay) {
        return;
    }

    ledRing.update();

    char title[32];
    snprintf(title, sizeof(title), "Installing v%s", version);
    char pct[8];
    unsigned p =
        (total > 0) ? (unsigned)(((unsigned long)cur * 100UL) / (unsigned long)total) : 0;
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
            if (bw > 0) {
                g_dlDisplay->drawBox(12, 34, bw, 10);
            }
        }

        w = g_dlDisplay->getStrWidth(pct);
        g_dlDisplay->drawStr((128 - w) / 2, 60, pct);
    } while (g_dlDisplay->nextPage());
}

bool downloaderStreamUpdate(WiFiClientSecure& client, const char* url, const char* version,
                            size_t expectedSize, const char* md5Hex) {
    // Parse URL to avoid String allocations
    const char* urlPath = url;
    if (strncmp(urlPath, "https://", 8) == 0) {
        urlPath += 8;
    } else if (strncmp(urlPath, "http://", 7) == 0) {
        urlPath += 7;
    }

    const char* slash = strchr(urlPath, '/');
    if (!slash) {
        slash = "/";
    }

    char host[64];
    size_t hostLen = slash > urlPath ? slash - urlPath : 0;
    if (hostLen >= sizeof(host)) {
        hostLen = sizeof(host) - 1;
    }
    strncpy(host, urlPath, hostLen);
    host[hostLen] = '\0';

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

    // Read headers manually to save HTTPClient overhead (~300 bytes)
    char c;
    int newlines = 0;
    unsigned long timeout = millis() + 10000UL;
    while (client.connected() || client.available()) {
        if (millis() > timeout) {
            return false;
        }
        if (!client.available()) {
            delay(1);
            continue;
        }
        c = client.read();
        if (c == '\n') {
            newlines++;
        } else if (c != '\r') {
            newlines = 0;
        }
        if (newlines == 2) {
            break;
        }
    }

    if (newlines != 2) {
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
            if (!client.connected() && client.available() <= 0) {
                break;
            }
            if (millis() - lastByteAt > kStreamIdleTimeoutMs) {
                break;
            }
            delay(1);
            continue;
        }
        size_t toRead = (size_t)avail < sizeof(buf) ? (size_t)avail : sizeof(buf);
        if (toRead > expectedSize - written) {
            toRead = expectedSize - written;
        }
        int n = client.readBytes(buf, toRead);
        if (n <= 0) {
            break;
        }
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

}  // namespace

[[noreturn]] void runDownloaderMode(const pending_ota::Slot& slot) {
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
    boot_safety::setLedReady(true);
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

    bool ok = downloaderStreamUpdate(client, slot.url, slot.version, (size_t)slot.expected_size,
                                     slot.md5_hex);

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
    while (true) {
        delay(1000);
    }
}

}  // namespace ota_downloader
