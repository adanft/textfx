#include "app/qt/PaintLayerItem.h"

#include <QColor>
#include <QJSValue>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace textfx {
namespace {

QVariantMap strokeMap(const QVariant &stroke) {
  const QVariant value = stroke.metaType() == QMetaType::fromType<QJSValue>()
                             ? stroke.value<QJSValue>().toVariant()
                             : stroke;
  return value.toMap();
}

QColor strokeColor(const QVariantMap &stroke, qreal opacity) {
  QString raw = stroke.value(QStringLiteral("color"), QStringLiteral("000000ff"))
                    .toString();
  if (raw.isEmpty())
    raw = QStringLiteral("000000ff");
  raw.remove(u'#');
  auto channel = [&raw](qsizetype offset) {
    bool ok = false;
    const int value = raw.mid(offset, 2).toInt(&ok, 16);
    return ok ? value : 0;
  };
  const int alpha = raw.size() >= 8 ? channel(6) : 255;
  return QColor(channel(0), channel(2), channel(4),
                qRound(alpha * std::clamp(opacity, 0.0, 1.0)));
}

QPointF pointFromVariant(const QVariant &value) {
  const QVariantList coordinates = value.toList();
  return {coordinates.value(0).toDouble(), coordinates.value(1).toDouble()};
}

} // namespace

PaintLayerItem::PaintLayerItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  connect(this, &QQuickItem::windowChanged, this,
          &PaintLayerItem::attachWindow);
  connect(this, &QQuickItem::widthChanged, this, &PaintLayerItem::schedulePaint);
  connect(this, &QQuickItem::heightChanged, this,
          &PaintLayerItem::schedulePaint);
}

void PaintLayerItem::setStrokes(const QVariantList &strokes) {
  if (strokes_ == strokes)
    return;
  strokes_ = strokes;
  emit strokesChanged();
  if (drawPersistedStrokes_)
    schedulePaintAndContentUpdate();
  else
    emit paintContentChanged();
}

void PaintLayerItem::setPreviewStroke(const QVariant &stroke) {
  if (previewStroke_ == stroke)
    return;
  previewStroke_ = stroke;
  emit previewStrokeChanged();
  if (drawPreviewStroke_)
    schedulePaintAndContentUpdate();
  else
    emit paintContentChanged();
}

void PaintLayerItem::setDrawPersistedStrokes(bool draw) {
  if (drawPersistedStrokes_ == draw)
    return;
  drawPersistedStrokes_ = draw;
  emit drawPersistedStrokesChanged();
  schedulePaintAndContentUpdate();
}

void PaintLayerItem::setDrawPreviewStroke(bool draw) {
  if (drawPreviewStroke_ == draw)
    return;
  drawPreviewStroke_ = draw;
  emit drawPreviewStrokeChanged();
  schedulePaintAndContentUpdate();
}

QVariant PaintLayerItem::normalizedStrokeSize(const QVariant &stroke) const {
  const QVariantMap map = strokeMap(stroke);
  if (!map.contains(QStringLiteral("size")))
    return defaultStrokeSize();
  bool ok = false;
  const qreal value = map.value(QStringLiteral("size")).toDouble(&ok);
  return ok && std::isfinite(value) ? std::max(minimumStrokeSize(), value)
                                    : minimumStrokeSize();
}

