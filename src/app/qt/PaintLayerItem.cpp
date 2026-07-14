#include "app/qt/PaintLayerItem.h"

#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QJSValue>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

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

struct PaintLayerItem::RenderStats {
  explicit RenderStats(bool enabled) : enabled(enabled) {}

  const bool enabled;
  std::atomic<quint64> paintCount{0};
  std::atomic<quint64> totalNanoseconds{0};
  std::atomic<quint64> maximumNanoseconds{0};
  std::atomic<int> latestStrokeCount{0};
};

PaintLayerItem::PaintLayerItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
  paintSnapshot_.store(std::make_shared<const PaintSnapshot>());
  renderStats_ = std::make_shared<RenderStats>(
      qEnvironmentVariableIntValue("TEXTFX_RENDER_STATS") == 1);
  setAntialiasing(true);
  connect(this, &QQuickItem::windowChanged, this,
          &PaintLayerItem::attachWindow);
  connect(this, &QQuickItem::widthChanged, this, &PaintLayerItem::schedulePaint);
  connect(this, &QQuickItem::heightChanged, this,
          &PaintLayerItem::schedulePaint);
  if (renderStats_->enabled) {
    auto *timer = new QTimer(this);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, this, &PaintLayerItem::reportRenderStats);
    timer->start();
  }
}

void PaintLayerItem::setStrokes(const QVariantList &strokes) {
  if (strokes_ == strokes)
    return;
  strokes_ = strokes;
  persistedDrawableStrokeCount_ = drawableStrokeCount(strokes_);
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
  previewStrokeDrawable_ = hasDrawableStroke(previewStroke_);
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
  return (drawPersistedStrokes_ ? persistedDrawableStrokeCount_ : 0) +
         (drawPreviewStroke_ && previewStrokeDrawable_ ? 1 : 0);
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
  preparePaintSnapshot();
  setPaintRevision(paintRevision_ + 1);
  update();
}

void PaintLayerItem::schedulePaintAndContentUpdate() {
  emit paintContentChanged();
  schedulePaint();
}

std::optional<PaintLayerItem::PaintStrokeSnapshot>
PaintLayerItem::strokeSnapshot(const QVariant &stroke) const {
  if (!hasDrawableStroke(stroke))
    return std::nullopt;

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
  return PaintStrokeSnapshot{std::move(path), std::move(pen)};
}

void PaintLayerItem::preparePaintSnapshot() {
  auto snapshot = std::make_shared<PaintSnapshot>();
  snapshot->strokes.reserve(static_cast<size_t>(liveStrokeCount()));
  if (drawPersistedStrokes_) {
    for (const QVariant &stroke : strokes_) {
      if (auto prepared = strokeSnapshot(stroke))
        snapshot->strokes.push_back(std::move(*prepared));
    }
  }
  if (drawPreviewStroke_) {
    if (auto prepared = strokeSnapshot(previewStroke_))
      snapshot->strokes.push_back(std::move(*prepared));
  }
  const int strokeCount = static_cast<int>(snapshot->strokes.size());
  paintSnapshot_.store(
      std::shared_ptr<const PaintSnapshot>(std::move(snapshot)),
      std::memory_order_release);
  setLastPaintedStrokeCount(strokeCount);
}

void PaintLayerItem::paint(QPainter *painter) {
  if (!painter)
    return;
  QElapsedTimer timer;
  if (renderStats_->enabled)
    timer.start();

  painter->setRenderHint(QPainter::Antialiasing, true);
  const auto snapshot = paintSnapshot_.load(std::memory_order_acquire);
  for (const PaintStrokeSnapshot &stroke : snapshot->strokes)
    painter->strokePath(stroke.path, stroke.pen);

  if (renderStats_->enabled) {
    const auto stats = renderStats_;
    const quint64 elapsed = static_cast<quint64>(timer.nsecsElapsed());
    stats->paintCount.fetch_add(1, std::memory_order_relaxed);
    stats->totalNanoseconds.fetch_add(elapsed, std::memory_order_relaxed);
    stats->latestStrokeCount.store(static_cast<int>(snapshot->strokes.size()),
                                   std::memory_order_relaxed);
    quint64 maximum = stats->maximumNanoseconds.load(std::memory_order_relaxed);
    while (maximum < elapsed &&
           !stats->maximumNanoseconds.compare_exchange_weak(
               maximum, elapsed, std::memory_order_relaxed)) {
    }
  }
}

void PaintLayerItem::reportRenderStats() {
  if (!renderStats_->enabled)
    return;
  const quint64 count = renderStats_->paintCount.load();
  if (count == reportedPaintCount_)
    return;
  reportedPaintCount_ = count;
  const quint64 total = renderStats_->totalNanoseconds.load();
  const quint64 maximum = renderStats_->maximumNanoseconds.load();
  QString backend = QStringLiteral("unavailable");
  if (window() && window()->isSceneGraphInitialized()) {
    backend = QString::number(
        static_cast<int>(window()->rendererInterface()->graphicsApi()));
  }
  qInfo().nospace() << "PaintLayerItem stats: count=" << count
                    << " total_ms=" << total / 1000000.0
                    << " avg_ms=" << total / (count * 1000000.0)
                    << " max_ms=" << maximum / 1000000.0
                    << " strokes=" << renderStats_->latestStrokeCount.load()
                    << " target=" << static_cast<int>(renderTarget())
                    << " backend=" << backend;
}

#ifdef TEXTFX_ENABLE_TEST_HOOKS
bool PaintLayerItem::renderStatsEnabledForTesting() const {
  return renderStats_->enabled;
}

QVariantMap PaintLayerItem::renderStatsForTesting() const {
  return {{QStringLiteral("paintCount"),
           QVariant::fromValue(renderStats_->paintCount.load())},
          {QStringLiteral("totalNanoseconds"),
           QVariant::fromValue(renderStats_->totalNanoseconds.load())},
          {QStringLiteral("maximumNanoseconds"),
           QVariant::fromValue(renderStats_->maximumNanoseconds.load())},
          {QStringLiteral("latestStrokeCount"),
           renderStats_->latestStrokeCount.load()}};
}

void PaintLayerItem::reportRenderStatsForTesting() { reportRenderStats(); }
#endif

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
