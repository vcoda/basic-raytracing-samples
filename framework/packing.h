#pragma once
#include <limits>

inline int8_t packSnorm(float x)
{
    x = std::max(-1.f, x);
    x = std::min(1.f, x);
    return int8_t(x * std::numeric_limits<int8_t>::max()); // [-127, 127]
}

inline uint8_t packUnorm(float x)
{
    x = std::max(0.f, x);
    x = std::min(1.f, x);
    return uint8_t(x * std::numeric_limits<uint8_t>::max()); // [0, 255]
}
