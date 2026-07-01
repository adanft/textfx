#include "render/RenderTextLayout.h"

#include <QFontMetricsF>
#include <QLineF>
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

  qreal remaining = std::clamp(distance, 0.0, pathLength(points));
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

  const QPointF a = points.at(points.size() - 2);
  const QPointF b = points.last();
  const qreal segment = std::max<qreal>(1.0, QLineF(a, b).length());
  return {b, (b - a) / segment};
}

bool isNeutralFlatPath(const QVector<QPointF> &normalizedPoints) {
  if (normalizedPoints.size() < 2)
    return false;
  if (std::abs(normalizedPoints.first().x()) > 0.0001 ||
      std::abs(normalizedPoints.last().x() - 1.0) > 0.0001)
    return false;
  return std::ranges::all_of(normalizedPoints, [](const QPointF &point) {
    return std::abs(point.y() - 0.5) <= 0.0001;
  });
}

QPainterPath pathTextLayoutPath(const TextLayoutOptions &options,
                                const QFont &font,
                                const QVector<QPointF> &normalizedPathPoints,
                                bool smoothPath, qreal pathLineSpacing) {
  QPainterPath path;
  QStringList lines;
  const QVector<QPointF> guidePoints = layoutPathPoints(
      normalizedPathPoints, options.width, options.height, smoothPath);
  const qreal guideLength = pathLength(guidePoints);
  if (guideLength <= 0.0)
    return textLayoutPath(options, font);

  TextLayoutOptions pathOptions = options;
  pathOptions.width = guideLength;
  pathOptions.height = 0.0;
  textLayoutPath(pathOptions, font, &lines);
  const QFontMetricsF metrics(font);
  const qreal firstLineOffset =
      -static_cast<qreal>(lines.size() - 1) * pathLineSpacing * 0.5;
  for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const QString &line = lines.at(lineIndex);
    qreal distance = std::max<qreal>(
        0.0, (guideLength - metrics.horizontalAdvance(line)) * 0.5);
    const qreal lineOffset =
        firstLineOffset + static_cast<qreal>(lineIndex) * pathLineSpacing;
    for (const QChar ch : line) {
      const qreal advance = metrics.horizontalAdvance(ch);
      const PathSample sample =
          pathSampleAtDistance(guidePoints, distance + advance * 0.5);
      distance += advance;
      if (ch.isSpace())
        continue;
      QPainterPath glyph;
      glyph.addText(-advance / 2.0, 0.0, font, QString(ch));
      const QPointF normal(-sample.tangent.y(), sample.tangent.x());
      QTransform transform;
      transform.translate(sample.point.x() + normal.x() * lineOffset,
                          sample.point.y() + normal.y() * lineOffset);
      transform.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) *
                       180.0 / kPi);
      path.addPath(transform.map(glyph));
    }
  }
  path.setFillRule(Qt::WindingFill);
  return path;
}

} // namespace textfx
