class SolidColor : public Animation {
   private:
    uint32_t color = 0;  // packed NeoPixel color

   public:
    // Constructor to set initial color
    SolidColor(size_t ledCount_, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
        m_isStatic = true;
        ledCount = ledCount_;            // set ledCount first
        setColor(r, g, b);               // set color
        buffer.resize(ledCount, color);  // resize buffer safely
    }

    SolidColor(size_t ledCount_, uint32_t packedColor) {
        m_isStatic = true;
        ledCount = ledCount_;
        color = packedColor;
        buffer.resize(ledCount, color);
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) { color = Adafruit_NeoPixel::Color(r, g, b); }

    void update() override {
        if (ledCount == 0) return;
        for (size_t i = 0; i < ledCount; ++i) {
            buffer[i] = color;
        }
    }
};
