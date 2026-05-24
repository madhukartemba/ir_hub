#include "../../global/Global.h"
#include <qrcode.h>

class ContactQrScreen : public Screen {
   private:
    static constexpr const char* kContactMailto = "mailto:madhusmiles.madhukar@gmail.com";
    static constexpr uint8_t kQrVersion = 3;  // 29x29 modules; fits 128x64 nicely
    // Use a minimal quiet zone so Version 3 can scale to ~full OLED height.
    static constexpr uint8_t kQrQuietZone = 1;

   public:
    void onEnter() override {
        LOG_DEBUG("[ContactQrScreen] onEnter");
        ledRing.breathe(COLOR_INFO);

        button.setClickCallback([this]() {
            LOG_DEBUG("[ContactQrScreen] onButtonClick - Exit");
            router.pop();
        });

        button.setLongPressCallback([this]() {
            LOG_DEBUG("[ContactQrScreen] onButtonLongPress - Exit");
            router.pop();
        });
    }

    void onUpdate() override {
        display.clear();
        drawQrCodeFullscreen();
        display.update();
    }

    void onExit() override {
        LOG_DEBUG("[ContactQrScreen] onExit");
    }

   private:
    void drawQrCodeFullscreen() {
        uint8_t qrcodeData[qrcode_getBufferSize(kQrVersion)];
        QRCode qrcode;
        int8_t rc = qrcode_initText(&qrcode, qrcodeData, kQrVersion, ECC_LOW, kContactMailto);
        if (rc != 0) {
            display.printCentered("QR encode failed", 28);
            return;
        }

        int matrix = qrcode.size;
        int side = matrix + (kQrQuietZone * 2);
        // Stretch to exact full OLED height (64 px) while preserving square bounds.
        // We map module edges proportionally so the QR fills the available height.
        int drawSize = display.getHeight();
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
