#pragma once

#include <QMetaObject>
#include <QPainterPath>
#include <QPen>
#include <QQuickPaintedItem>
#include <QVariant>
#include <QVariantList>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace textfx {

class PaintLayerItem : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList strokes READ strokes WRITE setStrokes NOTIFY
                 strokesChanged)
  Q_PROPERTY(QVariant previewStroke READ previewStroke WRITE setPreviewStroke
                 NOTIFY previewStrokeChanged)
  Q_PROPERTY(bool drawPersistedStrokes READ drawPersistedStrokes WRITE
                 setDrawPersistedStrokes NOTIFY drawPersistedStrokesChanged)
  Q_PROPERTY(bool drawPreviewStroke READ drawPreviewStroke WRITE
                 setDrawPreviewStroke NOTIFY drawPreviewStrokeChanged)
  Q_PROPERTY(bool hasPaintContent READ hasPaintContent NOTIFY paintContentChanged)
  Q_PROPERTY(int liveStrokeCount READ liveStrokeCount NOTIFY paintContentChanged)
  Q_PROPERTY(int lastPaintedStrokeCount READ lastPaintedStrokeCount WRITE
                 setLastPaintedStrokeCount NOTIFY lastPaintedStrokeCountChanged)
  Q_PROPERTY(int paintRevision READ paintRevision WRITE setPaintRevision NOTIFY
                 paintRevisionChanged)
  Q_PROPERTY(qreal defaultStrokeSize READ defaultStrokeSize CONSTANT)
  Q_PROPERTY(qreal minimumStrokeSize READ minimumStrokeSize CONSTANT)
  Q_PROPERTY(qreal defaultStrokeOpacity READ defaultStrokeOpacity CONSTANT)

public:
  explicit PaintLayerItem(QQuickItem *parent = nullptr);

  QVariantList strokes() const { return strokes_; }
  void setStrokes(const QVariantList &strokes);
  QVariant previewStroke() const { return previewStroke_; }
  void setPreviewStroke(const QVariant &stroke);
  bool drawPersistedStrokes() const { return drawPersistedStrokes_; }
  void setDrawPersistedStrokes(bool draw);
  bool drawPreviewStroke() const { return drawPreviewStroke_; }
  void setDrawPreviewStroke(bool draw);
  bool hasPaintContent() const { return liveStrokeCount() > 0; }
  int liveStrokeCount() const;
  int lastPaintedStrokeCount() const { return lastPaintedStrokeCount_; }
  void setLastPaintedStrokeCount(int count);
  int paintRevision() const { return paintRevision_; }
  void setPaintRevision(int revision);
  qreal defaultStrokeSize() const { return 12.0; }
  qreal minimumStrokeSize() const { return 1.0; }
  qreal defaultStrokeOpacity() const { return 1.0; }

  Q_INVOKABLE QVariant normalizedStrokeSize(const QVariant &stroke) const;
  Q_INVOKABLE QVariant normalizedStrokeOpacity(const QVariant &stroke) const;
  Q_INVOKABLE bool hasDrawableStroke(const QVariant &stroke) const;
  Q_INVOKABLE int drawableStrokeCount(const QVariantList &strokes) const;
  Q_INVOKABLE void schedulePaint();

  void paint(QPainter *painter) override;

#ifdef TEXTFX_ENABLE_TEST_HOOKS
  bool renderStatsEnabledForTesting() const;
  QVariantMap renderStatsForTesting() const;
  void reportRenderStatsForTesting();
#endif

signals:
  void strokesChanged();
  void previewStrokeChanged();
  void drawPersistedStrokesChanged();
  void drawPreviewStrokeChanged();
  void paintContentChanged();
  void lastPaintedStrokeCountChanged();
  void paintRevisionChanged();

private:
  struct PaintStrokeSnapshot {
    QPainterPath path;
    QPen pen;
  };
  struct PaintSnapshot {
    std::vector<PaintStrokeSnapshot> strokes;
  };
  struct RenderStats;

  void attachWindow(QQuickWindow *window);
  void selectRenderTarget(QQuickWindow *window);
  std::optional<PaintStrokeSnapshot>
  strokeSnapshot(const QVariant &stroke) const;
  void preparePaintSnapshot();
  void reportRenderStats();
  void schedulePaintAndContentUpdate();

  QVariantList strokes_;
  QVariant previewStroke_;
  bool drawPersistedStrokes_ = true;
  bool drawPreviewStroke_ = true;
  int persistedDrawableStrokeCount_ = 0;
  bool previewStrokeDrawable_ = false;
  int lastPaintedStrokeCount_ = 0;
  int paintRevision_ = 0;
  quint64 reportedPaintCount_ = 0;
  std::atomic<std::shared_ptr<const PaintSnapshot>> paintSnapshot_;
  std::shared_ptr<RenderStats> renderStats_;
  QMetaObject::Connection sceneGraphInitializedConnection_;
  QMetaObject::Connection sceneGraphInvalidatedConnection_;
};

} // namespace textfx
