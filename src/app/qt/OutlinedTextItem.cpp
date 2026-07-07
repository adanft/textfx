#include "app/qt/OutlinedTextItem.h"

#include "domain/AuthoringLimits.h"
#include "render/GaussianBlur.h"
#include "infrastructure/fonts/FontResolver.h"
#include "render/RenderTextLayout.h"

#include <QFont>
#include <QJsonDocument>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QQuickWindow>
#include <QRectF>

#include <algorithm>
#include <cmath>

namespace textfx {

namespace {
struct PaintOutlineLayer {
  bool enabled = true;
  QColor color = Qt::white;
  qreal size = 0.0;
  qreal strokeWidth = 0.0;
};

QVector<QPointF> normalizedPathPoints(const QVariantList &points) {
  QVector<QPointF> result;
  result.reserve(points.size());
  for (const QVariant &value : points) {
    const auto point = value.toList();
    result.append({point.value(0).toDouble(), point.value(1).toDouble()});
  }
  return result;
}

QPointF paintTranslationForBounds(const QRectF &paintedBounds,
                                  qreal layoutWidth, qreal layoutHeight,
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

QVector<PaintOutlineLayer> outlineLayersFromVariant(
    const QVariantList &layers, const QColor &fallbackColor,
    qreal fallbackSize) {
  QVector<PaintOutlineLayer> result;
  for (const QVariant &value : layers) {
    const QVariantMap map = value.toMap();
    PaintOutlineLayer layer;
    layer.enabled = map.value(QStringLiteral("enabled"), true).toBool();
    const QString colorValue =
        map.value(QStringLiteral("color"), fallbackColor.name(QColor::HexArgb))
            .toString();
    layer.color = QColor(colorValue);
    if (!layer.color.isValid() && colorValue.size() == 8) {
      bool ok = false;
      const uint rgba = colorValue.toUInt(&ok, 16);
      if (ok) {
        layer.color = QColor(static_cast<int>((rgba >> 24) & 0xff),
                             static_cast<int>((rgba >> 16) & 0xff),
                             static_cast<int>((rgba >> 8) & 0xff),
                             static_cast<int>(rgba & 0xff));
      }
    }
    if (!layer.color.isValid())
      layer.color = fallbackColor;
    layer.size = std::max<qreal>(0.0, map.value(QStringLiteral("size"), 0).toReal());
    result.append(layer);
  }
  if (result.isEmpty() && fallbackSize > 0)
    result.append({true, fallbackColor, fallbackSize / 2.0});
  return result;
}

QVector<PaintOutlineLayer>
withStackedStrokeWidths(QVector<PaintOutlineLayer> layers) {
  qreal cumulativeRadius = 0.0;
  for (auto &layer : layers) {
    if (!layer.enabled || layer.size <= 0.0)
      continue;
    cumulativeRadius += layer.size;
    layer.strokeWidth = cumulativeRadius * 2.0;
  }
  return layers;
}

qreal maxEnabledOutlineStrokeWidth(const QVector<PaintOutlineLayer> &layers) {
  qreal result = 0.0;
  for (const auto &layer : layers) {
    if (layer.enabled)
      result = std::max(result, layer.strokeWidth);
  }
  return result;
}
} // namespace

OutlinedTextItem::OutlinedTextItem(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  setAntialiasing(true);
}

void OutlinedTextItem::setText(const QString &value) {
  if (text_ == value)
    return;
  text_ = value;
  notifyLayoutChanged();
  emit textChanged();
}

void OutlinedTextItem::setFontFamily(const QString &value) {
  if (fontFamily_ == value)
    return;
  fontFamily_ = value;
  notifyLayoutChanged();
  emit fontFamilyChanged();
}

void OutlinedTextItem::setPixelSize(qreal value) {
  value = std::max<qreal>(1.0, value);
  if (qFuzzyCompare(pixelSize_, value))
    return;
  pixelSize_ = value;
  notifyLayoutChanged();
  emit pixelSizeChanged();
}

void OutlinedTextItem::setBold(bool value) {
  if (bold_ == value)
    return;
  bold_ = value;
  notifyLayoutChanged();
  emit boldChanged();
}

void OutlinedTextItem::setItalic(bool value) {
  if (italic_ == value)
    return;
  italic_ = value;
  notifyLayoutChanged();
  emit italicChanged();
}

void OutlinedTextItem::setLetterSpacing(qreal value) {
  if (qFuzzyCompare(letterSpacing_, value))
    return;
  letterSpacing_ = value;
  notifyLayoutChanged();
  emit letterSpacingChanged();
}

void OutlinedTextItem::setLineSpacing(qreal value) {
  if (qFuzzyCompare(lineSpacing_, value))
    return;
  lineSpacing_ = value;
  notifyLayoutChanged();
  emit lineSpacingChanged();
}

void OutlinedTextItem::setColor(const QColor &value) {
  if (color_ == value)
    return;
  color_ = value;
  update();
  emit colorChanged();
}

void OutlinedTextItem::setOutlineColor(const QColor &value) {
  if (outlineColor_ == value)
    return;
  outlineColor_ = value;
  update();
  emit outlineColorChanged();
}

void OutlinedTextItem::setOutlineSize(qreal value) {
  value = std::max<qreal>(0.0, value);
  if (qFuzzyCompare(outlineSize_, value))
    return;
  outlineSize_ = value;
  notifyLayoutChanged();
  emit outlineSizeChanged();
}

void OutlinedTextItem::setOutlineLayers(const QVariantList &value) {
  if (outlineLayers_ == value)
    return;
  outlineLayers_ = value;
  notifyLayoutChanged();
  emit outlineLayersChanged();
}

void OutlinedTextItem::setBlurSize(int value) {
  value = std::max(0, value);
  if (blurSize_ == value)
    return;
  blurSize_ = value;
  update();
  emit blurSizeChanged();
}

void OutlinedTextItem::setShadowEnabled(bool value) {
  if (shadowEnabled_ == value)
    return;
  shadowEnabled_ = value;
  update();
  emit shadowEnabledChanged();
}

void OutlinedTextItem::setShadowColor(const QColor &value) {
  if (shadowColor_ == value)
    return;
  shadowColor_ = value;
  update();
  emit shadowColorChanged();
}

void OutlinedTextItem::setShadowOffsetX(qreal value) {
  if (qFuzzyCompare(shadowOffsetX_, value))
    return;
  shadowOffsetX_ = value;
  update();
  emit shadowOffsetXChanged();
}

void OutlinedTextItem::setShadowOffsetY(qreal value) {
  if (qFuzzyCompare(shadowOffsetY_, value))
    return;
  shadowOffsetY_ = value;
  update();
  emit shadowOffsetYChanged();
}

void OutlinedTextItem::setShadowBlurSize(int value) {
  value = std::max(0, value);
  if (shadowBlurSize_ == value)
    return;
  shadowBlurSize_ = value;
  update();
  emit shadowBlurSizeChanged();
}

void OutlinedTextItem::setGradientEnabled(bool value) {
  if (gradientEnabled_ == value)
    return;
  gradientEnabled_ = value;
  update();
  emit gradientEnabledChanged();
}

void OutlinedTextItem::setGradientDirection(int value) {
  value = std::clamp(value, 0, 1);
  if (gradientDirection_ == value)
    return;
  gradientDirection_ = value;
  update();
  emit gradientDirectionChanged();
}

void OutlinedTextItem::setGradientColorA(const QColor &value) {
  if (gradientColorA_ == value)
    return;
  gradientColorA_ = value;
  update();
  emit gradientColorAChanged();
}

void OutlinedTextItem::setGradientColorB(const QColor &value) {
  if (gradientColorB_ == value)
    return;
  gradientColorB_ = value;
  update();
  emit gradientColorBChanged();
}

void OutlinedTextItem::setPathEnabled(bool value) {
  if (pathEnabled_ == value)
    return;
  pathEnabled_ = value;
  notifyLayoutChanged();
  emit pathEnabledChanged();
}

void OutlinedTextItem::setPathMode(int value) {
  value = std::clamp(value, 0, 1);
  if (pathMode_ == value)
    return;
  pathMode_ = value;
  notifyLayoutChanged();
  emit pathModeChanged();
}

void OutlinedTextItem::setPathPoints(const QVariantList &value) {
  if (pathPoints_ == value)
    return;
  pathPoints_ = value;
  notifyLayoutChanged();
  emit pathPointsChanged();
}

void OutlinedTextItem::setRenderScale(qreal value) {
  value = std::max<qreal>(0.0001, value);
  if (qFuzzyCompare(renderScale_, value))
    return;
  renderScale_ = value;
  notifyLayoutChanged();
  emit renderScaleChanged();
}

void OutlinedTextItem::setHorizontalAlignment(int value) {
  if (horizontalAlignment_ == value)
    return;
  horizontalAlignment_ = value;
  notifyLayoutChanged();
  emit horizontalAlignmentChanged();
}

void OutlinedTextItem::geometryChange(const QRectF &newGeometry,
                                      const QRectF &oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size()) {
    invalidateLayoutCache();
    updateOverflow();
    emit editLayoutMetricsChanged();
  }
}

bool OutlinedTextItem::editLayoutMetricsValid() const {
  return layoutCache().editMetricsValid;
}

qreal OutlinedTextItem::editLayoutTopPadding() const {
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.inset + std::max<qreal>(
                           0.0, (cache.layoutHeight - cache.inset * 2.0 -
                                 cache.blockHeight) /
                                    2.0);
}

qreal OutlinedTextItem::editLayoutLeftPadding() const {
  const LayoutCache &cache = layoutCache();
  return cache.editMetricsValid ? cache.inset : 0.0;
}

qreal OutlinedTextItem::editLayoutRightPadding() const {
  return editLayoutLeftPadding();
}

qreal OutlinedTextItem::editLayoutTabStopDistance() const {
  return layoutCache().tabStopDistance;
}

QVariantList OutlinedTextItem::editLayoutLineTops() const {
  QVariantList result;
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return result;
  for (const qreal lineTop : cache.lineTops)
    result.append(lineTop);
  return result;
}

qreal OutlinedTextItem::editLayoutPaintOffsetX() const {
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.paintTranslation.x();
}

qreal OutlinedTextItem::editLayoutPaintOffsetY() const {
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.paintTranslation.y();
}

void OutlinedTextItem::paint(QPainter *painter) {
  if (!painter || text_.isEmpty())
    return;

  const LayoutCache &cache = layoutCache();
  const qreal scale = cache.scale;
  const qreal layoutWidth = cache.layoutWidth;
  const qreal layoutHeight = cache.layoutHeight;
  const auto outlines = withStackedStrokeWidths(
      outlineLayersFromVariant(outlineLayers_, outlineColor_, outlineSize_));
  const qreal maxOutline = cache.maxOutline;
  painter->setRenderHint(QPainter::Antialiasing, true);

  QPainterPath path = cache.path;
  QRectF paintedBounds = cache.paintedBounds;
  const QPointF paintTranslation = cache.paintTranslation;
  const qreal dx = paintTranslation.x();
  const qreal dy = paintTranslation.y();
  if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
    path.translate(dx, dy);
    paintedBounds.translate(dx, dy);
  }

