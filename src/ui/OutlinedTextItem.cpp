#include "ui/OutlinedTextItem.h"

#include "core/EffectLimits.h"
#include "core/GaussianBlur.h"
#include "fonts/FontResolver.h"

#include <QFont>
#include <QJsonDocument>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTextLayout>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace textfx {

namespace {
QPointF pathPointAt(const QVariantList& points, double t, bool smooth)
{
    auto pointAt = [&](int index) {
        const auto point = points.at(index).toList();
        return QPointF(point.value(0).toDouble(), point.value(1).toDouble());
    };
    if (points.isEmpty()) return {};
    if (points.size() == 1) return pointAt(0);

    const int segmentCount = points.size() - 1;
    const double scaled = std::min(t * segmentCount, static_cast<double>(segmentCount) - 0.0001);
    const int segment = std::max(0, static_cast<int>(scaled));
    const double localT = scaled - segment;
    const QPointF a = pointAt(segment);
    const QPointF b = pointAt(segment + 1);
    if (!smooth) return {a.x() + (b.x() - a.x()) * localT, a.y() + (b.y() - a.y()) * localT};

    const QPointF p0 = pointAt(std::max(0, segment - 1));
    const QPointF p3 = pointAt(std::min(segmentCount, segment + 2));
    const double tt = localT * localT;
    const double ttt = tt * localT;
    auto curve = [&](double p0v, double p1v, double p2v, double p3v) {
        return 0.5 * ((2.0 * p1v) + (-p0v + p2v) * localT + (2.0 * p0v - 5.0 * p1v + 4.0 * p2v - p3v) * tt + (-p0v + 3.0 * p1v - 3.0 * p2v + p3v) * ttt);
    };
    return {curve(p0.x(), a.x(), b.x(), p3.x()), curve(p0.y(), a.y(), b.y(), p3.y())};
}

double pathAngleAt(const QVariantList& points, double t, bool smooth)
{
    const QPointF before = pathPointAt(points, std::max(0.0, t - 0.01), smooth);
    const QPointF after = pathPointAt(points, std::min(1.0, t + 0.01), smooth);
    return std::atan2(after.y() - before.y(), after.x() - before.x()) * 180.0 / 3.14159265358979323846;
}
} // namespace

OutlinedTextItem::OutlinedTextItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void OutlinedTextItem::setText(const QString& value)
{
    if (text_ == value) return;
    text_ = value;
    update();
    emit textChanged();
}

void OutlinedTextItem::setFontFamily(const QString& value)
{
    if (fontFamily_ == value) return;
    fontFamily_ = value;
    update();
    emit fontFamilyChanged();
}

void OutlinedTextItem::setPixelSize(qreal value)
{
    value = std::max<qreal>(1.0, value);
    if (qFuzzyCompare(pixelSize_, value)) return;
    pixelSize_ = value;
    update();
    emit pixelSizeChanged();
}

void OutlinedTextItem::setBold(bool value)
{
    if (bold_ == value) return;
    bold_ = value;
    update();
    emit boldChanged();
}

void OutlinedTextItem::setItalic(bool value)
{
    if (italic_ == value) return;
    italic_ = value;
    update();
    emit italicChanged();
}

void OutlinedTextItem::setLetterSpacing(qreal value)
{
    if (qFuzzyCompare(letterSpacing_, value)) return;
    letterSpacing_ = value;
    update();
    emit letterSpacingChanged();
}

void OutlinedTextItem::setLineSpacing(qreal value)
{
    if (qFuzzyCompare(lineSpacing_, value)) return;
    lineSpacing_ = value;
    update();
    emit lineSpacingChanged();
}

void OutlinedTextItem::setColor(const QColor& value)
{
    if (color_ == value) return;
    color_ = value;
    update();
    emit colorChanged();
}

void OutlinedTextItem::setOutlineColor(const QColor& value)
{
    if (outlineColor_ == value) return;
    outlineColor_ = value;
    update();
    emit outlineColorChanged();
}

void OutlinedTextItem::setOutlineSize(qreal value)
{
    value = std::max<qreal>(0.0, value);
    if (qFuzzyCompare(outlineSize_, value)) return;
    outlineSize_ = value;
    update();
    emit outlineSizeChanged();
}

