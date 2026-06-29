#pragma once

#include <algorithm>
#include <cmath>

namespace textfx {
inline constexpr int MaxBlurKernelRadius = 32;

inline int cappedBlurKernelRadius(double radius)
{
    return std::clamp(static_cast<int>(std::ceil(radius)), 0, MaxBlurKernelRadius);
}
} // namespace textfx
