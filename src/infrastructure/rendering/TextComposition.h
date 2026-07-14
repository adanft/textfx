#pragma once

#include "infrastructure/rendering/RenderTextLayout.h"

#include <QFont>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace textfx {

// Inputs are normalized by the preview/export adapters before composition.
struct TextCompositionOutlineLayer {
  bool enabled = true;
  qreal radius = 0.0;
};

QVector<qreal> stackedOutlineStrokeWidths(
    const QVector<TextCompositionOutlineLayer> &layers);
qreal maximumOutlineStrokeWidth(
    const QVector<TextCompositionOutlineLayer> &layers);
qreal outlineInset(qreal maximumStrokeWidth);

bool usesPathText(bool pathEnabled,
                   const QVector<QPointF> &normalizedPathPoints);
struct TextLayoutPathPolicy {
  bool enabled = false;
  QVector<QPointF> normalizedPoints;
  bool smooth = false;
};

QPainterPath composeTextLayoutPath(
    const TextLayoutOptions &options, const QFont &font,
    const TextLayoutPathPolicy &pathPolicy);
QPainterPath composeTextLayoutPath(
    const PreparedTextLayout &layout, const TextLayoutOptions &options,
    const TextLayoutPathPolicy &pathPolicy);

QRectF paintedTextBounds(const QPainterPath &path, qreal outlineStrokeWidth);
QPointF translationToConstrainTextBounds(const QRectF &paintedBounds,
                                         qreal layoutWidth,
                                         qreal layoutHeight,
                                         bool usingPathText);
QRectF effectBoundsForShadow(const QRectF &paintedBounds,
                             const QPainterPath &translatedPath,
                             const QPointF &shadowOffset,
                             qreal shadowSpread);

} // namespace textfx