  const int shadowRadius =
      shadowEnabled_ ? cappedBlurKernelRadius(shadowBlurSize_ * scale) : 0;
  QRectF effectBounds = paintedBounds;
  const QRectF shadowBounds =
      path.boundingRect().translated(shadowOffsetX_, shadowOffsetY_);
  if (shadowEnabled_)
    effectBounds = effectBounds.united(
        shadowBounds.adjusted(-shadowRadius / scale, -shadowRadius / scale,
                              shadowRadius / scale, shadowRadius / scale));

  auto paintShadow = [&](QPainter &target) {
    if (!shadowEnabled_)
      return;
    const QPainterPath shadowPath =
        path.translated(shadowOffsetX_, shadowOffsetY_);
    if (shadowRadius <= 0) {
      target.save();
      target.scale(scale, scale);
      target.fillPath(shadowPath, shadowColor_);
      target.restore();
      return;
    }
    const QRect sourceRect =
        QRectF(shadowBounds.left() * scale - shadowRadius,
               shadowBounds.top() * scale - shadowRadius,
               shadowBounds.width() * scale + shadowRadius * 2,
               shadowBounds.height() * scale + shadowRadius * 2)
            .toAlignedRect();
    if (sourceRect.isEmpty())
      return;
    QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    QPainter layerPainter(&layer);
    layerPainter.setRenderHint(QPainter::Antialiasing, true);
    layerPainter.translate(-sourceRect.topLeft());
    layerPainter.scale(scale, scale);
    layerPainter.fillPath(shadowPath, shadowColor_);
    layerPainter.end();
    target.drawImage(sourceRect.topLeft(),
                     gaussianBlurred(layer, shadowRadius));
  };
  auto paintText = [&](QPainter &target) {
    paintShadow(target);
    target.scale(scale, scale);
    for (auto outline = outlines.crbegin(); outline != outlines.crend();
         ++outline) {
      if (!outline->enabled || outline->strokeWidth <= 0)
        continue;
      QPainterPathStroker stroker;
      stroker.setWidth(outline->strokeWidth);
      stroker.setJoinStyle(Qt::RoundJoin);
      QPainterPath stroke = stroker.createStroke(path);
      stroke.setFillRule(Qt::WindingFill);
      target.fillPath(stroke, outline->color);
    }
    if (gradientEnabled_) {
      QLinearGradient gradient(0, 0, gradientDirection_ == 1 ? layoutWidth : 0,
                               gradientDirection_ == 1 ? 0 : layoutHeight);
      gradient.setColorAt(0.0, gradientColorA_);
      gradient.setColorAt(1.0, gradientColorB_);
      target.fillPath(path, gradient);
    } else {
      target.fillPath(path, color_);
    }
  };
  painter->save();
  const int radius = cappedBlurKernelRadius(blurSize_ * scale);
  if (radius > 0) {
    const QRect itemRect(0, 0, static_cast<int>(std::ceil(width())),
                         static_cast<int>(std::ceil(height())));
    const QRect sourceRect =
        QRectF(effectBounds.left() * scale - radius,
               effectBounds.top() * scale - radius,
               effectBounds.width() * scale + radius * 2,
               effectBounds.height() * scale + radius * 2)
            .toAlignedRect()
            .intersected(itemRect.adjusted(-radius, -radius, radius, radius));
    if (sourceRect.isEmpty()) {
      painter->restore();
      return;
    }
    const QString key = blurCacheKey(radius, sourceRect);
    if (blurCacheKey_ != key || blurCacheImage_.isNull()) {
      QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
      layer.fill(Qt::transparent);
      QPainter layerPainter(&layer);
      layerPainter.setRenderHint(QPainter::Antialiasing, true);
      layerPainter.translate(-sourceRect.topLeft());
      paintText(layerPainter);
      layerPainter.end();
      blurCacheImage_ = gaussianBlurred(layer, radius);
      blurCacheKey_ = key;
    }
    painter->setClipRect(QRectF(0, 0, width(), height()));
    painter->drawImage(sourceRect.topLeft(), blurCacheImage_);
  } else {
    paintText(*painter);
  }
  painter->restore();
}