void OutlinedTextItem::setBlurSize(int value)
{
    value = std::max(0, value);
    if (blurSize_ == value) return;
    blurSize_ = value;
    update();
    emit blurSizeChanged();
}

void OutlinedTextItem::setShadowEnabled(bool value)
{
    if (shadowEnabled_ == value) return;
    shadowEnabled_ = value;
    update();
    emit shadowEnabledChanged();
}

void OutlinedTextItem::setShadowColor(const QColor& value)
{
    if (shadowColor_ == value) return;
    shadowColor_ = value;
    update();
    emit shadowColorChanged();
}

void OutlinedTextItem::setShadowOffsetX(qreal value)
{
    if (qFuzzyCompare(shadowOffsetX_, value)) return;
    shadowOffsetX_ = value;
    update();
    emit shadowOffsetXChanged();
}

void OutlinedTextItem::setShadowOffsetY(qreal value)
{
    if (qFuzzyCompare(shadowOffsetY_, value)) return;
    shadowOffsetY_ = value;
    update();
    emit shadowOffsetYChanged();
}

void OutlinedTextItem::setShadowBlurSize(int value)
{
    value = std::max(0, value);
    if (shadowBlurSize_ == value) return;
    shadowBlurSize_ = value;
    update();
    emit shadowBlurSizeChanged();
}

void OutlinedTextItem::setGradientEnabled(bool value)
{
    if (gradientEnabled_ == value) return;
    gradientEnabled_ = value;
    update();
    emit gradientEnabledChanged();
}

void OutlinedTextItem::setGradientDirection(int value)
{
    value = std::clamp(value, 0, 1);
    if (gradientDirection_ == value) return;
    gradientDirection_ = value;
    update();
    emit gradientDirectionChanged();
}

void OutlinedTextItem::setGradientColorA(const QColor& value)
{
    if (gradientColorA_ == value) return;
    gradientColorA_ = value;
    update();
    emit gradientColorAChanged();
}

void OutlinedTextItem::setGradientColorB(const QColor& value)
{
    if (gradientColorB_ == value) return;
    gradientColorB_ = value;
    update();
    emit gradientColorBChanged();
}

void OutlinedTextItem::setPathEnabled(bool value)
{
    if (pathEnabled_ == value) return;
    pathEnabled_ = value;
    update();
    emit pathEnabledChanged();
}

void OutlinedTextItem::setPathMode(int value)
{
    value = std::clamp(value, 0, 1);
    if (pathMode_ == value) return;
    pathMode_ = value;
    update();
    emit pathModeChanged();
}

void OutlinedTextItem::setPathPoints(const QVariantList& value)
{
    if (pathPoints_ == value) return;
    pathPoints_ = value;
    update();
    emit pathPointsChanged();
}

void OutlinedTextItem::setRenderScale(qreal value)
{
    value = std::max<qreal>(0.0001, value);
    if (qFuzzyCompare(renderScale_, value)) return;
    renderScale_ = value;
    update();
    emit renderScaleChanged();
}

void OutlinedTextItem::setHorizontalAlignment(int value)
{
    if (horizontalAlignment_ == value) return;
    horizontalAlignment_ = value;
    update();
    emit horizontalAlignmentChanged();
}

