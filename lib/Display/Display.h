#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <memory>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

enum DisplayType { SSD1306, SH1106 };

enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

enum VerticalAlign { VALIGN_TOP, VALIGN_CENTER, VALIGN_BOTTOM };

class Display {
   public:
    Display()
        : textSize(1), textColor(1), displayFlipped(true), displayOn(false), displayType(SSD1306) {}

    ~Display() = default;

    bool begin(int sdaPin = -1, int sclPin = -1, DisplayType type = SSD1306, bool flipped = false) {
        displayType = type;
        displayFlipped = flipped;

        if (sdaPin != -1 && sclPin != -1) {
            Wire.begin(sdaPin, sclPin);
        }
        Wire.setClock(400000);  // 400 kHz — shared SH1106 + DRV2605 bus

        if (displayType == SH1106) {
            display = std::make_unique<U8G2_SH1106_128X64_NONAME_F_HW_I2C>(U8G2_R0, U8X8_PIN_NONE);
        } else {
            display = std::make_unique<U8G2_SSD1306_128X64_NONAME_F_HW_I2C>(U8G2_R0, U8X8_PIN_NONE);
        }

        if (!display->begin()) {
            return false;
        }

        display->setFont(u8g2_font_helvR08_tr);
        display->setFontDirection(0);
        display->setFontMode(1);  // Transparent mode

        if (displayFlipped) {
            display->setDisplayRotation(U8G2_R2);
        }

        resetFPS();
        clear();
        displayOn = true;  // Display is on after successful initialization
        return true;
    }

    void clear() {
        if (display) {
            display->clearBuffer();
        }
    }

    void update() {
        if (!display) return;

        unsigned long currentTime = millis();
        unsigned long frameDelay = 1000 / fps;  // ms per frame

        if (currentTime - lastUpdateTime < frameDelay) return;
        lastUpdateTime = currentTime;

        uint32_t hash = computeBufferHash();
        if (needsFirstSend || hash != lastBufferHash) {
            display->sendBuffer();
            lastBufferHash = hash;
            needsFirstSend = false;
        }
    }

    void invalidate() { needsFirstSend = true; }

    void setFPS(uint8_t targetFPS) {
        LOG_DEBUG("[Display] Setting display FPS to %d", targetFPS);
        if (targetFPS == 0) targetFPS = 1;  // Avoid division by zero
        fps = targetFPS;
    }

    void resetFPS() {
        LOG_DEBUG("[Display] Resetting display FPS to default %d", defaultFPS);
        fps = defaultFPS;
    }

    uint8_t getFPS() const { return fps; }

    uint8_t getDefaultFps() const { return defaultFPS; }

    uint8_t setDefaultFps(uint8_t targetFPS) {
        if (targetFPS == 0) targetFPS = 1;  // Avoid division by zero
        defaultFPS = targetFPS;
        return defaultFPS;
    }

    void turnOn() {
        if (display) {
            display->setPowerSave(0);
            displayOn = true;
            needsFirstSend = true;  // re-push in case GDDRAM was lost
        }
    }
    void turnOff() {
        if (display) {
            display->setPowerSave(1);
            displayOn = false;
        }
    }
    bool isDisplayOn() const { return displayOn; }
    void setBrightness(uint8_t brightness) {
        if (display) {
            display->setContrast(brightness);
        }
    }
    void setFlip(bool flip) {
        displayFlipped = flip;
        if (display) {
            display->setDisplayRotation(displayFlipped ? U8G2_R2 : U8G2_R0);
        }
    }
    bool getFlip() const { return displayFlipped; }
    void toggleFlip() {
        displayFlipped = !displayFlipped;
        if (display) {
            display->setDisplayRotation(displayFlipped ? U8G2_R2 : U8G2_R0);
        }
    }

    void print(const String& text, int x = 0, int y = 0) { print(text.c_str(), x, y); }
    void print(const char* text, int x = 0, int y = 0) {
        if (display) {
            int adjustedY = y + getTextHeight() - 2;  // U8g2 baseline vs Adafruit top-origin
            display->setCursor(x, adjustedY);
            display->print(text);
        }
    }
    void println(const String& text, int x = 0, int y = 0) { println(text.c_str(), x, y); }
    void println(const char* text, int x = 0, int y = 0) {
        if (display) {
            int adjustedY = y + getTextHeight() - 2;  // U8g2 baseline vs Adafruit top-origin
            display->setCursor(x, adjustedY);
            display->print(text);
        }
    }

