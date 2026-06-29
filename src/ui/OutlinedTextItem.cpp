#include "ui/OutlinedTextItem.h"

#include "core/EffectLimits.h"
#include "core/GaussianBlur.h"
#include "fonts/FontResolver.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace textfx {

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

    QPainterPath path = textPath(font, layoutWidth, inset);
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
        target.fillPath(path, color_);
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
                QString::number(shadowOffsetX_, 'g', 17), QString::number(shadowOffsetY_, 'g', 17), QString::number(shadowBlurSize_), QString::number(radius), QString::number(sourceRect.x()), QString::number(sourceRect.y()), QString::number(sourceRect.width()),
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