QString OutlinedTextItem::blurCacheKey(int radius,
                                       const QRect &sourceRect) const {
  return QStringList{text_,
                     fontFamily_,
                     QString::number(pixelSize_, 'g', 17),
                     bold_ ? QStringLiteral("1") : QStringLiteral("0"),
                     italic_ ? QStringLiteral("1") : QStringLiteral("0"),
                     QString::number(letterSpacing_, 'g', 17),
                     QString::number(lineSpacing_, 'g', 17),
                     QString::number(color_.rgba()),
                     QString::number(outlineColor_.rgba()),
                     QString::number(outlineSize_, 'g', 17),
                     QString::fromUtf8(QJsonDocument::fromVariant(outlineLayers_)
                                           .toJson(QJsonDocument::Compact)),
                     QString::number(renderScale_, 'g', 17),
                     QString::number(width(), 'g', 17),
                     QString::number(height(), 'g', 17),
                     QString::number(horizontalAlignment_),
                     shadowEnabled_ ? QStringLiteral("1") : QStringLiteral("0"),
                     QString::number(shadowColor_.rgba()),
                     QString::number(shadowOffsetX_, 'g', 17),
                     QString::number(shadowOffsetY_, 'g', 17),
                     QString::number(shadowBlurSize_),
                     gradientEnabled_ ? QStringLiteral("1")
                                      : QStringLiteral("0"),
                     QString::number(gradientDirection_),
                     QString::number(gradientColorA_.rgba()),
                     QString::number(gradientColorB_.rgba()),
                     pathEnabled_ ? QStringLiteral("1") : QStringLiteral("0"),
                     QString::number(pathMode_),
                     QString::fromUtf8(QJsonDocument::fromVariant(pathPoints_)
                                           .toJson(QJsonDocument::Compact)),
                     QString::number(radius),
                     QString::number(sourceRect.x()),
                     QString::number(sourceRect.y()),
                     QString::number(sourceRect.width()),
                     QString::number(sourceRect.height())}
      .join(u'\n');
}

