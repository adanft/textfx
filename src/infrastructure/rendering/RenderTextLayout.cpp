#include "infrastructure/rendering/RenderTextLayout.h"

#include <QFontMetricsF>
#include <QGlyphRun>
#include <QLineF>
#include <QRawFont>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <memory>

namespace textfx {
namespace {
constexpr qreal kPi = 3.14159265358979323846;

struct LaidOutLine {
  QString text;
  qreal x = 0.0;
  qreal y = 0.0;
  qreal ascent = 0.0;
  qreal height = 0.0;
};

qreal horizontalTextOffset(const QTextLine &line, qreal paintWidth,
                           int horizontalAlignment) {
  const qreal extra = std::max<qreal>(0.0, paintWidth - line.naturalTextWidth());
  const Qt::Alignment alignment =
      static_cast<Qt::Alignment>(horizontalAlignment & Qt::AlignHorizontal_Mask);
  if (alignment & Qt::AlignRight)
    return extra;
  if (alignment & Qt::AlignHCenter)
    return extra / 2.0;
  return 0.0;
}

QTextOption textLayoutOption(const QFont &font, int horizontalAlignment) {
  QTextOption option;
  option.setWrapMode(QTextOption::WordWrap);
  option.setTabStopDistance(textLayoutTabStopDistance(font));
  option.setAlignment(static_cast<Qt::Alignment>(horizontalAlignment));
  return option;
}

std::unique_ptr<QTextDocument> laidOutDocument(const TextLayoutOptions &options,
                                               const QFont &font) {
  auto document = std::make_unique<QTextDocument>();
  document->setUndoRedoEnabled(false);
  document->setDocumentMargin(0.0);
  document->setDefaultFont(font);
  document->setDefaultTextOption(
      textLayoutOption(font, options.horizontalAlignment));
  document->setPlainText(options.text);
  const qreal paintWidth =
      std::max<qreal>(1.0, options.width - options.inset * 2.0);
  document->setTextWidth(paintWidth);

  QTextCursor cursor(document.get());
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    QTextBlockFormat format = block.blockFormat();
    format.setAlignment(static_cast<Qt::Alignment>(
        options.horizontalAlignment & Qt::AlignHorizontal_Mask));
    format.setLineHeight(options.lineSpacing,
                         QTextBlockFormat::LineDistanceHeight);
    cursor.setPosition(block.position());
    cursor.mergeBlockFormat(format);
  }
  document->adjustSize();
  document->setTextWidth(paintWidth);
  return document;
}
} // namespace

qreal textLayoutTabStopDistance(const QFont &font) {
  return std::max<qreal>(
      1.0,
      std::round(QFontMetricsF(font).horizontalAdvance(QStringLiteral("    "))));
}

qreal textLayoutBlockHeight(const TextLayoutOptions &options,
                            const QFont &font) {
  const std::unique_ptr<QTextDocument> document = laidOutDocument(options, font);
  qreal blockHeight = 0.0;
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    const QTextLayout *layout = block.layout();
    if (!layout)
      continue;
    const QPointF blockPosition = layout->position();
    for (int i = 0; i < layout->lineCount(); ++i) {
      const QTextLine line = layout->lineAt(i);
      if (line.isValid())
        blockHeight = std::max(blockHeight,
                               blockPosition.y() + line.y() + line.height());
    }
  }
  return blockHeight;
}

QPainterPath textLayoutPath(const TextLayoutOptions &options, const QFont &font,
                            QStringList *lineTexts, QVector<qreal> *lineXs,
                            QVector<qreal> *lineBaselines,
                            QVector<qreal> *lineTops) {
  QPainterPath path;
  QVector<LaidOutLine> lines;
  const std::unique_ptr<QTextDocument> document = laidOutDocument(options, font);
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    const QTextLayout *layout = block.layout();
    if (!layout)
      continue;
    const QPointF blockPosition = layout->position();
    const QString blockText = block.text();
    for (int i = 0; i < layout->lineCount(); ++i) {
      const QTextLine line = layout->lineAt(i);
      if (!line.isValid())
        continue;
      lines.append({blockText.mid(line.textStart(), line.textLength()),
                    options.inset + line.x() +
                        horizontalTextOffset(
                            line,
                            std::max<qreal>(
                                1.0, options.width - options.inset * 2.0),
                            options.horizontalAlignment),
                    blockPosition.y() + line.y(),
                    line.ascent(), line.height()});
    }
  }
  const qreal blockHeight = textLayoutBlockHeight(options, font);
  qreal y =
      options.inset +
      std::max<qreal>(
          0.0, (options.height - options.inset * 2.0 - blockHeight) / 2.0);
  for (const LaidOutLine &line : lines) {
    const qreal baseline = y + line.y + line.ascent;
    if (lineTexts)
      lineTexts->append(line.text);
    if (lineXs)
      lineXs->append(line.x);
    if (lineBaselines)
      lineBaselines->append(baseline);
    if (lineTops)
      lineTops->append(y + line.y);
    path.addText(line.x, baseline, font, line.text);
  }
  path.setFillRule(Qt::WindingFill);
  return path;
}

