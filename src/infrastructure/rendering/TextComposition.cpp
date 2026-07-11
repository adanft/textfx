#include "infrastructure/rendering/TextComposition.h"

#include <QPainterPathStroker>

#include <algorithm>

namespace textfx {

QVector<qreal> stackedOutlineStrokeWidths(
    const QVector<TextCompositionOutlineLayer> &layers) {
  QVector<qreal> result(layers.size(), 0.0);
  qreal cumulativeRadius = 0.0;
  for (qsizetype index = 0; index < layers.size(); ++index) {
    const auto &layer = layers.at(index);
    if (!layer.enabled || layer.radius <= 0.0)
      continue;
    cumulativeRadius += layer.radius;
    result[index] = cumulativeRadius * 2.0;
  }
  return result;
}

qreal maximumOutlineStrokeWidth(
    const QVector<TextCompositionOutlineLayer> &layers) {
  const QVector<qreal> strokeWidths = stackedOutlineStrokeWidths(layers);
  qreal result = 0.0;
  for (const qreal strokeWidth : strokeWidths)
    result = std::max(result, strokeWidth);
  return result;
}

qreal outlineInset(qreal maximumStrokeWidth) {
  return maximumStrokeWidth > 0.0 ? maximumStrokeWidth / 2.0 : 0.0;
}

bool usesPathText(bool pathEnabled,
                  const QVector<QPointF> &normalizedPathPoints) {
  return pathEnabled && normalizedPathPoints.size() > 1;
}

QPainterPath composeTextLayoutPath(
    const TextLayoutOptions &options, const QFont &font,
    const TextLayoutPathPolicy &pathPolicy) {
  if (usesPathText(pathPolicy.enabled, pathPolicy.normalizedPoints))
    return pathTextLayoutPath(options, font, pathPolicy.normalizedPoints,
                               pathPolicy.smooth);
  return textLayoutPath(options, font);
}

QRectF paintedTextBounds(const QPainterPath &path, qreal outlineStrokeWidth) {
  QRectF result = path.boundingRect();
  if (outlineStrokeWidth <= 0.0)
    return result;

  QPainterPathStroker stroker;
  stroker.setWidth(outlineStrokeWidth);
  stroker.setJoinStyle(Qt::RoundJoin);
  return result.united(stroker.createStroke(path).boundingRect());
}

QPointF translationToConstrainTextBounds(const QRectF &paintedBounds,
                                         qreal layoutWidth,
                                         qreal layoutHeight,
                                         bool usingPathText) {
  qreal dx = 0.0;
  qreal dy = 0.0;
  if (paintedBounds.left() < 0.0)
    dx = -paintedBounds.left();
  else if (paintedBounds.width() <= layoutWidth &&
           paintedBounds.right() > layoutWidth)
    dx = layoutWidth - paintedBounds.right();

  if (!usingPathText) {
    if (paintedBounds.top() < 0.0)
      dy = -paintedBounds.top();
    else if (paintedBounds.height() <= layoutHeight &&
             paintedBounds.bottom() > layoutHeight)
      dy = layoutHeight - paintedBounds.bottom();
  }
  return {dx, dy};
}

QRectF effectBoundsForShadow(const QRectF &paintedBounds,
                             const QPainterPath &translatedPath,
                             const QPointF &shadowOffset,
                             qreal shadowSpread) {
  const QRectF shadowBounds = translatedPath.boundingRect()
                                  .translated(shadowOffset)
                                  .adjusted(-shadowSpread, -shadowSpread,
                                            shadowSpread, shadowSpread);
  return paintedBounds.united(shadowBounds);
}

} // namespace textfx
