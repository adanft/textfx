#include "render/RenderTextLayout.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QTextLayout>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace textfx {
namespace {
constexpr qreal kPi = 3.14159265358979323846;

struct LaidOutLine {
  QString text;
  qreal x = 0.0;
  qreal ascent = 0.0;
  qreal height = 0.0;
};
} // namespace

QPainterPath textLayoutPath(const TextLayoutOptions &options, const QFont &font,
                            QStringList *lineTexts, QVector<qreal> *lineXs,
                            QVector<qreal> *lineBaselines) {
  QPainterPath path;
  QVector<LaidOutLine> lines;
  QString text = options.text;
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(u'\r', u'\n');
  const QStringList paragraphs = text.split(u'\n');
  const qreal paintWidth =
      std::max<qreal>(1.0, options.width - options.inset * 2.0);
  for (const QString &paragraph : paragraphs) {
    QTextLayout layout(paragraph.isEmpty() ? QStringLiteral(" ") : paragraph,
                       font);
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    while (true) {
      QTextLine line = layout.createLine();
      if (!line.isValid())
        break;
      line.setLineWidth(paintWidth);
      qreal x = options.inset;
      if (options.horizontalAlignment == Qt::AlignHCenter)
        x = options.inset + (paintWidth - line.naturalTextWidth()) / 2.0;
      if (options.horizontalAlignment == Qt::AlignRight)
        x = options.inset + paintWidth - line.naturalTextWidth();
      lines.append({paragraph.mid(line.textStart(), line.textLength()), x,
                    line.ascent(), line.height()});
    }
    layout.endLayout();
  }
  qreal blockHeight = 0.0;
  for (qsizetype i = 0; i < lines.size(); ++i)
    blockHeight +=
        lines.at(i).height + (i + 1 < lines.size() ? options.lineSpacing : 0.0);
  qreal y =
      options.inset +
      std::max<qreal>(
          0.0, (options.height - options.inset * 2.0 - blockHeight) / 2.0);
  for (const LaidOutLine &line : lines) {
    const qreal baseline = y + line.ascent;
    if (lineTexts)
      lineTexts->append(line.text);
    if (lineXs)
      lineXs->append(line.x);
    if (lineBaselines)
      lineBaselines->append(baseline);
    path.addText(line.x, baseline, font, line.text);
    y += line.height + options.lineSpacing;
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
