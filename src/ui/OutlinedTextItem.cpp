#include "ui/OutlinedTextItem.h"

#include "core/EffectLimits.h"
#include "core/GaussianBlur.h"
#include "fonts/FontResolver.h"

#include <QFont>
#include <QFontMetricsF>
#include <QJsonDocument>
#include <QLinearGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTextLayout>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace textfx {

namespace {
QVector<QPointF> layoutPathPoints(const QVariantList& points, qreal layoutWidth, qreal layoutHeight, bool smooth)
{
    QVector<QPointF> result;
    result.reserve(points.size());
    for (const QVariant& value : points) {
        const auto point = value.toList();
        result.append({layoutWidth * point.value(0).toDouble(), layoutHeight * point.value(1).toDouble()});
    }

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

bool isNeutralFlatPath(const QVariantList& points)
{
    if (points.size() < 2) return false;
    const auto first = points.first().toList();
    const auto last = points.last().toList();
    if (first.size() < 2 || last.size() < 2 || std::abs(first.at(0).toDouble()) > 0.0001 || std::abs(last.at(0).toDouble() - 1.0) > 0.0001) return false;
    for (const QVariant& value : points) {
        const auto point = value.toList();
        if (point.size() < 2 || std::abs(point.at(1).toDouble() - 0.5) > 0.0001) return false;
    }
    return true;
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

    const bool usingPathText = pathEnabled_ && pathPoints_.size() > 1 && !isNeutralFlatPath(pathPoints_);
    QPainterPath path = usingPathText ? pathText(font, layoutWidth, layoutHeight) : textPath(font, layoutWidth, inset);
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
    if (!usingPathText) {
        if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();
        else if (paintedBounds.height() <= layoutHeight && paintedBounds.bottom() > layoutHeight) dy = layoutHeight - paintedBounds.bottom();
    }
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

QPainterPath OutlinedTextItem::textPath(const QFont& font, qreal layoutWidth, qreal inset, QStringList* lineTexts, QVector<qreal>* lineXs, QVector<qreal>* lineBaselines) const
{
    QPainterPath path;
    QString text = text_;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');
    const QStringList paragraphs = text.split(u'\n');
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
            const qreal baseline = line.position().y() + line.ascent();
            if (lineTexts) lineTexts->append(run);
            if (lineXs) lineXs->append(x);
            if (lineBaselines) lineBaselines->append(baseline);
            path.addText(line.position().x(), baseline, font, run);
            y += line.height() + lineSpacing_;
        }
        layout.endLayout();
    }
    path.setFillRule(Qt::WindingFill);
    return path;
}

QPainterPath OutlinedTextItem::pathText(const QFont& font, qreal layoutWidth, qreal layoutHeight) const
{
    // Non-neutral paths still approximate shaped text per QChar; neutral flat paths bypass this path.
    QPainterPath path;
    QStringList lines;
    const qreal inset = outlineSize_ > 0 ? outlineSize_ / 2.0 : 0.0;
    const bool smooth = pathMode_ == 1;
    const QVector<QPointF> guidePoints = layoutPathPoints(pathPoints_, layoutWidth, layoutHeight, smooth);
    const qreal guideLength = pathLength(guidePoints);
    if (guideLength <= 0.0) return textPath(font, layoutWidth, inset);
    textPath(font, guideLength, inset, &lines);
    const QFontMetricsF metrics(font);
    const qreal lineSpacing = pixelSize_ + lineSpacing_;
    const qreal firstLineOffset = -(lines.size() - 1) * lineSpacing * 0.5;
    for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString& line = lines.at(lineIndex);
        qreal distance = std::max<qreal>(0.0, (guideLength - metrics.horizontalAdvance(line)) * 0.5);
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

#ifdef TEXTFX_TESTING
QStringList OutlinedTextItem::wrappedLinesForTesting() const
{
    QStringList lines;
    const qreal scale = std::max<qreal>(0.0001, renderScale_);
    const qreal inset = outlineSize_ > 0 ? outlineSize_ / 2.0 : 0.0;
    textPath(layoutFont(), std::max<qreal>(1.0, width() / scale), inset, &lines);
    return lines;
}

QPointF OutlinedTextItem::pathBaselinePointForTesting(qreal distance, qreal layoutWidth, qreal layoutHeight) const
{
    return pathSampleAtDistance(layoutPathPoints(pathPoints_, layoutWidth, layoutHeight, pathMode_ == 1), distance).point;
}
#endif

} // namespace textfx
