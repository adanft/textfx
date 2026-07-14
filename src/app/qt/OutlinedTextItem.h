#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QStringList>
#include <QVariantList>
#include <QVector>

namespace textfx {

class OutlinedTextItem : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
  Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY
                 fontFamilyChanged)
  Q_PROPERTY(
      qreal pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
  Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
  Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY italicChanged)
  Q_PROPERTY(qreal letterSpacing READ letterSpacing WRITE setLetterSpacing
                 NOTIFY letterSpacingChanged)
  Q_PROPERTY(qreal lineSpacing READ lineSpacing WRITE setLineSpacing NOTIFY
                 lineSpacingChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY
                 outlineColorChanged)
  Q_PROPERTY(qreal outlineSize READ outlineSize WRITE setOutlineSize NOTIFY
                  outlineSizeChanged)
  Q_PROPERTY(QVariantList outlineLayers READ outlineLayers WRITE setOutlineLayers
                 NOTIFY outlineLayersChanged)
  Q_PROPERTY(
      int blurSize READ blurSize WRITE setBlurSize NOTIFY blurSizeChanged)
  Q_PROPERTY(bool shadowEnabled READ shadowEnabled WRITE setShadowEnabled NOTIFY
                 shadowEnabledChanged)
  Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor NOTIFY
                 shadowColorChanged)
  Q_PROPERTY(qreal shadowOffsetX READ shadowOffsetX WRITE setShadowOffsetX
                 NOTIFY shadowOffsetXChanged)
  Q_PROPERTY(qreal shadowOffsetY READ shadowOffsetY WRITE setShadowOffsetY
                 NOTIFY shadowOffsetYChanged)
  Q_PROPERTY(int shadowBlurSize READ shadowBlurSize WRITE setShadowBlurSize
                 NOTIFY shadowBlurSizeChanged)
  Q_PROPERTY(bool gradientEnabled READ gradientEnabled WRITE setGradientEnabled
                 NOTIFY gradientEnabledChanged)
  Q_PROPERTY(int gradientDirection READ gradientDirection WRITE
                 setGradientDirection NOTIFY gradientDirectionChanged)
  Q_PROPERTY(QColor gradientColorA READ gradientColorA WRITE setGradientColorA
                 NOTIFY gradientColorAChanged)
  Q_PROPERTY(QColor gradientColorB READ gradientColorB WRITE setGradientColorB
                 NOTIFY gradientColorBChanged)
  Q_PROPERTY(bool pathEnabled READ pathEnabled WRITE setPathEnabled NOTIFY
                 pathEnabledChanged)
  Q_PROPERTY(
      int pathMode READ pathMode WRITE setPathMode NOTIFY pathModeChanged)
  Q_PROPERTY(QVariantList pathPoints READ pathPoints WRITE setPathPoints NOTIFY
                 pathPointsChanged)
  Q_PROPERTY(qreal renderScale READ renderScale WRITE setRenderScale NOTIFY
                 renderScaleChanged)
  Q_PROPERTY(int horizontalAlignment READ horizontalAlignment WRITE
                 setHorizontalAlignment NOTIFY horizontalAlignmentChanged)
  Q_PROPERTY(bool overflow READ overflow NOTIFY overflowChanged)
  Q_PROPERTY(bool editLayoutMetricsValid READ editLayoutMetricsValid NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutTopPadding READ editLayoutTopPadding NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutLeftPadding READ editLayoutLeftPadding NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutRightPadding READ editLayoutRightPadding NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutTabStopDistance READ editLayoutTabStopDistance
                 NOTIFY editLayoutMetricsChanged)
  Q_PROPERTY(QVariantList editLayoutLineTops READ editLayoutLineTops NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutPaintOffsetX READ editLayoutPaintOffsetX NOTIFY
                 editLayoutMetricsChanged)
  Q_PROPERTY(qreal editLayoutPaintOffsetY READ editLayoutPaintOffsetY NOTIFY
                 editLayoutMetricsChanged)

