#include "app/qt/OutlinedTextItem.h"

#include "domain/AuthoringLimits.h"
#include "infrastructure/rendering/GaussianBlur.h"
#include "infrastructure/fonts/FontResolver.h"
#include "infrastructure/rendering/RenderPaintSpecs.h"
#include "infrastructure/rendering/TextComposition.h"
#include "infrastructure/rendering/RenderTextLayout.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QQuickWindow>
#include <QThread>
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
withStackedStrokeWidths(
    QVector<PaintOutlineLayer> layers,
    const QVector<TextCompositionOutlineLayer> &compositionLayers) {
  const QVector<qreal> strokeWidths =
      stackedOutlineStrokeWidths(compositionLayers);
  for (qsizetype index = 0; index < layers.size(); ++index)
    layers[index].strokeWidth = strokeWidths.at(index);
  return layers;
}

QVector<TextCompositionOutlineLayer>
compositionOutlineLayers(const QVector<PaintOutlineLayer> &layers) {
  QVector<TextCompositionOutlineLayer> compositionLayers;
  compositionLayers.reserve(layers.size());
  for (const PaintOutlineLayer &layer : layers)
    compositionLayers.append({layer.enabled, layer.size});
  return compositionLayers;
}

qreal maxEnabledOutlineStrokeWidth(
    const QVector<TextCompositionOutlineLayer> &compositionLayers) {
  return maximumOutlineStrokeWidth(compositionLayers);
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
  requestPaintRefresh();
  emit colorChanged();
}

void OutlinedTextItem::setOutlineColor(const QColor &value) {
  if (outlineColor_ == value)
    return;
  outlineColor_ = value;
  requestPaintRefresh();
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
  const auto oldGeometry = stackedOutlineStrokeWidths(compositionOutlineLayers(
      outlineLayersFromVariant(outlineLayers_, outlineColor_, outlineSize_)));
  const auto newGeometry = stackedOutlineStrokeWidths(compositionOutlineLayers(
      outlineLayersFromVariant(value, outlineColor_, outlineSize_)));
  outlineLayers_ = value;
  if (oldGeometry == newGeometry)
    requestPaintRefresh();
  else
    notifyLayoutChanged();
  emit outlineLayersChanged();
}

void OutlinedTextItem::setBlurSize(int value) {
  value = std::max(0, value);
  if (blurSize_ == value)
    return;
  blurSize_ = value;
  requestPaintRefresh();
  emit blurSizeChanged();
}

void OutlinedTextItem::setShadowEnabled(bool value) {
  if (shadowEnabled_ == value)
    return;
  shadowEnabled_ = value;
  requestPaintRefresh();
  emit shadowEnabledChanged();
}

void OutlinedTextItem::setShadowColor(const QColor &value) {
  if (shadowColor_ == value)
    return;
  shadowColor_ = value;
  requestPaintRefresh();
  emit shadowColorChanged();
}

void OutlinedTextItem::setShadowOffsetX(qreal value) {
  if (qFuzzyCompare(shadowOffsetX_, value))
    return;
  shadowOffsetX_ = value;
  requestPaintRefresh();
  emit shadowOffsetXChanged();
}

void OutlinedTextItem::setShadowOffsetY(qreal value) {
  if (qFuzzyCompare(shadowOffsetY_, value))
    return;
  shadowOffsetY_ = value;
  requestPaintRefresh();
  emit shadowOffsetYChanged();
}

void OutlinedTextItem::setShadowBlurSize(int value) {
  value = std::max(0, value);
  if (shadowBlurSize_ == value)
    return;
  shadowBlurSize_ = value;
  requestPaintRefresh();
  emit shadowBlurSizeChanged();
}

void OutlinedTextItem::setGradientEnabled(bool value) {
  if (gradientEnabled_ == value)
    return;
  gradientEnabled_ = value;
  requestPaintRefresh();
  emit gradientEnabledChanged();
}