QVector<QPointF> layoutPathPoints(const QVector<QPointF> &normalizedPoints,
                                  qreal layoutWidth, qreal layoutHeight,
                                  bool smooth) {
  QVector<QPointF> result;
  result.reserve(normalizedPoints.size());
  for (const QPointF &point : normalizedPoints)
    result.append({layoutWidth * point.x(), layoutHeight * point.y()});

  for (int pass = 0; smooth && pass < 3 && result.size() >= 3; ++pass) {
    QVector<QPointF> next;
    next.reserve(result.size() * 2);
    next.append(result.first());
    for (qsizetype i = 0; i + 1 < result.size(); ++i) {
      const QPointF from = result.at(i);
      const QPointF to = result.at(i + 1);
      next.append(from + (to - from) * 0.25);
      next.append(from + (to - from) * 0.75);
    }
    next.append(result.last());
    result = next;
  }
  return result;
}

qreal pathLength(const QVector<QPointF> &points) {
  qreal result = 0.0;
  for (qsizetype i = 0; i + 1 < points.size(); ++i)
    result += QLineF(points.at(i), points.at(i + 1)).length();
  return result;
}

PathSample pathSampleAtDistance(const QVector<QPointF> &points,
                                qreal distance) {
  if (points.isEmpty())
    return {};
  if (points.size() == 1)
    return {points.first(), {1.0, 0.0}};

  qsizetype firstSegmentIndex = -1;
  qsizetype lastSegmentIndex = -1;
  for (qsizetype i = 0; i + 1 < points.size(); ++i) {
    if (QLineF(points.at(i), points.at(i + 1)).length() <= 0.0)
      continue;
    if (firstSegmentIndex < 0)
      firstSegmentIndex = i;
    lastSegmentIndex = i;
  }
  if (firstSegmentIndex < 0)
    return {points.first(), {1.0, 0.0}};

  const QPointF firstA = points.at(firstSegmentIndex);
  const QPointF firstB = points.at(firstSegmentIndex + 1);
  const QPointF firstTangent =
      (firstB - firstA) / QLineF(firstA, firstB).length();
  if (distance < 0.0)
    return {firstA + firstTangent * distance, firstTangent};

  qreal remaining = distance;
  for (qsizetype i = 0; i + 1 < points.size(); ++i) {
    const QPointF a = points.at(i);
    const QPointF b = points.at(i + 1);
    const qreal segment = QLineF(a, b).length();
    if (segment <= 0.0)
      continue;
    const QPointF tangent = (b - a) / segment;
    if (remaining <= segment)
      return {a + tangent * remaining, tangent};
    remaining -= segment;
  }

  const QPointF lastA = points.at(lastSegmentIndex);
  const QPointF lastB = points.at(lastSegmentIndex + 1);
  const qreal lastLength = QLineF(lastA, lastB).length();
  const QPointF lastTangent = (lastB - lastA) / lastLength;
  return {lastB + lastTangent * remaining, lastTangent};
}

QPainterPath pathTextLayoutPath(const TextLayoutOptions &options,
                                const QFont &font,
                                const QVector<QPointF> &normalizedPathPoints,
                                bool smoothPath) {
  QPainterPath path;
  const QVector<QPointF> guidePoints = layoutPathPoints(
      normalizedPathPoints, options.width, options.height, smoothPath);
  const qreal guideLength = pathLength(guidePoints);
  if (guideLength <= 0.0)
    return textLayoutPath(options, font);

  const std::unique_ptr<QTextDocument> document = laidOutDocument(options, font);
  const qreal blockHeight = textLayoutBlockHeight(options, font);
  const qreal documentTop =
      options.inset +
      std::max<qreal>(
          0.0, (options.height - options.inset * 2.0 - blockHeight) / 2.0);
  const QPointF neutralGuideOrigin(0.0, options.height * 0.5);
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    const QTextLayout *layout = block.layout();
    if (!layout)
      continue;
    const QPointF layoutOrigin(options.inset, documentTop + layout->position().y());
    for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex) {
      const QTextLine line = layout->lineAt(lineIndex);
      if (!line.isValid())
        continue;
      for (const QGlyphRun &run :
           line.glyphRuns(line.textStart(), line.textLength())) {
        const QVector<quint32> indexes = run.glyphIndexes();
        const QVector<QPointF> positions = run.positions();
        const qsizetype count = std::min(indexes.size(), positions.size());
        for (qsizetype glyphIndex = 0; glyphIndex < count; ++glyphIndex) {
          const QPainterPath outline =
              run.rawFont().pathForGlyph(indexes.at(glyphIndex));
          if (outline.isEmpty())
            continue;
          const QPointF glyphOrigin = layoutOrigin + positions.at(glyphIndex);
          const PathSample sample =
              pathSampleAtDistance(guidePoints, glyphOrigin.x());
          const QPointF normal(-sample.tangent.y(), sample.tangent.x());
          const QPointF mappedOrigin =
              sample.point + normal * (glyphOrigin.y() - neutralGuideOrigin.y());
          QTransform transform;
          transform.translate(mappedOrigin.x(), mappedOrigin.y());
          transform.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) *
                           180.0 / kPi);
          path.addPath(transform.map(outline));
        }
      }
    }
  }
  path.setFillRule(Qt::WindingFill);
  return path;
}

} // namespace textfx