public:
  explicit OutlinedTextItem(QQuickItem *parent = nullptr);

  QString text() const { return text_; }
  void setText(const QString &value);
  QString fontFamily() const { return fontFamily_; }
  void setFontFamily(const QString &value);
  qreal pixelSize() const { return pixelSize_; }
  void setPixelSize(qreal value);
  bool bold() const { return bold_; }
  void setBold(bool value);
  bool italic() const { return italic_; }
  void setItalic(bool value);
  qreal letterSpacing() const { return letterSpacing_; }
  void setLetterSpacing(qreal value);
  qreal lineSpacing() const { return lineSpacing_; }
  void setLineSpacing(qreal value);
  QColor color() const { return color_; }
  void setColor(const QColor &value);
  QColor outlineColor() const { return outlineColor_; }
  void setOutlineColor(const QColor &value);
  qreal outlineSize() const { return outlineSize_; }
  void setOutlineSize(qreal value);
  QVariantList outlineLayers() const { return outlineLayers_; }
  void setOutlineLayers(const QVariantList &value);
  int blurSize() const { return blurSize_; }
  void setBlurSize(int value);
  bool shadowEnabled() const { return shadowEnabled_; }
  void setShadowEnabled(bool value);
  QColor shadowColor() const { return shadowColor_; }
  void setShadowColor(const QColor &value);
  qreal shadowOffsetX() const { return shadowOffsetX_; }
  void setShadowOffsetX(qreal value);
  qreal shadowOffsetY() const { return shadowOffsetY_; }
  void setShadowOffsetY(qreal value);
  int shadowBlurSize() const { return shadowBlurSize_; }
  void setShadowBlurSize(int value);
  bool gradientEnabled() const { return gradientEnabled_; }
  void setGradientEnabled(bool value);
  int gradientDirection() const { return gradientDirection_; }
  void setGradientDirection(int value);
  QColor gradientColorA() const { return gradientColorA_; }
  void setGradientColorA(const QColor &value);
  QColor gradientColorB() const { return gradientColorB_; }
  void setGradientColorB(const QColor &value);
  bool pathEnabled() const { return pathEnabled_; }
  void setPathEnabled(bool value);
  int pathMode() const { return pathMode_; }
  void setPathMode(int value);
  QVariantList pathPoints() const { return pathPoints_; }
  void setPathPoints(const QVariantList &value);
  qreal renderScale() const { return renderScale_; }
  void setRenderScale(qreal value);
  int horizontalAlignment() const { return horizontalAlignment_; }
  void setHorizontalAlignment(int value);
  bool overflow() const;
  bool editLayoutMetricsValid() const;
  qreal editLayoutTopPadding() const;
  qreal editLayoutLeftPadding() const;
  qreal editLayoutRightPadding() const;
  qreal editLayoutTabStopDistance() const;
  QVariantList editLayoutLineTops() const;
  qreal editLayoutPaintOffsetX() const;
  qreal editLayoutPaintOffsetY() const;

#ifdef TEXTFX_TESTING
  int paintRequestRevisionForTesting() const { return paintRequestRevision_; }
  bool polishPendingForTesting() const { return polishPending_; }
  int layoutCacheRebuildCountForTesting() const;
  void processPendingPolishForTesting() { updatePolish(); }
  int outlineStrokeRebuildCountForTesting() const {
    return outlineStrokeRebuildCount_;
  }
  int shadowBlurRebuildCountForTesting() const {
    return shadowBlurRebuildCount_;
  }
  qreal effectiveMaxOutlineSizeForTesting() const {
    return effectiveMaxOutlineSize();
  }
  QStringList wrappedLinesForTesting() const;
  QPointF pathBaselinePointForTesting(qreal distance, qreal layoutWidth,
                                      qreal layoutHeight) const;
#endif

  void paint(QPainter *painter) override;

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  void updatePolish() override;

signals:
  void textChanged();
  void fontFamilyChanged();
  void pixelSizeChanged();
  void boldChanged();
  void italicChanged();
  void letterSpacingChanged();
  void lineSpacingChanged();
  void colorChanged();
  void outlineColorChanged();
  void outlineSizeChanged();
  void outlineLayersChanged();
  void blurSizeChanged();
  void shadowEnabledChanged();
  void shadowColorChanged();
  void shadowOffsetXChanged();
  void shadowOffsetYChanged();
  void shadowBlurSizeChanged();
  void gradientEnabledChanged();
  void gradientDirectionChanged();
  void gradientColorAChanged();
  void gradientColorBChanged();
  void pathEnabledChanged();
  void pathModeChanged();
  void pathPointsChanged();
  void renderScaleChanged();
  void horizontalAlignmentChanged();
  void overflowChanged();
  void editLayoutMetricsChanged();
