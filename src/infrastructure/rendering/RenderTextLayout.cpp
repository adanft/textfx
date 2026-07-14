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

#ifdef TEXTFX_ENABLE_TEST_HOOKS
int textDocumentLayoutCount = 0;
#endif

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
#ifdef TEXTFX_ENABLE_TEST_HOOKS
  ++textDocumentLayoutCount;
#endif
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
  return prepareTextLayout(options, font).blockHeight;
}

PreparedTextLayout prepareTextLayout(const TextLayoutOptions &options,
                                     const QFont &font) {
  PreparedTextLayout result;
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
      result.blockHeight =
          std::max(result.blockHeight,
                   blockPosition.y() + line.y() + line.height());
      lines.append({blockText.mid(line.textStart(), line.textLength()),
                    options.inset + line.x() +
                        horizontalTextOffset(
                            line,
                            std::max<qreal>(
                                1.0, options.width - options.inset * 2.0),
                            options.horizontalAlignment),
                    blockPosition.y() + line.y(),
                    line.ascent(), line.height()});
      for (const QGlyphRun &run :
           line.glyphRuns(line.textStart(), line.textLength())) {
        const QVector<quint32> indexes = run.glyphIndexes();
        const QVector<QPointF> positions = run.positions();
        const qsizetype count = std::min(indexes.size(), positions.size());
        for (qsizetype index = 0; index < count; ++index) {
          const QPainterPath outline = run.rawFont().pathForGlyph(indexes.at(index));
          if (!outline.isEmpty())
            result.glyphs.append({outline, blockPosition + positions.at(index)});
        }
      }
    }
  }
  const qreal y =
      options.inset +
      std::max<qreal>(
          0.0,
          (options.height - options.inset * 2.0 - result.blockHeight) / 2.0);
  for (PreparedTextGlyph &glyph : result.glyphs)
    glyph.origin += QPointF(options.inset, y);
  for (const LaidOutLine &line : lines) {
    const qreal baseline = y + line.y + line.ascent;
    result.lineTexts.append(line.text);
    result.lineXs.append(line.x);
    result.lineBaselines.append(baseline);
    result.lineTops.append(y + line.y);
    result.plainPath.addText(line.x, baseline, font, line.text);
  }
  result.plainPath.setFillRule(Qt::WindingFill);
  return result;
}

QPainterPath textLayoutPath(const TextLayoutOptions &options, const QFont &font,
                            QStringList *lineTexts, QVector<qreal> *lineXs,
                            QVector<qreal> *lineBaselines,
                            QVector<qreal> *lineTops) {
  const PreparedTextLayout layout = prepareTextLayout(options, font);
  if (lineTexts)
    lineTexts->append(layout.lineTexts);
  if (lineXs)
    lineXs->append(layout.lineXs);
  if (lineBaselines)
    lineBaselines->append(layout.lineBaselines);
  if (lineTops)
    lineTops->append(layout.lineTops);
  return layout.plainPath;
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
  return pathTextLayoutPath(prepareTextLayout(options, font), options,
                            normalizedPathPoints, smoothPath);
}

QPainterPath pathTextLayoutPath(const PreparedTextLayout &layout,
                                const TextLayoutOptions &options,
                                const QVector<QPointF> &normalizedPathPoints,
                                bool smoothPath) {
  QPainterPath path;
  const QVector<QPointF> guidePoints = layoutPathPoints(
      normalizedPathPoints, options.width, options.height, smoothPath);
  const qreal guideLength = pathLength(guidePoints);
  if (guideLength <= 0.0)
    return layout.plainPath;

  const QPointF neutralGuideOrigin(0.0, options.height * 0.5);
  for (const PreparedTextGlyph &glyph : layout.glyphs) {
    const PathSample sample =
        pathSampleAtDistance(guidePoints, glyph.origin.x());
    const QPointF normal(-sample.tangent.y(), sample.tangent.x());
    const QPointF mappedOrigin =
        sample.point + normal * (glyph.origin.y() - neutralGuideOrigin.y());
    QTransform transform;
    transform.translate(mappedOrigin.x(), mappedOrigin.y());
    transform.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) * 180.0 /
                     kPi);
    path.addPath(transform.map(glyph.outline));
  }
  path.setFillRule(Qt::WindingFill);
  return path;
}

#ifdef TEXTFX_ENABLE_TEST_HOOKS
void resetTextDocumentLayoutCountForTesting() { textDocumentLayoutCount = 0; }
int textDocumentLayoutCountForTesting() { return textDocumentLayoutCount; }
#endif

} // namespace textfx
