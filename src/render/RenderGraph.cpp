#include "render/RenderGraph.h"

#include "core/EffectLimits.h"
#include "core/GaussianBlur.h"
#include "fonts/FontResolver.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QString>
#include <QStringList>
#include <QTextLayout>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace textfx {
namespace {
QColor toQColor(const std::string& color)
{
    if (color.size() != 8) return QColor(0, 0, 0, 255);
    const auto byte = [&](std::size_t i) { return std::stoi(color.substr(i, 2), nullptr, 16); };
    return QColor(byte(0), byte(2), byte(4), byte(6));
}

QFont qFontFor(const TextBox& box)
{
    QFont font(QString::fromStdString(box.style.fontFamily));
    font.setPixelSize(std::max(1, box.style.fontSize));
    font.setBold(box.style.bold);
    font.setItalic(box.style.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing, box.style.letterSpacing);
    return resolveFont(font).font;
}

QPainterPath textPathFor(const TextBox& box, const QFont& font, double inset, QStringList* lineTexts = nullptr, QVector<qreal>* lineXs = nullptr, QVector<qreal>* lineBaselines = nullptr)
{
    struct LaidOutLine {
        QString text;
        qreal x = 0.0;
        qreal ascent = 0.0;
        qreal height = 0.0;
    };

    QPainterPath path;
    QVector<LaidOutLine> lines;
    auto text = box.style.uppercase ? QString::fromStdString(box.text).toUpper() : QString::fromStdString(box.text);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');
    const QStringList paragraphs = text.split(u'\n');
    const qreal paintWidth = std::max<qreal>(1.0, box.bounds.w - inset * 2.0);
    for (const QString& paragraph : paragraphs) {
        QTextLayout layout(paragraph.isEmpty() ? QStringLiteral(" ") : paragraph, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(paintWidth);
            qreal x = inset;
            if (box.style.alignment == TextAlignment::Center) x = inset + (paintWidth - line.naturalTextWidth()) / 2.0;
            if (box.style.alignment == TextAlignment::Right) x = inset + paintWidth - line.naturalTextWidth();
            lines.append({paragraph.mid(line.textStart(), line.textLength()), x, line.ascent(), line.height()});
        }
        layout.endLayout();
    }
    qreal blockHeight = 0.0;
    for (qsizetype i = 0; i < lines.size(); ++i) blockHeight += lines.at(i).height + (i + 1 < lines.size() ? box.style.lineSpacing : 0.0);
    qreal y = inset + std::max<qreal>(0.0, (box.bounds.h - inset * 2.0 - blockHeight) / 2.0);
    for (const LaidOutLine& line : lines) {
        const qreal baseline = y + line.ascent;
        if (lineTexts) lineTexts->append(line.text);
        if (lineXs) lineXs->append(line.x);
        if (lineBaselines) lineBaselines->append(baseline);
        path.addText(line.x, baseline, font, line.text);
        y += line.height + box.style.lineSpacing;
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

QVector<QPointF> layoutPathPoints(const std::vector<Point>& points, double width, double height, bool smooth)
{
    QVector<QPointF> result;
    result.reserve(static_cast<qsizetype>(points.size()));
    for (const auto& point : points) result.append({width * point.x, height * point.y});

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

struct PathSample {
    QPointF point;
    QPointF tangent = {1.0, 0.0};
};

qreal pathLength(const QVector<QPointF>& points)
{
    qreal result = 0.0;
    for (qsizetype i = 0; i + 1 < points.size(); ++i) result += QLineF(points.at(i), points.at(i + 1)).length();
    return result;
}

PathSample pathSampleAtDistance(const QVector<QPointF>& points, qreal distance)
{
    if (points.isEmpty()) return {};
    if (points.size() == 1) return {points.first(), {1.0, 0.0}};

    qreal remaining = std::clamp(distance, 0.0, pathLength(points));
    for (qsizetype i = 0; i + 1 < points.size(); ++i) {
        const QPointF a = points.at(i);
        const QPointF b = points.at(i + 1);
        const qreal segment = QLineF(a, b).length();
        if (segment <= 0.0) continue;
        const QPointF tangent = (b - a) / segment;
        if (remaining <= segment) return {a + tangent * remaining, tangent};
        remaining -= segment;
    }

    const QPointF a = points.at(points.size() - 2);
    const QPointF b = points.last();
    const qreal segment = std::max<qreal>(1.0, QLineF(a, b).length());
    return {b, (b - a) / segment};
}

bool isNeutralFlatPath(const std::vector<Point>& points)
{
    if (points.size() < 2) return false;
    if (std::abs(points.front().x) > 0.0001 || std::abs(points.back().x - 1.0) > 0.0001) return false;
    return std::ranges::all_of(points, [](const Point& point) { return std::abs(point.y - 0.5) <= 0.0001; });
}

QBrush fillBrushFor(const TextBox& box)
{
    if (!box.effects.gradientEnabled) return QBrush(toQColor(box.style.textColor));
    QLinearGradient gradient(0, 0, box.effects.gradientDirection == 1 ? box.bounds.w : 0, box.effects.gradientDirection == 1 ? 0 : box.bounds.h);
    gradient.setColorAt(0.0, toQColor(box.effects.gradientColorA));
    gradient.setColorAt(1.0, toQColor(box.effects.gradientColorB));
    return QBrush(gradient);
}

void fillShadow(QPainter& painter, const QPainterPath& path, const QColor& color, int blurSize)
{
    const int radius = cappedBlurKernelRadius(blurSize);
    if (radius <= 0) {
        painter.fillPath(path, color);
        return;
    }
    const QRect sourceRect = path.boundingRect().adjusted(-radius, -radius, radius, radius).toAlignedRect();
    if (sourceRect.isEmpty()) return;
    QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    QPainter layerPainter(&layer);
    layerPainter.setRenderHint(QPainter::Antialiasing, true);
    layerPainter.translate(-sourceRect.topLeft());
    layerPainter.fillPath(path, color);
    layerPainter.end();
    painter.drawImage(sourceRect.topLeft(), gaussianBlurred(layer, radius));
}

QPainterPath pathTextFor(const TextBox& box, const QFont& font, double inset)
{
    // Non-neutral paths still approximate shaped text per QChar; neutral flat paths bypass this path.
    QPainterPath path;
    QStringList lines;
    const bool smooth = box.effects.pathMode == 1;
    const QVector<QPointF> guidePoints = layoutPathPoints(box.effects.pathPoints, box.bounds.w, box.bounds.h, smooth);
    const qreal guideLength = pathLength(guidePoints);
    if (guideLength <= 0.0) return textPathFor(box, font, inset);
    TextBox pathLayoutBox = box;
    pathLayoutBox.bounds.w = guideLength;
    textPathFor(pathLayoutBox, font, inset, &lines);
    const QFontMetricsF metrics(font);
    const qreal lineSpacing = box.style.fontSize + box.style.lineSpacing;
    const qreal firstLineOffset = -(lines.size() - 1) * lineSpacing * 0.5;
    for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString& line = lines.at(lineIndex);
        double distance = std::max<qreal>(0.0, (guideLength - metrics.horizontalAdvance(line)) * 0.5);
        const qreal lineOffset = firstLineOffset + lineIndex * lineSpacing;
        for (const QChar ch : line) {
            const qreal advance = metrics.horizontalAdvance(ch);
            const PathSample sample = pathSampleAtDistance(guidePoints, distance + advance * 0.5);
            distance += advance;
            if (ch.isSpace()) continue;
            QPainterPath glyph;
            glyph.addText(-advance / 2.0, 0.0, font, QString(ch));
            const QPointF normal(-sample.tangent.y(), sample.tangent.x());
            QTransform transform;
            transform.translate(sample.point.x() + normal.x() * lineOffset, sample.point.y() + normal.y() * lineOffset);
            transform.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) * 180.0 / 3.14159265358979323846);
            path.addPath(transform.map(glyph));
        }
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

void drawTextBox(QPainter& painter, const TextBox& box)
{
    if (box.text.empty()) return;
    const auto font = qFontFor(box);
    const qreal outline = box.effects.outlineEnabled && box.effects.outlineSize > 0 ? box.effects.outlineSize : 0;
    const qreal inset = outline > 0 ? outline / 2.0 : 0.0;
    const bool usingPathText = box.effects.pathEnabled && box.effects.pathPoints.size() > 1 && !isNeutralFlatPath(box.effects.pathPoints);
    auto path = usingPathText ? pathTextFor(box, font, inset) : textPathFor(box, font, inset);
    QRectF paintedBounds = path.boundingRect();
    if (outline > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(outline);
        stroker.setJoinStyle(Qt::RoundJoin);
        paintedBounds = paintedBounds.united(stroker.createStroke(path).boundingRect());
    }
    qreal dx = 0.0;
    qreal dy = 0.0;
    if (paintedBounds.left() < 0.0) dx = -paintedBounds.left();
    else if (paintedBounds.width() <= box.bounds.w && paintedBounds.right() > box.bounds.w) dx = box.bounds.w - paintedBounds.right();
    if (!usingPathText) {
        if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();
        else if (paintedBounds.height() <= box.bounds.h && paintedBounds.bottom() > box.bounds.h) dy = box.bounds.h - paintedBounds.bottom();
    }
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
        path.translate(dx, dy);
        paintedBounds.translate(dx, dy);
    }
    QRectF effectBounds = paintedBounds;
    const int shadowRadius = box.effects.shadowEnabled ? cappedBlurKernelRadius(box.effects.shadowBlurSize) : 0;
    if (box.effects.shadowEnabled) {
        effectBounds = effectBounds.united(path.boundingRect()
                                               .translated(box.effects.shadowOffsetX, box.effects.shadowOffsetY)
                                               .adjusted(-shadowRadius, -shadowRadius, shadowRadius, shadowRadius));
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(box.bounds.x + box.bounds.w / 2.0, box.bounds.y + box.bounds.h / 2.0);
    painter.rotate(box.rotationDegrees);
    painter.translate(-box.bounds.w / 2.0, -box.bounds.h / 2.0);
    if (box.effects.perspectiveEnabled) {
        QPolygonF source({{0, 0}, {box.bounds.w, 0}, {box.bounds.w, box.bounds.h}, {0, box.bounds.h}});
        QPolygonF target({{box.effects.perspectiveNw.x, box.effects.perspectiveNw.y},
                          {box.bounds.w + box.effects.perspectiveNe.x, box.effects.perspectiveNe.y},
                          {box.bounds.w + box.effects.perspectiveSe.x, box.bounds.h + box.effects.perspectiveSe.y},
                          {box.effects.perspectiveSw.x, box.bounds.h + box.effects.perspectiveSw.y}});
        QTransform transform;
        if (QTransform::quadToQuad(source, target, transform)) painter.setTransform(transform, true);
    }
    auto paintText = [&](QPainter& target, bool clipShadow) {
        if (box.effects.shadowEnabled) {
            if (clipShadow) {
                target.save();
                target.setClipRect(QRectF(0, 0, box.bounds.w, box.bounds.h));
                fillShadow(target, path.translated(box.effects.shadowOffsetX, box.effects.shadowOffsetY), toQColor(box.effects.shadowColor), box.effects.shadowBlurSize);
                target.restore();
            } else {
                fillShadow(target, path.translated(box.effects.shadowOffsetX, box.effects.shadowOffsetY), toQColor(box.effects.shadowColor), box.effects.shadowBlurSize);
            }
        }
        if (outline <= 0) return;
        QPainterPathStroker stroker;
        stroker.setWidth(outline);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath stroke = stroker.createStroke(path);
        stroke.setFillRule(Qt::WindingFill);
        target.fillPath(stroke, toQColor(box.effects.outlineColor));
    };
    auto paintFill = [&](QPainter& target) {
        target.fillPath(path, fillBrushFor(box));
    };
    if (box.effects.blurEnabled && box.effects.blurSize > 0) {
        const int pad = std::max(1, cappedBlurKernelRadius(box.effects.blurSize));
        const QRect sourceRect = effectBounds.adjusted(-pad, -pad, pad, pad)
                                     .toAlignedRect()
                                     .intersected(QRectF(0, 0, box.bounds.w, box.bounds.h).adjusted(-pad, -pad, pad, pad).toAlignedRect());
        if (sourceRect.isEmpty()) {
            painter.restore();
            return;
        }
        QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter layerPainter(&layer);
        layerPainter.setRenderHint(QPainter::Antialiasing, true);
        layerPainter.translate(-sourceRect.topLeft());
        paintText(layerPainter, false);
        paintFill(layerPainter);
        layerPainter.end();
        painter.save();
        painter.setClipRect(QRectF(0, 0, box.bounds.w, box.bounds.h));
        painter.drawImage(sourceRect.topLeft(), gaussianBlurred(layer, pad));
        painter.restore();
    } else {
        paintText(painter, true);
        paintFill(painter);
    }
    painter.restore();
}

} // namespace

bool RenderGraph::exportPagePng(const DocumentModel& document, const std::filesystem::path& pageImagePath, const std::filesystem::path& exportPath, std::string* error) const
{
    QImage source(QString::fromStdString(pageImagePath.string()));
    if (source.isNull()) {
        if (error) *error = "Could not load page image: " + pageImagePath.string();
        return false;
    }
    source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&source);
    for (const auto& box : document.textBoxes()) drawTextBox(painter, box);
    painter.end();

    std::filesystem::create_directories(exportPath.parent_path());
    const auto tempPath = exportPath.string() + ".tmp";
    if (!source.save(QString::fromStdString(tempPath), "PNG")) {
        if (error) *error = "Could not write PNG output: " + exportPath.string();
        return false;
    }
    std::filesystem::remove(exportPath);
    std::filesystem::rename(tempPath, exportPath);
    return true;
}

std::vector<std::string> RenderGraph::warnings(const DocumentModel& document) const
{
    std::vector<std::string> result;
    for (const auto& box : document.textBoxes()) {
        if (box.effects.perspectiveEnabled) result.push_back("Perspective is approximated with an affine transform in MVP export/preview");
        if (box.effects.pathEnabled) result.push_back(box.effects.pathMode == 1 ? "Smooth path text is approximated with TypeX-style smoothed segments" : "Path text is approximated on straight segments between path handles");
    }
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace textfx