#ifdef TEXTFX_TESTING
  void paintRequestRevisionChangedForTesting();
#endif

private:
  struct LayoutCache {
    QFont font;
    qreal scale = 1.0;
    qreal layoutWidth = 1.0;
    qreal layoutHeight = 1.0;
    qreal maxOutline = 0.0;
    qreal inset = 0.0;
    qreal blockHeight = 0.0;
    qreal tabStopDistance = 0.0;
    bool usingPathText = false;
    bool editMetricsValid = true;
    QPainterPath path;
    QRectF paintedBounds;
    QPointF paintTranslation;
    QPainterPath translatedPath;
    QRectF translatedPaintedBounds;
    QVector<qreal> lineTops;
#ifdef TEXTFX_TESTING
    QStringList wrappedLines;
#endif
  };

  QFont layoutFont() const;
  const LayoutCache &layoutCache() const;
  void ensureLayoutCurrent() const;
  void processPendingUpdates();
  void invalidateLayoutCache();
  void updateOverflow();
  void notifyLayoutChanged();
  void requestPaintRefresh();
  qreal effectiveMaxOutlineSize() const;
  QPointF paintTranslationForCurrentLayout() const;
  struct OutlineCache {
    quint64 geometryRevision = 0;
    QVector<qreal> widths;
    QVector<QPainterPath> strokes;
  };
  struct ShadowCache {
    quint64 geometryRevision = 0;
    QPointF offset;
    QColor color;
    qreal scale = 0.0;
    int radius = -1;
    QRect sourceRect;
    QPainterPath path;
    QImage image;
  };
  struct BlurCacheKey {
    quint64 renderRevision = 0;
    int radius = -1;
    QRect sourceRect;
  };
  QString text_;
  QString fontFamily_;
  qreal pixelSize_ = 12.0;
  bool bold_ = false;
  bool italic_ = false;
  qreal letterSpacing_ = 0.0;
  qreal lineSpacing_ = 0.0;
  QColor color_ = Qt::black;
  QColor outlineColor_ = Qt::white;
  qreal outlineSize_ = 0.0;
  QVariantList outlineLayers_;
  int blurSize_ = 0;
  bool shadowEnabled_ = false;
  QColor shadowColor_ = QColor(0, 0, 0, 160);
  qreal shadowOffsetX_ = 0.0;
  qreal shadowOffsetY_ = 0.0;
  int shadowBlurSize_ = 0;
  bool gradientEnabled_ = false;
  int gradientDirection_ = 0;
  QColor gradientColorA_ = Qt::white;
  QColor gradientColorB_ = Qt::black;
  bool pathEnabled_ = false;
  int pathMode_ = 0;
  QVariantList pathPoints_;
  qreal renderScale_ = 1.0;
  int horizontalAlignment_ = Qt::AlignLeft;
  bool overflow_ = false;
  mutable bool layoutCacheDirty_ = true;
  mutable bool ensuringLayoutCurrent_ = false;
  bool layoutNotificationPending_ = false;
  bool paintRefreshPending_ = false;
  bool polishPending_ = false;
  mutable LayoutCache layoutCache_;
  quint64 geometryRevision_ = 1;
  quint64 renderRevision_ = 1;
  OutlineCache outlineCache_;
  ShadowCache shadowCache_;
#ifdef TEXTFX_TESTING
  mutable int layoutCacheRebuildCount_ = 0;
  int outlineStrokeRebuildCount_ = 0;
  int shadowBlurRebuildCount_ = 0;
#endif
  BlurCacheKey blurCacheKey_;
  QImage blurCacheImage_;
#ifdef TEXTFX_TESTING
  int paintRequestRevision_ = 0;
#endif
};

} // namespace textfx