void OutlinedTextItem::setGradientDirection(int value) {
  value = std::clamp(value, 0, 1);
  if (gradientDirection_ == value)
    return;
  gradientDirection_ = value;
  requestPaintRefresh();
  emit gradientDirectionChanged();
}

void OutlinedTextItem::setGradientColorA(const QColor &value) {
  if (gradientColorA_ == value)
    return;
  gradientColorA_ = value;
  requestPaintRefresh();
  emit gradientColorAChanged();
}

void OutlinedTextItem::setGradientColorB(const QColor &value) {
  if (gradientColorB_ == value)
    return;
  gradientColorB_ = value;
  requestPaintRefresh();
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
  if (newGeometry.size() != oldGeometry.size())
    notifyLayoutChanged();
}

bool OutlinedTextItem::overflow() const {
  ensureLayoutCurrent();
  return overflow_;
}

bool OutlinedTextItem::editLayoutMetricsValid() const {
  ensureLayoutCurrent();
  return layoutCache().editMetricsValid;
}

qreal OutlinedTextItem::editLayoutTopPadding() const {
  ensureLayoutCurrent();
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.inset + std::max<qreal>(
                           0.0, (cache.layoutHeight - cache.inset * 2.0 -
                                 cache.blockHeight) /
                                    2.0);
}

qreal OutlinedTextItem::editLayoutLeftPadding() const {
  ensureLayoutCurrent();
  const LayoutCache &cache = layoutCache();
  return cache.editMetricsValid ? cache.inset : 0.0;
}

qreal OutlinedTextItem::editLayoutRightPadding() const {
  return editLayoutLeftPadding();
}

qreal OutlinedTextItem::editLayoutTabStopDistance() const {
  ensureLayoutCurrent();
  return layoutCache().tabStopDistance;
}

QVariantList OutlinedTextItem::editLayoutLineTops() const {
  ensureLayoutCurrent();
  QVariantList result;
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return result;
  for (const qreal lineTop : cache.lineTops)
    result.append(lineTop);
  return result;
}

qreal OutlinedTextItem::editLayoutPaintOffsetX() const {
  ensureLayoutCurrent();
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.paintTranslation.x();
}

qreal OutlinedTextItem::editLayoutPaintOffsetY() const {
  ensureLayoutCurrent();
  const LayoutCache &cache = layoutCache();
  if (!cache.editMetricsValid)
    return 0.0;
  return cache.paintTranslation.y();
}