    void printCentered(const String& text, int y = -1) { printCentered(text.c_str(), y); }
    void printCentered(const char* text, int y = -1) {
        if (!display) return;

        int textWidth = getTextWidth(text);
        int x = (SCREEN_WIDTH - textWidth) / 2;

        if (y == -1) {
            y = (SCREEN_HEIGHT - getTextHeight()) / 2;
        }

        int adjustedY = y + getTextHeight() - 2;
        display->setCursor(x, adjustedY);
        display->print(text);
    }
    void printAligned(const String& text, TextAlign align, int y = 0, int x = 0) {
        printAligned(text.c_str(), align, y, x);
    }
    void printAligned(const char* text, TextAlign align, int y = 0, int x = 0) {
        if (!display) return;

        int textX = calculateTextX(String(text), align, x);
        int adjustedY = y + getTextHeight() - 2;
        display->setCursor(textX, adjustedY);
        display->print(text);
    }
    void printVerticallyAligned(const String& text, VerticalAlign valign,
                                TextAlign halign = ALIGN_LEFT, int x = 0) {
        printVerticallyAligned(text.c_str(), valign, halign, x);
    }
    void printVerticallyAligned(const char* text, VerticalAlign valign,
                                TextAlign halign = ALIGN_LEFT, int x = 0) {
        if (!display) return;

        int textHeight = getTextHeight();
        int y = calculateTextY(valign, textHeight);
        printAligned(text, halign, y, x);
    }

    void printWrapped(const String& text, int x = 0, int y = 0, int maxWidth = SCREEN_WIDTH) {
        printWrapped(text.c_str(), x, y, maxWidth);
    }
    void printWrapped(const char* text, int x = 0, int y = 0, int maxWidth = SCREEN_WIDTH) {
        if (!display) return;
        wrapText(String(text), x, y, maxWidth);
    }
    void printWrappedCentered(const String& text, int y = 0) {
        printWrappedCentered(text.c_str(), y);
    }
    void printWrappedCentered(const char* text, int y = 0) {
        if (!display) return;
        wrapText(String(text), 0, y, SCREEN_WIDTH, ALIGN_CENTER);
    }

    void printInBox(const String& text, int x, int y, int width, int height, bool border = true,
                    TextAlign align = ALIGN_CENTER) {
        printInBox(text.c_str(), x, y, width, height, border, align);
    }
    void printInBox(const char* text, int x, int y, int width, int height, bool border = true,
                    TextAlign align = ALIGN_CENTER) {
        if (!display) return;

        if (border) {
            drawBorder(x, y, width, height);
        }

        int textX = x + 2;
        int textY = y + 2;
        int availableWidth = width - 4;

        if (align == ALIGN_CENTER) {
            int textWidth = getTextWidth(text);
            if (textWidth <= availableWidth) {
                textX = x + (width - textWidth) / 2;
            } else {
                wrapText(String(text), textX, textY, availableWidth, ALIGN_CENTER);
                return;
            }
        } else if (align == ALIGN_RIGHT) {
            int textWidth = getTextWidth(text);
            textX = x + width - textWidth - 2;
        }

        int adjustedTextY = textY + getTextHeight() - 2;
        display->setCursor(textX, adjustedTextY);
        display->print(text);
    }

    void setTextSize(uint8_t size) {
        textSize = size;
        if (display) {
            switch (size) {
                case 1:
                    display->setFont(u8g2_font_helvR08_tr);  // Small
                    break;
                case 2:
                    display->setFont(u8g2_font_helvR10_tr);  // Medium
                    break;
                case 3:
                    display->setFont(u8g2_font_helvR12_tr);  // Large
                    break;
                default:
                    display->setFont(u8g2_font_helvR08_tr);
                    break;
            }
        }
    }
    void setTextColor(uint16_t color) {
        textColor = color;
        if (display) {
            display->setDrawColor(color);
        }
    }
    uint8_t getTextSize() const { return textSize; }

    int getTextWidth(const String& text) { return getTextWidth(text.c_str()); }
    int getTextWidth(const char* text) {
        if (!display) return 0;
        return display->getStrWidth(text);
    }
    int getTextHeight() {
        if (!display) return 0;
        return display->getMaxCharHeight();
    }
    int getCharWidth() {
        if (!display) return 6 * textSize;
        return display->getStrWidth("A");  // Width of a single character
    }
    int getCharHeight() {
        if (!display) return 8 * textSize;
        return display->getMaxCharHeight();
    }

    int getWidth() const { return SCREEN_WIDTH; }
    int getHeight() const { return SCREEN_HEIGHT; }

