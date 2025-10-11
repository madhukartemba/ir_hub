#pragma once
#include <cstddef>  // for size_t
#include <cstdint>  // for uint8_t, uint32_t
#include <vector>   // for std::vector

class LEDDriver {
   public:
    virtual ~LEDDriver() = default;

    virtual void begin() = 0;                                   // Initialize hardware
    virtual void show(const std::vector<uint32_t>& frame) = 0;  // Display frame
    virtual size_t getLedCount() = 0;                           // Led count
};