void OutlinedTextItem::paint(QPainter* painter)
{
    if (!painter || text_.isEmpty()) return;

    const qreal scale = std::max<qreal>(0.0001, renderScale_);
    const qreal layoutWidth = std::max<qreal>(1.0, width() / scale);
    const qreal layoutHeight = std::max<qreal>(1.0, height() / scale);
    const QFont font = layoutFont();
    const qreal inset = outlineSize_ > 0 ? outlineSize_ / 2.0 : 0.0;
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path = pathEnabled_ && pathPoints_.size() > 1 ? pathText(font, layoutWidth, layoutHeight) : textPath(font, layoutWidth, inset);
    QRectF paintedBounds = path.boundingRect();
    if (outlineSize_ > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(outlineSize_);
        stroker.setJoinStyle(Qt::RoundJoin);
        paintedBounds = paintedBounds.united(stroker.createStroke(path).boundingRect());
    }

    qreal dx = 0.0;
    qreal dy = 0.0;
    if (paintedBounds.left() < 0.0) dx = -paintedBounds.left();
    else if (paintedBounds.width() <= layoutWidth && paintedBounds.right() > layoutWidth) dx = layoutWidth - paintedBounds.right();
    if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();
    else if (paintedBounds.height() <= layoutHeight && paintedBounds.bottom() > layoutHeight) dy = layoutHeight - paintedBounds.bottom();
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
        path.translate(dx, dy);
        paintedBounds.translate(dx, dy);
    }

    const int shadowRadius = shadowEnabled_ ? cappedBlurKernelRadius(shadowBlurSize_ * scale) : 0;
    QRectF effectBounds = paintedBounds;
    const QRectF shadowBounds = path.boundingRect().translated(shadowOffsetX_, shadowOffsetY_);
    if (shadowEnabled_) effectBounds = effectBounds.united(shadowBounds.adjusted(-shadowRadius / scale, -shadowRadius / scale, shadowRadius / scale, shadowRadius / scale));

    auto paintShadow = [&](QPainter& target) {
        if (!shadowEnabled_) return;
        const QPainterPath shadowPath = path.translated(shadowOffsetX_, shadowOffsetY_);
        if (shadowRadius <= 0) {
            target.save();
            target.scale(scale, scale);
            target.fillPath(shadowPath, shadowColor_);
            target.restore();
            return;
        }
        const QRect sourceRect = QRectF(shadowBounds.left() * scale - shadowRadius,
                                        shadowBounds.top() * scale - shadowRadius,
                                        shadowBounds.width() * scale + shadowRadius * 2,
                                        shadowBounds.height() * scale + shadowRadius * 2)
                                     .toAlignedRect();
        if (sourceRect.isEmpty()) return;
        QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        QPainter layerPainter(&layer);
        layerPainter.setRenderHint(QPainter::Antialiasing, true);
        layerPainter.translate(-sourceRect.topLeft());
        layerPainter.scale(scale, scale);
        layerPainter.fillPath(shadowPath, shadowColor_);
        layerPainter.end();
        target.drawImage(sourceRect.topLeft(), gaussianBlurred(layer, shadowRadius));
    };
    auto paintText = [&](QPainter& target) {
        paintShadow(target);
        target.scale(scale, scale);
        if (outlineSize_ > 0) {
            QPainterPathStroker stroker;
            stroker.setWidth(outlineSize_);
            stroker.setJoinStyle(Qt::RoundJoin);
            QPainterPath stroke = stroker.createStroke(path);
            stroke.setFillRule(Qt::WindingFill);
            target.fillPath(stroke, outlineColor_);
        }
        if (gradientEnabled_) {
            QLinearGradient gradient(0, 0, gradientDirection_ == 1 ? layoutWidth : 0, gradientDirection_ == 1 ? 0 : layoutHeight);
            gradient.setColorAt(0.0, gradientColorA_);
            gradient.setColorAt(1.0, gradientColorB_);
            target.fillPath(path, gradient);
        } else {
            target.fillPath(path, color_);
        }
    };
    painter->save();
    const int radius = cappedBlurKernelRadius(blurSize_ * scale);
    if (radius > 0) {
        const QRect itemRect(0, 0, static_cast<int>(std::ceil(width())), static_cast<int>(std::ceil(height())));
        const QRect sourceRect = QRectF(effectBounds.left() * scale - radius,
                                        effectBounds.top() * scale - radius,
                                        effectBounds.width() * scale + radius * 2,
                                        effectBounds.height() * scale + radius * 2)
                                     .toAlignedRect()
                                     .intersected(itemRect.adjusted(-radius, -radius, radius, radius));
        if (sourceRect.isEmpty()) {
            painter->restore();
            return;
        }
        const QString key = blurCacheKey(radius, sourceRect);
        if (blurCacheKey_ != key || blurCacheImage_.isNull()) {
            QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
            layer.fill(Qt::transparent);
            QPainter layerPainter(&layer);
            layerPainter.setRenderHint(QPainter::Antialiasing, true);
            layerPainter.translate(-sourceRect.topLeft());
            paintText(layerPainter);
            layerPainter.end();
            blurCacheImage_ = gaussianBlurred(layer, radius);
            blurCacheKey_ = key;
        }
        painter->setClipRect(QRectF(0, 0, width(), height()));
        painter->drawImage(sourceRect.topLeft(), blurCacheImage_);
    } else {
        paintText(*painter);
    }
    painter->restore();
}

