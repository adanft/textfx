#pragma once

#include <QBrush>
#include <QColor>

namespace textfx {

inline constexpr int VerticalGradientDirection = 0;
inline constexpr int HorizontalGradientDirection = 1;

struct TextFillSpec {
  bool gradientEnabled = false;
  QColor baseColor;
  int gradientDirection = 0;
  QColor gradientColorA;
  QColor gradientColorB;
  qreal layoutWidth = 0.0;
  qreal layoutHeight = 0.0;
};

QBrush textFillBrush(const TextFillSpec &spec);

} // namespace textfx

