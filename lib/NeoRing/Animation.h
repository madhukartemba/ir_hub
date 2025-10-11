#pragma once
#include <algorithm>  // std::fill
#include <cstddef>    // for size_t
#include <cstdint>    // for uint32_t
#include <vector>     // for std::vector

class Animation {
   protected:
    size_t ledCount = 0;
    std::vector<uint32_t> buffer;

   public:
    float speed = 1.0f;

    virtual ~Animation() = default;

    virtual void update() = 0;

    virtual const std::vector<uint32_t>& getFrame() const { return buffer; }

    virtual void reset() { std::fill(buffer.begin(), buffer.end(), 0); }
};