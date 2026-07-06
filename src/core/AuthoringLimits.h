#pragma once

#include <algorithm>
#include <cmath>

namespace textfx {
inline constexpr int MinFontSize = 1;
inline constexpr int MaxFontSize = 512;
inline constexpr int MinTextSpacing = -100;
inline constexpr int MaxTextSpacing = 300;
inline constexpr double MinBoxSize = 12.0;
inline constexpr int MinEffectSize = 0;
inline constexpr int MaxEffectSize = 128;
inline constexpr int DefaultOutlineSize = 2;
inline constexpr int AddedOutlineLayerSizeStep = 2;
inline constexpr int MinShadowOffset = -512;
inline constexpr int MaxShadowOffset = 512;
inline constexpr int MaxBlurKernelRadius = 32;

inline int clampedEffectSize(int size) {
  return std::clamp(size, MinEffectSize, MaxEffectSize);
}

inline int clampedShadowOffset(int offset) {
  return std::clamp(offset, MinShadowOffset, MaxShadowOffset);
}

inline int cappedBlurKernelRadius(double radius) {
  return std::clamp(static_cast<int>(std::ceil(radius)), 0,
                    MaxBlurKernelRadius);
}
} // namespace textfx