QFont OutlinedTextItem::layoutFont() const {
  QFont font(fontFamily_);
  font.setPixelSize(static_cast<int>(std::round(pixelSize_)));
  font.setBold(bold_);
  font.setItalic(italic_);
  font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing_);
  return resolveFont(font).font;
}

const OutlinedTextItem::LayoutCache &OutlinedTextItem::layoutCache() const {
  if (!layoutCacheDirty_)
    return layoutCache_;

  LayoutCache cache;
  cache.scale = std::max<qreal>(0.0001, renderScale_);
  cache.layoutWidth = std::max<qreal>(1.0, width() / cache.scale);
  cache.layoutHeight = std::max<qreal>(1.0, height() / cache.scale);
  cache.maxOutline = effectiveMaxOutlineSize();
  cache.inset = cache.maxOutline > 0 ? cache.maxOutline / 2.0 : 0.0;
  cache.font = layoutFont();
  cache.tabStopDistance = textLayoutTabStopDistance(cache.font);

  const QVector<QPointF> normalizedPoints = normalizedPathPoints(pathPoints_);
  cache.usingPathText = pathEnabled_ && normalizedPoints.size() > 1 &&
                        !isNeutralFlatPath(normalizedPoints);
  cache.editMetricsValid = !cache.usingPathText;

  const TextLayoutOptions layoutOptions{text_,
                                        cache.layoutWidth,
                                        cache.layoutHeight,
                                        cache.inset,
                                        lineSpacing_,
                                        horizontalAlignment_};
  if (cache.usingPathText) {
    cache.path = pathTextLayoutPath(layoutOptions, cache.font, normalizedPoints,
                                    pathMode_ == 1, pixelSize_ + lineSpacing_);
#ifdef TEXTFX_TESTING
    textLayoutPath(layoutOptions, cache.font, &cache.wrappedLines);
#endif
  } else {
    cache.path = textLayoutPath(layoutOptions, cache.font,
#ifdef TEXTFX_TESTING
                                &cache.wrappedLines,
#else
                                nullptr,
#endif
                                nullptr, nullptr, &cache.lineTops);
    cache.blockHeight = textLayoutBlockHeight(layoutOptions, cache.font);
  }

  cache.paintedBounds = cache.path.boundingRect();
  if (cache.maxOutline > 0) {
    QPainterPathStroker stroker;
    stroker.setWidth(cache.maxOutline);
    stroker.setJoinStyle(Qt::RoundJoin);
    cache.paintedBounds = cache.paintedBounds.united(
        stroker.createStroke(cache.path).boundingRect());
  }
  cache.paintTranslation =
      paintTranslationForBounds(cache.paintedBounds, cache.layoutWidth,
                                cache.layoutHeight, cache.usingPathText);

  layoutCache_ = cache;
  layoutCacheDirty_ = false;
#ifdef TEXTFX_TESTING
  ++layoutCacheRebuildCount_;
#endif
  return layoutCache_;
}

