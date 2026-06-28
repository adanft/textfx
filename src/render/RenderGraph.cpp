#include "render/RenderGraph.h"

#include "fonts/FontResolver.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QString>
#include <QStringList>
#include <QTextLayout>

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

QPainterPath textPathFor(const TextBox& box, const QFont& font, double inset)
{
    QPainterPath path;
    const auto text = box.style.uppercase ? QString::fromStdString(box.text).toUpper() : QString::fromStdString(box.text);
    const QStringList paragraphs = text.split(u'\n');
    qreal y = inset;
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
            line.setPosition({x, y});
            path.addText(line.position().x(), line.position().y() + line.ascent(), font, paragraph.mid(line.textStart(), line.textLength()));
            y += line.height() + box.style.lineSpacing;
        }
        layout.endLayout();
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

Point pathPointAt(const std::vector<Point>& points, double t, bool smooth);
double pathAngleAt(const std::vector<Point>& points, double t, bool smooth);

QBrush fillBrushFor(const TextBox& box)
{
    if (!box.effects.gradientEnabled) return QBrush(toQColor(box.style.textColor));
    QLinearGradient gradient(0, 0, box.effects.gradientDirection == 1 ? box.bounds.w : 0, box.effects.gradientDirection == 1 ? 0 : box.bounds.h);
    gradient.setColorAt(0.0, toQColor(box.effects.gradientColorA));
    gradient.setColorAt(1.0, toQColor(box.effects.gradientColorB));
    return QBrush(gradient);
}

void fillPath(QPainter& painter, const QPainterPath& path, const QBrush& brush, int blurSize)
{
    const int radius = std::max(0, blurSize);
    if (radius > 0) {
        painter.save();
        painter.setOpacity(0.18);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius) painter.fillPath(path.translated(x, y), brush);
            }
        }
        painter.restore();
    }
    painter.fillPath(path, brush);
}

QPainterPath pathTextFor(const TextBox& box, const QFont& font)
{
    QPainterPath path;
    const auto text = box.style.uppercase ? QString::fromStdString(box.text).toUpper() : QString::fromStdString(box.text);
    const auto count = std::max<qsizetype>(1, text.size());
    for (int i = 0; i < text.size(); ++i) {
        const auto t = count == 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(count - 1);
        const auto point = pathPointAt(box.effects.pathPoints, t, box.effects.pathMode == 1);
        QPainterPath glyph;
        glyph.addText(0, 0, font, text.mid(i, 1));
        QTransform transform;
        transform.translate(box.bounds.w * point.x, box.bounds.h * point.y);
        transform.rotate(pathAngleAt(box.effects.pathPoints, t, box.effects.pathMode == 1));
        path.addPath(transform.map(glyph));
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
    auto path = box.effects.pathEnabled && box.effects.pathPoints.size() > 1 ? pathTextFor(box, font) : textPathFor(box, font, inset);
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
    if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();
    else if (paintedBounds.height() <= box.bounds.h && paintedBounds.bottom() > box.bounds.h) dy = box.bounds.h - paintedBounds.bottom();
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) path.translate(dx, dy);

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
    if (box.effects.shadowEnabled) fillPath(painter, path.translated(box.effects.shadowOffsetX, box.effects.shadowOffsetY), QBrush(toQColor(box.effects.shadowColor)), box.effects.shadowBlurSize);
    if (outline > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(outline);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath stroke = stroker.createStroke(path);
        stroke.setFillRule(Qt::WindingFill);
        painter.fillPath(stroke, toQColor(box.effects.outlineColor));
    }
    fillPath(painter, path, fillBrushFor(box), box.effects.blurEnabled ? box.effects.blurSize : 0);
    painter.restore();
}

Point pathPointAt(const std::vector<Point>& points, double t, bool smooth)
{
    if (points.empty()) return {};
    if (points.size() == 1) return points.front();

    const auto segmentCount = static_cast<int>(points.size()) - 1;
    const auto scaled = std::min(t * segmentCount, static_cast<double>(segmentCount) - 0.0001);
    const auto segment = std::max(0, static_cast<int>(scaled));
    const auto localT = scaled - segment;
    const auto& a = points[static_cast<std::size_t>(segment)];
    const auto& b = points[static_cast<std::size_t>(segment + 1)];
    if (!smooth) return {a.x + (b.x - a.x) * localT, a.y + (b.y - a.y) * localT};

    const auto& p0 = points[static_cast<std::size_t>(std::max(0, segment - 1))];
    const auto& p3 = points[static_cast<std::size_t>(std::min(segmentCount, segment + 2))];
    const auto tt = localT * localT;
    const auto ttt = tt * localT;
    const auto curve = [&](double p0v, double p1v, double p2v, double p3v) {
        return 0.5 * ((2.0 * p1v) + (-p0v + p2v) * localT + (2.0 * p0v - 5.0 * p1v + 4.0 * p2v - p3v) * tt + (-p0v + 3.0 * p1v - 3.0 * p2v + p3v) * ttt);
    };
    return {curve(p0.x, a.x, b.x, p3.x), curve(p0.y, a.y, b.y, p3.y)};
}

double pathAngleAt(const std::vector<Point>& points, double t, bool smooth)
{
    const auto before = pathPointAt(points, std::max(0.0, t - 0.01), smooth);
    const auto after = pathPointAt(points, std::min(1.0, t + 0.01), smooth);
    return std::atan2(after.y - before.y, after.x - before.x) * 180.0 / 3.14159265358979323846;
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
        if (box.effects.pathEnabled) result.push_back(box.effects.pathMode == 1 ? "Smooth path text is approximated with a Catmull-Rom curve through path handles" : "Path text is approximated on straight segments between path handles");
    }
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace textfx