QVariant PaintLayerItem::normalizedStrokeOpacity(const QVariant &stroke) const {
  const QVariantMap map = strokeMap(stroke);
  if (!map.contains(QStringLiteral("opacity")))
    return defaultStrokeOpacity();
  bool ok = false;
  const qreal value = map.value(QStringLiteral("opacity")).toDouble(&ok);
  return ok && std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

bool PaintLayerItem::hasDrawableStroke(const QVariant &stroke) const {
  const QVariantMap map = strokeMap(stroke);
  return !map.value(QStringLiteral("points")).toList().isEmpty() &&
         normalizedStrokeOpacity(stroke).toDouble() > 0.0;
}

int PaintLayerItem::drawableStrokeCount(const QVariantList &strokes) const {
  return static_cast<int>(std::count_if(
      strokes.cbegin(), strokes.cend(),
      [this](const QVariant &stroke) { return hasDrawableStroke(stroke); }));
}

int PaintLayerItem::liveStrokeCount() const {
  return (drawPersistedStrokes_ ? drawableStrokeCount(strokes_) : 0) +
         (drawPreviewStroke_ && hasDrawableStroke(previewStroke_) ? 1 : 0);
}

void PaintLayerItem::setLastPaintedStrokeCount(int count) {
  if (lastPaintedStrokeCount_ == count)
    return;
  lastPaintedStrokeCount_ = count;
  emit lastPaintedStrokeCountChanged();
}

void PaintLayerItem::setPaintRevision(int revision) {
  if (paintRevision_ == revision)
    return;
  paintRevision_ = revision;
  emit paintRevisionChanged();
}

void PaintLayerItem::schedulePaint() {
  setPaintRevision(paintRevision_ + 1);
  update();
}

void PaintLayerItem::schedulePaintAndContentUpdate() {
  emit paintContentChanged();
  schedulePaint();
}

bool PaintLayerItem::drawStroke(QPainter &painter,
                                const QVariant &stroke) const {
  if (!hasDrawableStroke(stroke))
    return false;

  const QVariantMap map = strokeMap(stroke);
  const QVariantList points = map.value(QStringLiteral("points")).toList();
  const QPointF first = pointFromVariant(points.constFirst());
  QPainterPath path(first);
  for (qsizetype index = 1; index < points.size(); ++index)
    path.lineTo(pointFromVariant(points.at(index)));
  if (points.size() == 1)
    path.lineTo(first + QPointF(0.1, 0.0));

  QPen pen(strokeColor(map, normalizedStrokeOpacity(stroke).toDouble()),
           normalizedStrokeSize(stroke).toDouble(), Qt::SolidLine, Qt::RoundCap,
           Qt::RoundJoin);
  painter.strokePath(path, pen);
  return true;
}

void PaintLayerItem::paint(QPainter *painter) {
  if (!painter)
    return;
  painter->setRenderHint(QPainter::Antialiasing, true);
  int paintedCount = 0;
  if (drawPersistedStrokes_) {
    for (const QVariant &stroke : strokes_)
      paintedCount += drawStroke(*painter, stroke) ? 1 : 0;
  }
  if (drawPreviewStroke_)
    paintedCount += drawStroke(*painter, previewStroke_) ? 1 : 0;
  setLastPaintedStrokeCount(paintedCount);
  setPaintRevision(paintRevision_ + 1);
}

void PaintLayerItem::attachWindow(QQuickWindow *window) {
  disconnect(sceneGraphInitializedConnection_);
  disconnect(sceneGraphInvalidatedConnection_);
  setRenderTarget(Image);
  if (!window)
    return;

  sceneGraphInitializedConnection_ = connect(
      window, &QQuickWindow::sceneGraphInitialized, this,
      [this, window] {
        QMetaObject::invokeMethod(
            this, [this, window] { selectRenderTarget(window); },
            Qt::QueuedConnection);
      },
      Qt::DirectConnection);
  sceneGraphInvalidatedConnection_ = connect(
      window, &QQuickWindow::sceneGraphInvalidated, this,
      [this] {
        QMetaObject::invokeMethod(
            this, [this] { setRenderTarget(Image); }, Qt::QueuedConnection);
      },
      Qt::DirectConnection);
  if (window->isSceneGraphInitialized())
    selectRenderTarget(window);
}

void PaintLayerItem::selectRenderTarget(QQuickWindow *window) {
  if (window != this->window())
    return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  const bool openGl = window->rendererInterface()->graphicsApi() ==
                      QSGRendererInterface::OpenGL;
  setRenderTarget(openGl ? FramebufferObject : Image);
#else
  Q_UNUSED(window);
  setRenderTarget(Image);
#endif
}

} // namespace textfx