QString OutlinedTextItem::blurCacheKey(int radius, const QRect& sourceRect) const
{
    return QStringList{
               text_, fontFamily_, QString::number(pixelSize_, 'g', 17), bold_ ? QStringLiteral("1") : QStringLiteral("0"), italic_ ? QStringLiteral("1") : QStringLiteral("0"),
                QString::number(letterSpacing_, 'g', 17), QString::number(lineSpacing_, 'g', 17), QString::number(color_.rgba()), QString::number(outlineColor_.rgba()),
                QString::number(outlineSize_, 'g', 17), QString::number(renderScale_, 'g', 17), QString::number(width(), 'g', 17), QString::number(height(), 'g', 17),
                 QString::number(horizontalAlignment_), shadowEnabled_ ? QStringLiteral("1") : QStringLiteral("0"), QString::number(shadowColor_.rgba()),
                QString::number(shadowOffsetX_, 'g', 17), QString::number(shadowOffsetY_, 'g', 17), QString::number(shadowBlurSize_), gradientEnabled_ ? QStringLiteral("1") : QStringLiteral("0"), QString::number(gradientDirection_),
                 QString::number(gradientColorA_.rgba()), QString::number(gradientColorB_.rgba()), pathEnabled_ ? QStringLiteral("1") : QStringLiteral("0"), QString::number(pathMode_), QString::fromUtf8(QJsonDocument::fromVariant(pathPoints_).toJson(QJsonDocument::Compact)), QString::number(radius), QString::number(sourceRect.x()), QString::number(sourceRect.y()), QString::number(sourceRect.width()),
                  QString::number(sourceRect.height())}
        .join(u'\n');
}

QFont OutlinedTextItem::layoutFont() const
{
    QFont font(fontFamily_);
    font.setPixelSize(static_cast<int>(std::round(pixelSize_)));
    font.setBold(bold_);
    font.setItalic(italic_);
    font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing_);
    return resolveFont(font).font;
}

QPainterPath OutlinedTextItem::textPath(const QFont& font, qreal layoutWidth, qreal inset, QStringList* lineTexts) const
{
    QPainterPath path;
    const QStringList paragraphs = text_.split(u'\n');
    qreal y = inset;
    const qreal paintWidth = std::max<qreal>(1.0, layoutWidth - inset * 2.0);
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
            if (horizontalAlignment_ == Qt::AlignHCenter) x = inset + (paintWidth - line.naturalTextWidth()) / 2.0;
            if (horizontalAlignment_ == Qt::AlignRight) x = inset + paintWidth - line.naturalTextWidth();
            line.setPosition({x, y});
            const QString run = paragraph.mid(line.textStart(), line.textLength());
            if (lineTexts) lineTexts->append(run);
            path.addText(line.position().x(), line.position().y() + line.ascent(), font, run);
            y += line.height() + lineSpacing_;
        }
        layout.endLayout();
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

QPainterPath OutlinedTextItem::pathText(const QFont& font, qreal layoutWidth, qreal layoutHeight) const
{
    QPainterPath path;
    const qsizetype count = std::max<qsizetype>(1, text_.size());
    for (qsizetype i = 0; i < text_.size(); ++i) {
        const double t = count == 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(count - 1);
        const QPointF point = pathPointAt(pathPoints_, t, pathMode_ == 1);
        QPainterPath glyph;
        glyph.addText(0, 0, font, text_.mid(i, 1));
        QTransform transform;
        transform.translate(layoutWidth * point.x(), layoutHeight * point.y());
        transform.rotate(pathAngleAt(pathPoints_, t, pathMode_ == 1));
        path.addPath(transform.map(glyph));
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

#ifdef TEXTFX_TESTING
QStringList OutlinedTextItem::wrappedLinesForTesting() const
{
    QStringList lines;
    const qreal scale = std::max<qreal>(0.0001, renderScale_);
    const qreal inset = outlineSize_ > 0 ? outlineSize_ / 2.0 : 0.0;
    textPath(layoutFont(), std::max<qreal>(1.0, width() / scale), inset, &lines);
    return lines;
}
#endif

} // namespace textfx