void OutlinedTextItem::paint(QPainter *painter) {
  // Direct unit-test paints run on the item's GUI thread. Scene-graph paints
  // arrive after updatePolish(), so QObject notifications never cross threads.
  if (QThread::currentThread() == thread())
    processPendingUpdates();
  if (!painter || text_.isEmpty())
    return;

  const LayoutCache &cache = layoutCache();
  const qreal scale = cache.scale;
  const qreal layoutWidth = cache.layoutWidth;
  const qreal layoutHeight = cache.layoutHeight;
  const auto outlineLayers =
      outlineLayersFromVariant(outlineLayers_, outlineColor_, outlineSize_);
  const auto outlines = withStackedStrokeWidths(
      outlineLayers, compositionOutlineLayers(outlineLayers));
  const qreal maxOutline = cache.maxOutline;
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QPainterPath &path = cache.translatedPath;
  const QRectF &paintedBounds = cache.translatedPaintedBounds;

  QVector<qreal> outlineWidths;
  outlineWidths.reserve(outlines.size());
  for (const auto &outline : outlines)
    outlineWidths.append(outline.enabled ? outline.strokeWidth : 0.0);
  if (outlineCache_.geometryRevision != geometryRevision_ ||
      outlineCache_.widths != outlineWidths) {
    outlineCache_.geometryRevision = geometryRevision_;
    outlineCache_.widths = outlineWidths;
    outlineCache_.strokes.clear();
    outlineCache_.strokes.reserve(outlines.size());
    for (const qreal width : outlineWidths) {
      if (width <= 0) {
        outlineCache_.strokes.append(QPainterPath{});
        continue;
      }
      QPainterPathStroker stroker;
      stroker.setWidth(width);
      stroker.setJoinStyle(Qt::RoundJoin);
      QPainterPath stroke = stroker.createStroke(path);
      stroke.setFillRule(Qt::WindingFill);
      outlineCache_.strokes.append(stroke);
#ifdef TEXTFX_TESTING
      ++outlineStrokeRebuildCount_;
#endif
    }
  }

  // Preview keeps its legacy device-pixel blur policy: scale first, then cap.
  // TextComposition receives the equivalent document-space spread for bounds.
  const int shadowRadius =
      shadowEnabled_ ? cappedBlurKernelRadius(shadowBlurSize_ * scale) : 0;
  const QRectF shadowBounds =
      path.boundingRect().translated(shadowOffsetX_, shadowOffsetY_);
  const QRectF effectBounds =
      shadowEnabled_
          ? effectBoundsForShadow(paintedBounds, path,
                                  {shadowOffsetX_, shadowOffsetY_},
                                  shadowRadius / scale)
          : paintedBounds;

  auto paintShadow = [&](QPainter &target) {
    if (!shadowEnabled_)
      return;
    const QPointF shadowOffset(shadowOffsetX_, shadowOffsetY_);
    if (shadowCache_.geometryRevision != geometryRevision_ ||
        shadowCache_.offset != shadowOffset) {
      shadowCache_.geometryRevision = geometryRevision_;
      shadowCache_.offset = shadowOffset;
      shadowCache_.path = path.translated(shadowOffset);
      shadowCache_.image = {};
    }
    const QPainterPath &shadowPath = shadowCache_.path;
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
    if (shadowCache_.geometryRevision != geometryRevision_ ||
        shadowCache_.offset != shadowOffset ||
        shadowCache_.color != shadowColor_ ||
        shadowCache_.scale != scale || shadowCache_.radius != shadowRadius ||
        shadowCache_.sourceRect != sourceRect || shadowCache_.image.isNull()) {
      QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
      layer.fill(Qt::transparent);
      QPainter layerPainter(&layer);
      layerPainter.setRenderHint(QPainter::Antialiasing, true);
      layerPainter.translate(-sourceRect.topLeft());
      layerPainter.scale(scale, scale);
      layerPainter.fillPath(shadowPath, shadowColor_);
      layerPainter.end();
      shadowCache_ = {geometryRevision_, shadowOffset, shadowColor_, scale,
                      shadowRadius, sourceRect, shadowPath,
                      gaussianBlurred(layer, shadowRadius)};
#ifdef TEXTFX_TESTING
      ++shadowBlurRebuildCount_;
#endif
    }
    target.drawImage(sourceRect.topLeft(), shadowCache_.image);
  };
  auto paintText = [&](QPainter &target) {
    paintShadow(target);
    target.scale(scale, scale);
    for (qsizetype index = outlines.size(); index-- > 0;) {
      const auto &outline = outlines.at(index);
      if (!outline.enabled || outline.strokeWidth <= 0)
        continue;
      target.fillPath(outlineCache_.strokes.at(index), outline.color);
    }
    target.fillPath(path, textFillBrush({.gradientEnabled = gradientEnabled_,
                                         .baseColor = color_,
                                         .gradientDirection = gradientDirection_,
                                         .gradientColorA = gradientColorA_,
                                         .gradientColorB = gradientColorB_,
                                         .layoutWidth = layoutWidth,
                                         .layoutHeight = layoutHeight}));
  };
  painter->save();
  // Keep blur execution in device pixels; export has document scale 1.0.
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
    const BlurCacheKey key{renderRevision_, radius, sourceRect};
    if (blurCacheKey_.renderRevision != key.renderRevision ||
        blurCacheKey_.radius != key.radius ||
        blurCacheKey_.sourceRect != key.sourceRect || blurCacheImage_.isNull()) {
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
  cache.inset = outlineInset(cache.maxOutline);
  cache.font = layoutFont();
  cache.tabStopDistance = textLayoutTabStopDistance(cache.font);

  const QVector<QPointF> normalizedPoints = normalizedPathPoints(pathPoints_);
  cache.usingPathText = usesPathText(pathEnabled_, normalizedPoints);
  cache.editMetricsValid = !cache.usingPathText;

  const TextLayoutOptions layoutOptions{text_,
                                        cache.layoutWidth,
                                        cache.layoutHeight,
                                        cache.inset,
                                        lineSpacing_,
                                        horizontalAlignment_};
  const TextLayoutPathPolicy pathPolicy{
      .enabled = pathEnabled_,
      .normalizedPoints = normalizedPoints,
      .smooth = pathMode_ == 1};
  const PreparedTextLayout prepared = prepareTextLayout(layoutOptions, cache.font);
  cache.path = composeTextLayoutPath(prepared, layoutOptions, pathPolicy);
  cache.blockHeight = prepared.blockHeight;
  cache.lineTops = prepared.lineTops;
#ifdef TEXTFX_TESTING
  cache.wrappedLines = prepared.lineTexts;
#endif

  cache.paintedBounds = paintedTextBounds(cache.path, cache.maxOutline);
  cache.paintTranslation = translationToConstrainTextBounds(
      cache.paintedBounds, cache.layoutWidth, cache.layoutHeight,
      cache.usingPathText);
  cache.translatedPath = cache.path.translated(cache.paintTranslation);
  cache.translatedPaintedBounds =
      cache.paintedBounds.translated(cache.paintTranslation);

  layoutCache_ = cache;
  layoutCacheDirty_ = false;
#ifdef TEXTFX_TESTING
  ++layoutCacheRebuildCount_;
#endif
  return layoutCache_;
}

void OutlinedTextItem::invalidateLayoutCache() {
  layoutCacheDirty_ = true;
  ++geometryRevision_;
  ++renderRevision_;
}

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
  layoutNotificationPending_ = true;
  requestPaintRefresh();
}

