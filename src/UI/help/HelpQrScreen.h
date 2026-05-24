#include "../../global/Global.h"
#include <qrcode.h>

class HelpQrScreen : public Screen {
   private:
    static constexpr const char* kHelpUrl =
        "https://ir-hub.pages.dev/docs/HELP.md";
    static constexpr uint8_t kQrVersion = 3;  // keep modules large for reliable scanning
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
    void drawQrCodeFullscreen() {
        uint8_t qrcodeData[qrcode_getBufferSize(kQrVersion)];
        QRCode qrcode;
        int8_t rc = qrcode_initText(&qrcode, qrcodeData, kQrVersion, ECC_LOW, kHelpUrl);
        if (rc != 0) {
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
