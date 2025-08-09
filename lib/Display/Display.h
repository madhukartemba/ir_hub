#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>
#include <memory>

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Text alignment constants
enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

enum VerticalAlign { VALIGN_TOP, VALIGN_CENTER, VALIGN_BOTTOM };

class Display {
   public:
    // Constructor
    Display() : textSize(1), textColor(1), displayFlipped(true) {}

    // Destructor
    ~Display() = default;

    // Initialize the display
    bool begin(int sdaPin = -1, int sclPin = -1) {
        // Initialize I2C if pins are specified
        if (sdaPin != -1 && sclPin != -1) {
            Wire.begin(sdaPin, sclPin);
        }

        // Create display object
        display =
            std::make_unique<Adafruit_SSD1306>(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

        // Initialize display
        if (!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            return false;
        }

        // Set default text properties
        display->setTextSize(textSize);
        display->setTextColor(textColor);
        display->cp437(true);  // Use full 256 char 'Code Page 437' font

        // Apply display rotation if flipped
        display->setRotation(displayFlipped ? 2 : 0);

        clear();
        return true;
    }

    // Basic display control
    void clear() {
        if (display) {
            display->clearDisplay();
        }
    }
    void update() {
        if (display) {
            display->display();
        }
    }
    void turnOn() {
        if (display) {
            display->ssd1306_command(SSD1306_DISPLAYON);
        }
    }
    void turnOff() {
        if (display) {
            display->ssd1306_command(SSD1306_DISPLAYOFF);
        }
    }
    void setBrightness(uint8_t brightness) {
        if (display) {
            display->ssd1306_command(SSD1306_SETCONTRAST);
            display->ssd1306_command(brightness);
        }
    }
    void setFlip(bool flip) {
        displayFlipped = flip;
        if (display) {
            display->setRotation(displayFlipped ? 2 : 0);
        }
    }
    bool getFlip() const { return displayFlipped; }
    void toggleFlip() {
        displayFlipped = !displayFlipped;
        if (display) {
            display->setRotation(displayFlipped ? 2 : 0);
        }
    }

    // Text display methods
    void print(const String& text, int x = 0, int y = 0) { print(text.c_str(), x, y); }
    void print(const char* text, int x = 0, int y = 0) {
        if (display) {
            display->setCursor(x, y);
            display->print(text);
        }
    }
    void println(const String& text, int x = 0, int y = 0) { println(text.c_str(), x, y); }
    void println(const char* text, int x = 0, int y = 0) {
        if (display) {
            display->setCursor(x, y);
            display->println(text);
        }
    }

    // Advanced text methods
    void printCentered(const String& text, int y = -1) { printCentered(text.c_str(), y); }
    void printCentered(const char* text, int y = -1) {
        if (!display) return;

        int textWidth = getTextWidth(text);
        int x = (SCREEN_WIDTH - textWidth) / 2;

        if (y == -1) {
            y = (SCREEN_HEIGHT - getTextHeight()) / 2;
        }

        display->setCursor(x, y);
        display->print(text);
    }
    void printAligned(const String& text, TextAlign align, int y = 0, int x = 0) {
        printAligned(text.c_str(), align, y, x);
    }
    void printAligned(const char* text, TextAlign align, int y = 0, int x = 0) {
        if (!display) return;

        int textX = calculateTextX(String(text), align, x);
        display->setCursor(textX, y);
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

    // Text wrapping
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

    // Text box with border
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

        // Calculate text position within the box
        int textX = x + 2;               // Padding from left edge
        int textY = y + 2;               // Padding from top edge
        int availableWidth = width - 4;  // Account for padding

        if (align == ALIGN_CENTER) {
            int textWidth = getTextWidth(text);
            if (textWidth <= availableWidth) {
                textX = x + (width - textWidth) / 2;
            } else {
                // Text is too wide, use wrapping
                wrapText(String(text), textX, textY, availableWidth, ALIGN_CENTER);
                return;
            }
        } else if (align == ALIGN_RIGHT) {
            int textWidth = getTextWidth(text);
            textX = x + width - textWidth - 2;  // Padding from right edge
        }

        display->setCursor(textX, textY);
        display->print(text);
    }

    // Text size and font
    void setTextSize(uint8_t size) {
        textSize = size;
        if (display) {
            display->setTextSize(size);
        }
    }
    void setTextColor(uint16_t color) {
        textColor = color;
        if (display) {
            display->setTextColor(color);
        }
    }
    uint8_t getTextSize() const { return textSize; }

    // Text measurement
    int getTextWidth(const String& text) { return getTextWidth(text.c_str()); }
    int getTextWidth(const char* text) {
        if (!display) return 0;

        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        return w;
    }
    int getTextHeight() {
        if (!display) return 0;

        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds("Ag", 0, 0, &x1, &y1, &w,
                               &h);  // Use characters with ascenders and descenders
        return h;
    }
    int getCharWidth() {
        return 6 * textSize;  // Standard character width
    }
    int getCharHeight() {
        return 8 * textSize;  // Standard character height
    }

    // Screen dimensions
    int getWidth() const { return SCREEN_WIDTH; }
    int getHeight() const { return SCREEN_HEIGHT; }

    // Drawing methods
    void drawPixel(int x, int y, uint16_t color = 1) {
        if (display) {
            display->drawPixel(x, y, color);
        }
    }
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color = 1) {
        if (display) {
            display->drawLine(x0, y0, x1, y1, color);
        }
    }
    void drawRect(int x, int y, int width, int height, uint16_t color = 1) {
        if (display) {
            display->drawRect(x, y, width, height, color);
        }
    }
    void fillRect(int x, int y, int width, int height, uint16_t color = 1) {
        if (display) {
            display->fillRect(x, y, width, height, color);
        }
    }
    void drawCircle(int x, int y, int radius, uint16_t color = 1) {
        if (display) {
            display->drawCircle(x, y, radius, color);
        }
    }
    void fillCircle(int x, int y, int radius, uint16_t color = 1) {
        if (display) {
            display->fillCircle(x, y, radius, color);
        }
    }

