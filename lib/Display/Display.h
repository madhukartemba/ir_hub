#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Text alignment constants
enum TextAlign {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

enum VerticalAlign {
    VALIGN_TOP,
    VALIGN_CENTER,
    VALIGN_BOTTOM
};

class Display {
public:
    // Constructor
    Display();
    
    // Destructor
    ~Display();

    // Initialize the display
    bool begin(int sdaPin = -1, int sclPin = -1);
    
    // Basic display control
    void clear();
    void update();
    void turnOn();
    void turnOff();
    void setBrightness(uint8_t brightness);
    
    // Text display methods
    void print(const String& text, int x = 0, int y = 0);
    void print(const char* text, int x = 0, int y = 0);
    void println(const String& text, int x = 0, int y = 0);
    void println(const char* text, int x = 0, int y = 0);
    
    // Advanced text methods
    void printCentered(const String& text, int y = -1);
    void printCentered(const char* text, int y = -1);
    void printAligned(const String& text, TextAlign align, int y = 0, int x = 0);
    void printAligned(const char* text, TextAlign align, int y = 0, int x = 0);
    void printVerticallyAligned(const String& text, VerticalAlign valign, TextAlign halign = ALIGN_LEFT, int x = 0);
    void printVerticallyAligned(const char* text, VerticalAlign valign, TextAlign halign = ALIGN_LEFT, int x = 0);
    
    // Text wrapping
    void printWrapped(const String& text, int x = 0, int y = 0, int maxWidth = SCREEN_WIDTH);
    void printWrapped(const char* text, int x = 0, int y = 0, int maxWidth = SCREEN_WIDTH);
    void printWrappedCentered(const String& text, int y = 0);
    void printWrappedCentered(const char* text, int y = 0);
    
    // Text box with border
    void printInBox(const String& text, int x, int y, int width, int height, bool border = true, TextAlign align = ALIGN_CENTER);
    void printInBox(const char* text, int x, int y, int width, int height, bool border = true, TextAlign align = ALIGN_CENTER);
    
    // Text size and font
    void setTextSize(uint8_t size);
    void setTextColor(uint16_t color);
    uint8_t getTextSize() const;
    
    // Text measurement
    int getTextWidth(const String& text);
    int getTextWidth(const char* text);
    int getTextHeight();
    int getCharWidth();
    int getCharHeight();
    
    // Screen dimensions
    int getWidth() const;
    int getHeight() const;
    
    // Drawing methods
    void drawPixel(int x, int y, uint16_t color = 1);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color = 1);
    void drawRect(int x, int y, int width, int height, uint16_t color = 1);
    void fillRect(int x, int y, int width, int height, uint16_t color = 1);
    void drawCircle(int x, int y, int radius, uint16_t color = 1);
    void fillCircle(int x, int y, int radius, uint16_t color = 1);
    
    // Progress bar
    void drawProgressBar(int x, int y, int width, int height, int progress, int maxProgress = 100, bool showText = true);
    
    // Menu helpers
    void drawMenuItem(const String& text, int index, int totalItems, bool selected = false, int startY = 0);
    void drawMenuItem(const char* text, int index, int totalItems, bool selected = false, int startY = 0);
    
    // Get raw display object for advanced operations
    Adafruit_SSD1306* getDisplay();

private:
    Adafruit_SSD1306* display;
    uint8_t textSize;
    uint16_t textColor;
    
    // Helper methods
    void wrapText(const String& text, int x, int y, int maxWidth, TextAlign align = ALIGN_LEFT);
    int getWordWidth(const String& word);
    int calculateTextX(const String& text, TextAlign align, int x = 0, int maxWidth = SCREEN_WIDTH);
    int calculateTextY(VerticalAlign valign, int textHeight);
    void drawBorder(int x, int y, int width, int height);
};

#endif // DISPLAY_H
