#pragma once

#include "domain/document/DocumentModel.h"

#include <algorithm>
#include <cmath>

namespace textfx {

inline constexpr double MinimumPaintStrokeSize = 1.0;

inline double normalizedPaintStrokeSize(double size) {
  return std::isfinite(size) ? std::max(MinimumPaintStrokeSize, size)
                             : MinimumPaintStrokeSize;
}

inline double normalizedPaintStrokeOpacity(double opacity) {
  return std::isfinite(opacity) ? std::clamp(opacity, 0.0, 1.0) : 0.0;
}

inline PaintStroke normalizedPaintStroke(PaintStroke stroke) {
  stroke.size = normalizedPaintStrokeSize(stroke.size);
  stroke.opacity = normalizedPaintStrokeOpacity(stroke.opacity);
  return stroke;
}

inline bool isDrawablePaintStroke(const PaintStroke &stroke) {
  return !stroke.points.empty() &&
         normalizedPaintStrokeOpacity(stroke.opacity) > 0.0;
}

} // namespace textfx
