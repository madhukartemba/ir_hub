#include "Display.h"

Display::Display() : display(nullptr), textSize(1), textColor(1) {
}

Display::~Display() {
    if (display) {
        delete display;
    }
}

bool Display::begin(int sdaPin, int sclPin) {
    // Initialize I2C if pins are specified
    if (sdaPin != -1 && sclPin != -1) {
        Wire.begin(sdaPin, sclPin);
    }
    
    // Create display object
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    
    // Initialize display
    if (!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        return false;
    }
    
    // Set default text properties
    display->setTextSize(textSize);
    display->setTextColor(textColor);
    display->cp437(true); // Use full 256 char 'Code Page 437' font
    
    clear();
    return true;
}

void Display::clear() {
    if (display) {
        display->clearDisplay();
    }
}

void Display::update() {
    if (display) {
        display->display();
    }
}

void Display::turnOn() {
    if (display) {
        display->ssd1306_command(SSD1306_DISPLAYON);
    }
}

void Display::turnOff() {
    if (display) {
        display->ssd1306_command(SSD1306_DISPLAYOFF);
    }
}

void Display::setBrightness(uint8_t brightness) {
    if (display) {
        display->ssd1306_command(SSD1306_SETCONTRAST);
        display->ssd1306_command(brightness);
    }
}

void Display::print(const String& text, int x, int y) {
    print(text.c_str(), x, y);
}

void Display::print(const char* text, int x, int y) {
    if (display) {
        display->setCursor(x, y);
        display->print(text);
    }
}

void Display::println(const String& text, int x, int y) {
    println(text.c_str(), x, y);
}

void Display::println(const char* text, int x, int y) {
    if (display) {
        display->setCursor(x, y);
        display->println(text);
    }
}

void Display::printCentered(const String& text, int y) {
    printCentered(text.c_str(), y);
}

void Display::printCentered(const char* text, int y) {
    if (!display) return;
    
    int textWidth = getTextWidth(text);
    int x = (SCREEN_WIDTH - textWidth) / 2;
    
    if (y == -1) {
        y = (SCREEN_HEIGHT - getTextHeight()) / 2;
    }
    
    display->setCursor(x, y);
    display->print(text);
}

void Display::printAligned(const String& text, TextAlign align, int y, int x) {
    printAligned(text.c_str(), align, y, x);
}

void Display::printAligned(const char* text, TextAlign align, int y, int x) {
    if (!display) return;
    
    int textX = calculateTextX(String(text), align, x);
    display->setCursor(textX, y);
    display->print(text);
}

void Display::printVerticallyAligned(const String& text, VerticalAlign valign, TextAlign halign, int x) {
    printVerticallyAligned(text.c_str(), valign, halign, x);
}

void Display::printVerticallyAligned(const char* text, VerticalAlign valign, TextAlign halign, int x) {
    if (!display) return;
    
    int textHeight = getTextHeight();
    int y = calculateTextY(valign, textHeight);
    printAligned(text, halign, y, x);
}

void Display::printWrapped(const String& text, int x, int y, int maxWidth) {
    printWrapped(text.c_str(), x, y, maxWidth);
}

void Display::printWrapped(const char* text, int x, int y, int maxWidth) {
    if (!display) return;
    wrapText(String(text), x, y, maxWidth);
}

void Display::printWrappedCentered(const String& text, int y) {
    printWrappedCentered(text.c_str(), y);
}

void Display::printWrappedCentered(const char* text, int y) {
    if (!display) return;
    wrapText(String(text), 0, y, SCREEN_WIDTH, ALIGN_CENTER);
}

void Display::printInBox(const String& text, int x, int y, int width, int height, bool border, TextAlign align) {
    printInBox(text.c_str(), x, y, width, height, border, align);
}

void Display::printInBox(const char* text, int x, int y, int width, int height, bool border, TextAlign align) {
    if (!display) return;
    
    if (border) {
        drawBorder(x, y, width, height);
    }
    
    // Calculate text position within the box
    int textX = x + 2; // Padding from left edge
    int textY = y + 2; // Padding from top edge
    int availableWidth = width - 4; // Account for padding
    
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
        textX = x + width - textWidth - 2; // Padding from right edge
    }
    
    display->setCursor(textX, textY);
    display->print(text);
}

void Display::setTextSize(uint8_t size) {
    textSize = size;
    if (display) {
        display->setTextSize(size);
    }
}

