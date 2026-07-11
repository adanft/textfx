#pragma once

#include <QFont>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <Qt>

namespace textfx {

struct TextLayoutOptions {
  QString text;
  qreal width = 1.0;
  qreal height = 1.0;
  qreal inset = 0.0;
  qreal lineSpacing = 0.0;
  int horizontalAlignment = Qt::AlignLeft;
};

qreal textLayoutTabStopDistance(const QFont &font);
qreal textLayoutBlockHeight(const TextLayoutOptions &options,
                            const QFont &font);

struct PathSample {
  QPointF point;
  QPointF tangent = {1.0, 0.0};
};

QPainterPath textLayoutPath(const TextLayoutOptions &options, const QFont &font,
                            QStringList *lineTexts = nullptr,
                            QVector<qreal> *lineXs = nullptr,
                            QVector<qreal> *lineBaselines = nullptr,
                            QVector<qreal> *lineTops = nullptr);
QPainterPath pathTextLayoutPath(const TextLayoutOptions &options,
                                const QFont &font,
                                const QVector<QPointF> &normalizedPathPoints,
                                bool smoothPath);

QVector<QPointF> layoutPathPoints(const QVector<QPointF> &normalizedPoints,
                                  qreal layoutWidth, qreal layoutHeight,
                                  bool smooth);
qreal pathLength(const QVector<QPointF> &points);
PathSample pathSampleAtDistance(const QVector<QPointF> &points, qreal distance);

} // namespace textfx