void OutlinedTextItem::invalidateLayoutCache() { layoutCacheDirty_ = true; }

void OutlinedTextItem::updateOverflow() {
  const LayoutCache &cache = layoutCache();
  QRectF paintedBounds = cache.paintedBounds;
  paintedBounds.translate(cache.paintTranslation);

  constexpr qreal kOverflowEpsilon = 0.5;
  const bool hasOverflow =
      !text_.isEmpty() &&
      (paintedBounds.width() > cache.layoutWidth + kOverflowEpsilon ||
       paintedBounds.height() > cache.layoutHeight + kOverflowEpsilon ||
       paintedBounds.left() < -kOverflowEpsilon ||
       paintedBounds.top() < -kOverflowEpsilon ||
       paintedBounds.right() > cache.layoutWidth + kOverflowEpsilon ||
       paintedBounds.bottom() > cache.layoutHeight + kOverflowEpsilon);
  if (overflow_ == hasOverflow)
    return;
  overflow_ = hasOverflow;
  emit overflowChanged();
}

void OutlinedTextItem::notifyLayoutChanged() {
  invalidateLayoutCache();
  updateOverflow();
  requestPaintRefresh();
  emit editLayoutMetricsChanged();
}

void OutlinedTextItem::requestPaintRefresh() {
  update();
  if (auto *quickWindow = window())
    quickWindow->update();
#ifdef TEXTFX_TESTING
  ++paintRequestRevision_;
  emit paintRequestRevisionChangedForTesting();
#endif
}

qreal OutlinedTextItem::effectiveMaxOutlineSize() const {
  const auto outlines = withStackedStrokeWidths(
      outlineLayersFromVariant(outlineLayers_, outlineColor_, outlineSize_));
  return maxEnabledOutlineStrokeWidth(outlines);
}

QPointF OutlinedTextItem::paintTranslationForCurrentLayout() const {
  return layoutCache().paintTranslation;
}

#ifdef TEXTFX_TESTING
int OutlinedTextItem::layoutCacheRebuildCountForTesting() const {
  layoutCache();
  return layoutCacheRebuildCount_;
}

QStringList OutlinedTextItem::wrappedLinesForTesting() const {
  return layoutCache().wrappedLines;
}

QPointF
OutlinedTextItem::pathBaselinePointForTesting(qreal distance, qreal layoutWidth,
                                              qreal layoutHeight) const {
  return pathSampleAtDistance(
             layoutPathPoints(normalizedPathPoints(pathPoints_), layoutWidth,
                              layoutHeight, pathMode_ == 1),
             distance)
      .point;
}
#endif

} // namespace textfx