void Display::setTextColor(uint16_t color) {
    textColor = color;
    if (display) {
        display->setTextColor(color);
    }
}

uint8_t Display::getTextSize() const {
    return textSize;
}

int Display::getTextWidth(const String& text) {
    return getTextWidth(text.c_str());
}

int Display::getTextWidth(const char* text) {
    if (!display) return 0;
    
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
}

int Display::getTextHeight() {
    if (!display) return 0;
    
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h); // Use characters with ascenders and descenders
    return h;
}

int Display::getCharWidth() {
    return 6 * textSize; // Standard character width
}

int Display::getCharHeight() {
    return 8 * textSize; // Standard character height
}

int Display::getWidth() const {
    return SCREEN_WIDTH;
}

int Display::getHeight() const {
    return SCREEN_HEIGHT;
}

void Display::drawPixel(int x, int y, uint16_t color) {
    if (display) {
        display->drawPixel(x, y, color);
    }
}

void Display::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    if (display) {
        display->drawLine(x0, y0, x1, y1, color);
    }
}

void Display::drawRect(int x, int y, int width, int height, uint16_t color) {
    if (display) {
        display->drawRect(x, y, width, height, color);
    }
}

void Display::fillRect(int x, int y, int width, int height, uint16_t color) {
    if (display) {
        display->fillRect(x, y, width, height, color);
    }
}

void Display::drawCircle(int x, int y, int radius, uint16_t color) {
    if (display) {
        display->drawCircle(x, y, radius, color);
    }
}

void Display::fillCircle(int x, int y, int radius, uint16_t color) {
    if (display) {
        display->fillCircle(x, y, radius, color);
    }
}

void Display::drawProgressBar(int x, int y, int width, int height, int progress, int maxProgress, bool showText) {
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
        setTextColor(0); // Black text on white background
        print(percentText, textX, textY);
        setTextColor(1); // Reset to white text
    }
}

void Display::drawMenuItem(const String& text, int index, int totalItems, bool selected, int startY) {
    drawMenuItem(text.c_str(), index, totalItems, selected, startY);
}

void Display::drawMenuItem(const char* text, int index, int totalItems, bool selected, int startY) {
    if (!display) return;
    
    int itemHeight = getTextHeight() + 4; // Add some padding
    int y = startY + (index * itemHeight);
    
    if (selected) {
        // Highlight the selected item
        fillRect(0, y, SCREEN_WIDTH, itemHeight, 1);
        setTextColor(0); // Black text on white background
        print(text, 4, y + 2);
        setTextColor(1); // Reset to white text
    } else {
        print(text, 4, y + 2);
    }
}

Adafruit_SSD1306* Display::getDisplay() {
    return display;
}

// Private helper methods

void Display::wrapText(const String& text, int x, int y, int maxWidth, TextAlign align) {
    if (!display) return;
    
    String remainingText = text;
    int currentY = y;
    int lineHeight = getTextHeight() + 2; // Add some line spacing
    
    while (remainingText.length() > 0) {
        String line = "";
        String word = "";
        int lineWidth = 0;
        int i = 0;
        
        // Build line word by word
        while (i < remainingText.length()) {
            char c = remainingText[i];
            
            if (c == ' ' || c == '\n' || i == remainingText.length() - 1) {
                if (c != ' ' && c != '\n') {
                    word += c; // Include the last character
                }
                
                int wordWidth = getTextWidth(word);
                
                if (lineWidth + wordWidth <= maxWidth || line.length() == 0) {
                    if (line.length() > 0) line += " ";
                    line += word;
                    lineWidth += (line.length() > word.length() ? getCharWidth() : 0) + wordWidth;
                } else {
                    // Word doesn't fit, print current line and start new one
                    break;
                }
                
                word = "";
                
                if (c == '\n') {
                    i++; // Skip the newline character
                    break;
                }
            } else {
                word += c;
            }
            i++;
        }
        
        // Print the line
        if (line.length() > 0) {
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

int Display::getWordWidth(const String& word) {
    return getTextWidth(word);
}

int Display::calculateTextX(const String& text, TextAlign align, int x, int maxWidth) {
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

int Display::calculateTextY(VerticalAlign valign, int textHeight) {
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

void Display::drawBorder(int x, int y, int width, int height) {
    if (display) {
        display->drawRect(x, y, width, height, 1);
    }
}