void OutlinedTextItem::requestPaintRefresh() {
  paintRefreshPending_ = true;
  if (polishPending_)
    return;
  polishPending_ = true;
  polish();
}

void OutlinedTextItem::updatePolish() {
  polishPending_ = false;
  processPendingUpdates();
}

void OutlinedTextItem::processPendingUpdates() {
  const bool refreshPaint = paintRefreshPending_;
  paintRefreshPending_ = false;
  ensureLayoutCurrent();
  if (!refreshPaint)
    return;
  ++renderRevision_;
  update();
  if (auto *quickWindow = window())
    quickWindow->update();
#ifdef TEXTFX_TESTING
  ++paintRequestRevision_;
  emit paintRequestRevisionChangedForTesting();
#endif
}

void OutlinedTextItem::ensureLayoutCurrent() const {
  if ((!layoutCacheDirty_ && !layoutNotificationPending_) ||
      ensuringLayoutCurrent_)
    return;
  ensuringLayoutCurrent_ = true;
  auto *self = const_cast<OutlinedTextItem *>(this);
  const bool notifyMetrics = self->layoutNotificationPending_;
  self->layoutNotificationPending_ = false;
  (void)layoutCache();
  self->updateOverflow();
  ensuringLayoutCurrent_ = false;
  if (notifyMetrics)
    emit self->editLayoutMetricsChanged();
}

qreal OutlinedTextItem::effectiveMaxOutlineSize() const {
  const auto outlineLayers =
      outlineLayersFromVariant(outlineLayers_, outlineColor_, outlineSize_);
  return maxEnabledOutlineStrokeWidth(compositionOutlineLayers(outlineLayers));
}

QPointF OutlinedTextItem::paintTranslationForCurrentLayout() const {
  ensureLayoutCurrent();
  return layoutCache().paintTranslation;
}

#ifdef TEXTFX_TESTING
int OutlinedTextItem::layoutCacheRebuildCountForTesting() const {
  ensureLayoutCurrent();
  return layoutCacheRebuildCount_;
}

QStringList OutlinedTextItem::wrappedLinesForTesting() const {
  ensureLayoutCurrent();
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
