#include "../../global/Global.h"
#include <qrcode.h>

class HelpQrScreen : public Screen {
   private:
    static constexpr const char* kHelpUrl =
        "https://github.com/madhukartemba/ir_hub/blob/main/docs/HELP.md";
    static constexpr uint8_t kQrQuietZone = 1;

   public:
    void onEnter() override {
        LOG_DEBUG("[HelpQrScreen] onEnter");
        ledRing.breathe(COLOR_INFO);

        button.setClickCallback([this]() {
            LOG_DEBUG("[HelpQrScreen] onButtonClick - Exit");
            router.pop();
        });
        button.setLongPressCallback([this]() {
            LOG_DEBUG("[HelpQrScreen] onButtonLongPress - Exit");
            router.pop();
        });
    }

    void onUpdate() override {
        display.clear();
        drawQrCodeFullscreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[HelpQrScreen] onExit");
    }

   private:
    bool initQrForBestFit(QRCode& outQr, uint8_t* outBuf, size_t bufSize) {
        (void)bufSize;
        // Try smaller-to-larger versions; first one that fits payload wins.
        for (uint8_t version = 3; version <= 10; version++) {
            int8_t rc = qrcode_initText(&outQr, outBuf, version, ECC_LOW, kHelpUrl);
            if (rc == 0) {
                return true;
            }
        }
        return false;
    }

    void drawQrCodeFullscreen() {
        // Max temp buffer for version 10.
        uint8_t qrcodeData[qrcode_getBufferSize(10)];
        QRCode qrcode;
        if (!initQrForBestFit(qrcode, qrcodeData, sizeof(qrcodeData))) {
            display.printCentered("Help QR failed", 28);
            return;
        }

        int matrix = qrcode.size;
        int side = matrix + (kQrQuietZone * 2);
        int drawSize = display.getHeight();  // full-height square
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
                if (w < 1) w = 1;
                if (h < 1) h = 1;
                display.fillRect(px0, py0, w, h);
            }
        }
    }
};
