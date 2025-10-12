#pragma once
#include <cstdint>

class Color {
   public:
    // Predefined colors
    static constexpr uint32_t Red = 0xFF0000;
    static constexpr uint32_t LightRed = 0xFF8000;
    static constexpr uint32_t Green = 0x00FF00;
    static constexpr uint32_t LightGreen = 0x80FF00;
    static constexpr uint32_t Blue = 0x0000FF;
    static constexpr uint32_t LightBlue = 0x0080FF;
    static constexpr uint32_t Amber = 0xFFBF00;
    static constexpr uint32_t White = 0xFFFFFF;
    static constexpr uint32_t Black = 0x000000;
    static constexpr uint32_t Yellow = 0xFFFF00;
    static constexpr uint32_t Cyan = 0x00FFFF;
    static constexpr uint32_t Magenta = 0xFF00FF;
    static constexpr uint32_t Orange = 0xFF8000;
    static constexpr uint32_t Purple = 0x8000FF;
    static constexpr uint32_t Pink = 0xFF0080;
    static constexpr uint32_t Lime = 0x80FF00;
    static constexpr uint32_t Teal = 0x008080;
    static constexpr uint32_t Navy = 0x000080;
    static constexpr uint32_t Gray = 0x808080;
    static constexpr uint32_t Silver = 0xC0C0C0;
    static constexpr uint32_t Maroon = 0x800000;
    static constexpr uint32_t OrangeRed = 0xFF4500;
    static constexpr uint32_t DarkGreen = 0x006400;
    static constexpr uint32_t DarkOrange = 0xCC7000;
    static constexpr uint32_t DarkRed = 0x8B0000;
    static constexpr uint32_t DarkBlue = 0x00008B;
    static constexpr uint32_t CornflowerBlue = 0x6495ED;
    static constexpr uint32_t RoyalBlue = 0x4169E1;

    // Helper to create custom RGB colors
    static uint32_t fromRGB(uint8_t r, uint8_t g, uint8_t b) {
        return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
};