    void drawPixel(int x, int y, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawPixel(x, y);
        }
    }
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawLine(x0, y0, x1, y1);
        }
    }
    void drawRect(int x, int y, int width, int height, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawFrame(x, y, width, height);
        }
    }
    void fillRect(int x, int y, int width, int height, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawBox(x, y, width, height);
        }
    }
    void drawCircle(int x, int y, int radius, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawCircle(x, y, radius,
                                U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT |
                                    U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        }
    }
    void fillCircle(int x, int y, int radius, uint16_t color = 1) {
        if (display) {
            display->setDrawColor(color);
            display->drawDisc(x, y, radius,
                              U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT |
                                  U8G2_DRAW_LOWER_RIGHT);
        }
    }

    void drawProgressBar(int x, int y, int width, int height, int progress, int maxProgress = 100,
                         bool showText = true) {
        if (!display) return;
        if (maxProgress <= 0) maxProgress = 1;
        if (progress < 0) progress = 0;
        if (progress > maxProgress) progress = maxProgress;

        drawRect(x, y, width, height);
        int innerPad = (width >= 12 && height >= 8) ? 2 : 1;
        int innerW = width - (innerPad * 2);
        int innerH = height - (innerPad * 2);
        if (innerW > 0 && innerH > 0) {
            int fillWidth = (innerW * progress) / maxProgress;
            if (fillWidth > 0) {
                fillRect(x + innerPad, y + innerPad, fillWidth, innerH);
            }
        }

        if (showText) {
            String percentText = String((progress * 100) / maxProgress) + "%";
            int textY = y + height + 2;
            if (textY + getTextHeight() > SCREEN_HEIGHT) {
                textY = y + (height - getTextHeight()) / 2;
            }
            printCentered(percentText, textY);
        }
    }

    void drawMenuItem(const String& text, int index, int totalItems, bool selected = false,
                      int startY = 0) {
        drawMenuItem(text.c_str(), index, totalItems, selected, startY);
    }
    void drawMenuItem(const char* text, int index, int totalItems, bool selected = false,
                      int startY = 0) {
        if (!display) return;

        int itemHeight = getTextHeight() + 4;  // Add some padding
        int y = startY + (index * itemHeight);

        if (selected) {
            fillRect(0, y, SCREEN_WIDTH, itemHeight, 1);
            setTextColor(0);  // Black text on white background
            print(text, 4, y + 2);
            setTextColor(1);  // Reset to white text
        } else {
            print(text, 4, y + 2);
        }
    }

    U8G2* getDisplay() { return display.get(); }

   private:
    std::unique_ptr<U8G2> display;
    uint8_t textSize;
    uint16_t textColor;
    bool displayFlipped;
    bool displayOn;
    DisplayType displayType;

    uint8_t fps = 10;              // Target FPS
    uint8_t defaultFPS = 10;       // Default FPS
    unsigned long lastUpdateTime;  // Last time display was updated

    uint32_t lastBufferHash = 0;
    bool needsFirstSend = true;

    uint32_t computeBufferHash() const {  // FNV-1a dirty check
        if (!display) return 0;
        const uint8_t* buf = display->getBufferPtr();
        size_t size = static_cast<size_t>(display->getBufferTileWidth()) *
                      display->getBufferTileHeight() * 8;
        uint32_t h = 0x811C9DC5u;
        for (size_t i = 0; i < size; i++) {
            h ^= buf[i];
            h *= 0x01000193u;
        }
        return h;
    }

    void wrapText(const String& text, int x, int y, int maxWidth, TextAlign align = ALIGN_LEFT) {
        if (!display) return;

        String remainingText = text;
        int currentY = y;
        int lineHeight = getTextHeight() + 2;  // Add some line spacing

        while ((int)remainingText.length() > 0) {
            String line = "";
            String word = "";
            int lineWidth = 0;
            int i = 0;

            while (i < (int)remainingText.length()) {
                char c = remainingText[i];

                if (c == ' ' || c == '\n' || i == (int)remainingText.length() - 1) {
                    if (c != ' ' && c != '\n') {
                        word += c;  // Include the last character
                    }

                    int wordWidth = getTextWidth(word);

                    if (lineWidth + wordWidth <= maxWidth || (int)line.length() == 0) {
                        if ((int)line.length() > 0) line += " ";
                        line += word;
                        lineWidth +=
                            ((int)line.length() > (int)word.length() ? getCharWidth() : 0) +
                            wordWidth;
                    } else {
                        break;
                    }

                    word = "";

                    if (c == '\n') {
                        i++;  // Skip the newline character
                        break;
                    }
                } else {
                    word += c;
                }
                i++;
            }

            if ((int)line.length() > 0) {
                int lineX = calculateTextX(line, align, x, maxWidth);
                int adjustedCurrentY = currentY + getTextHeight() - 2;
                display->setCursor(lineX, adjustedCurrentY);
                display->print(line);
                currentY += lineHeight;
            }

            remainingText = remainingText.substring(i);

            if (currentY + lineHeight > SCREEN_HEIGHT) {
                break;
            }
        }
    }

    int getWordWidth(const String& word) { return getTextWidth(word); }

    int calculateTextX(const String& text, TextAlign align, int x = 0,
                       int maxWidth = SCREEN_WIDTH) {
        switch (align) {
            case ALIGN_CENTER:
                return x + (maxWidth - getTextWidth(text)) / 2;
            case ALIGN_RIGHT:
                return x + maxWidth - getTextWidth(text);
            case ALIGN_LEFT:
            default:
                return x;
        }
    }

    int calculateTextY(VerticalAlign valign, int textHeight) {
        switch (valign) {
            case VALIGN_CENTER:
                return (SCREEN_HEIGHT - textHeight) / 2;
            case VALIGN_BOTTOM:
                return SCREEN_HEIGHT - textHeight;
            case VALIGN_TOP:
            default:
                return 0;
        }
    }

    void drawBorder(int x, int y, int width, int height) {
        if (display) {
            display->setDrawColor(1);
            display->drawFrame(x, y, width, height);
        }
    }
};

#endif  // DISPLAY_H
