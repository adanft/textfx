#include "ui/OutlinedTextItem.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStringList>
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
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) path.translate(dx, dy);

    painter->save();
    painter->scale(scale, scale);
    if (outlineSize_ > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(outlineSize_);
        stroker.setJoinStyle(Qt::RoundJoin);
        painter->fillPath(stroker.createStroke(path), outlineColor_);
    }
    painter->fillPath(path, color_);
    painter->restore();
}

QFont OutlinedTextItem::layoutFont() const
{
    QFont font(fontFamily_);
    font.setPixelSize(static_cast<int>(std::round(pixelSize_)));
    font.setBold(bold_);
    font.setItalic(italic_);
    font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing_);
    return font;
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