    // Progress bar
    void drawProgressBar(int x, int y, int width, int height, int progress, int maxProgress = 100,
                         bool showText = true) {
        if (!display) return;

        // Draw border
        drawRect(x, y, width, height);

        // Calculate fill width
        int fillWidth = (width - 2) * progress / maxProgress;
        if (fillWidth > 0) {
            fillRect(x + 1, y + 1, fillWidth, height - 2);
        }

        // Show percentage text if requested
        if (showText) {
            String percentText = String((progress * 100) / maxProgress) + "%";
            int textWidth = getTextWidth(percentText);
            int textX = x + (width - textWidth) / 2;
            int textY = y + (height - getTextHeight()) / 2;

            // Set inverted colors for text
            setTextColor(0);  // Black text on white background
            print(percentText, textX, textY);
            setTextColor(1);  // Reset to white text
        }
    }

    // Menu helpers
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
            // Highlight the selected item
            fillRect(0, y, SCREEN_WIDTH, itemHeight, 1);
            setTextColor(0);  // Black text on white background
            print(text, 4, y + 2);
            setTextColor(1);  // Reset to white text
        } else {
            print(text, 4, y + 2);
        }
    }

    // Get raw display object for advanced operations
    Adafruit_SSD1306* getDisplay() { return display.get(); }

   private:
    std::unique_ptr<Adafruit_SSD1306> display;
    uint8_t textSize;
    uint16_t textColor;
    bool displayFlipped;

    // Helper methods
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

            // Build line word by word
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
                        // Word doesn't fit, print current line and start new one
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

            // Print the line
            if ((int)line.length() > 0) {
                int lineX = calculateTextX(line, align, x, maxWidth);
                display->setCursor(lineX, currentY);
                display->print(line);
                currentY += lineHeight;
            }

            // Remove processed text
            remainingText = remainingText.substring(i);

            // Check if we've run out of vertical space
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
            display->drawRect(x, y, width, height, 1);
        }
    }
};

#endif  // DISPLAY_H
