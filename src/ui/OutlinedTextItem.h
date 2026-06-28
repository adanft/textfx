#pragma once

#include <QColor>
#include <QFont>
#include <QQuickPaintedItem>
#include <QStringList>

namespace textfx {

class OutlinedTextItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(qreal pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
    Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY italicChanged)
    Q_PROPERTY(qreal letterSpacing READ letterSpacing WRITE setLetterSpacing NOTIFY letterSpacingChanged)
    Q_PROPERTY(qreal lineSpacing READ lineSpacing WRITE setLineSpacing NOTIFY lineSpacingChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)
    Q_PROPERTY(qreal outlineSize READ outlineSize WRITE setOutlineSize NOTIFY outlineSizeChanged)
    Q_PROPERTY(qreal renderScale READ renderScale WRITE setRenderScale NOTIFY renderScaleChanged)
    Q_PROPERTY(int horizontalAlignment READ horizontalAlignment WRITE setHorizontalAlignment NOTIFY horizontalAlignmentChanged)

public:
    explicit OutlinedTextItem(QQuickItem* parent = nullptr);

    QString text() const { return text_; }
    void setText(const QString& value);
    QString fontFamily() const { return fontFamily_; }
    void setFontFamily(const QString& value);
    qreal pixelSize() const { return pixelSize_; }
    void setPixelSize(qreal value);
    bool bold() const { return bold_; }
    void setBold(bool value);
    bool italic() const { return italic_; }
    void setItalic(bool value);
    qreal letterSpacing() const { return letterSpacing_; }
    void setLetterSpacing(qreal value);
    qreal lineSpacing() const { return lineSpacing_; }
    void setLineSpacing(qreal value);
    QColor color() const { return color_; }
    void setColor(const QColor& value);
    QColor outlineColor() const { return outlineColor_; }
    void setOutlineColor(const QColor& value);
    qreal outlineSize() const { return outlineSize_; }
    void setOutlineSize(qreal value);
    qreal renderScale() const { return renderScale_; }
    void setRenderScale(qreal value);
    int horizontalAlignment() const { return horizontalAlignment_; }
    void setHorizontalAlignment(int value);

#ifdef TEXTFX_TESTING
    QStringList wrappedLinesForTesting() const;
#endif

    void paint(QPainter* painter) override;

signals:
    void textChanged();
    void fontFamilyChanged();
    void pixelSizeChanged();
    void boldChanged();
    void italicChanged();
    void letterSpacingChanged();
    void lineSpacingChanged();
    void colorChanged();
    void outlineColorChanged();
    void outlineSizeChanged();
    void renderScaleChanged();
    void horizontalAlignmentChanged();

private:
    QFont layoutFont() const;
    QPainterPath textPath(const QFont& font, qreal layoutWidth, qreal inset, QStringList* lineTexts = nullptr) const;
    QString text_;
    QString fontFamily_;
    qreal pixelSize_ = 12.0;
    bool bold_ = false;
    bool italic_ = false;
    qreal letterSpacing_ = 0.0;
    qreal lineSpacing_ = 0.0;
    QColor color_ = Qt::black;
    QColor outlineColor_ = Qt::white;
    qreal outlineSize_ = 0.0;
    qreal renderScale_ = 1.0;
    int horizontalAlignment_ = Qt::AlignLeft;
};

} // namespace textfx
