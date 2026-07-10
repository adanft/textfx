#include "infrastructure/rendering/RenderPaintSpecs.h"

#include <QLinearGradient>

namespace textfx {

QBrush textFillBrush(const TextFillSpec &spec) {
  if (!spec.gradientEnabled)
    return QBrush(spec.baseColor);

  const bool horizontal = spec.gradientDirection == HorizontalGradientDirection;
  const bool vertical = spec.gradientDirection == VerticalGradientDirection;
  QLinearGradient gradient(0, 0, horizontal ? spec.layoutWidth : 0,
                           vertical || !horizontal ? spec.layoutHeight : 0);
  gradient.setColorAt(0.0, spec.gradientColorA);
  gradient.setColorAt(1.0, spec.gradientColorB);
  return QBrush(gradient);
}

} // namespace textfx

