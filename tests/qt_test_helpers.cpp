#include "qt_test_helpers.h"

#include "ui/OutlinedTextItem.h"

#include <QFile>
#include <QJsonDocument>
#include <QQuickItem>
#include <QTest>
#include <QWindow>
#include <qqml.h>

#include <cstring>
#include <iterator>

namespace textfx::test {

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<textfx::OutlinedTextItem>("TextFX.Ui", 1, 0, "OutlinedTextItem");
    Q_UNUSED(registered);
}

void typeText(QWindow* window, QStringView text)
{
    for (const QChar ch : text) QTest::keyClick(window, ch.toLatin1());
}

QObject* findVisualChildByName(QQuickItem* item, const QString& objectName)
{
    if (!item) return nullptr;
    if (item->objectName() == objectName) return item;
    for (QQuickItem* child : item->childItems()) {
        if (QObject* found = findVisualChildByName(child, objectName)) return found;
    }
    return nullptr;
}

void touch(const QString& path, QSize size)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(QColor(240, 240, 240, 255));
    QVERIFY(image.save(path, "PNG"));
}

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool hasPngMagic(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.read(8) == QByteArray::fromHex("89504e470d0a1a0a");
}

QString readQmlFile(const QString& name)
{
    QFile file(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/") + name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString qmlSource()
{
    return readQmlFile(QStringLiteral("Main.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("ColorButton.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("CentralCanvasShell.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("EditorChrome.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("LeftInspectorPanel.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("RightInspectorPanel.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("ShortcutMenuItem.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("TextBoxDelegate.qml")) + QLatin1Char('\n')
        + readQmlFile(QStringLiteral("TextStyleButton.qml"));
}

bool imageDiffers(const QString& a, const QString& b)
{
    const QImage first = QImage(a).convertToFormat(QImage::Format_RGBA8888);
    const QImage second = QImage(b).convertToFormat(QImage::Format_RGBA8888);
    if (first.isNull() || second.isNull()) return false;
    return imagesDiffer(first, second);
}

bool imagesDiffer(const QImage& first, const QImage& second)
{
    if (first.size() != second.size()) return true;
    for (int y = 0; y < first.height(); ++y) {
        if (std::memcmp(first.constScanLine(y), second.constScanLine(y), static_cast<std::size_t>(first.bytesPerLine())) != 0) return true;
    }
    return false;
}

QRect visibleBounds(const QImage& image, const QColor& background)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 0 && color != background) bounds = bounds.isNull() ? QRect(x, y, 1, 1) : bounds.united(QRect(x, y, 1, 1));
        }
    }
    return bounds;
}

QByteArray utf16be(const QString& text)
{
    QByteArray bytes;
    bytes.reserve(text.size() * 2);
    for (const QChar ch : text) {
        const ushort value = ch.unicode();
        bytes.append(static_cast<char>((value >> 8) & 0xff));
        bytes.append(static_cast<char>(value & 0xff));
    }
    return bytes;
}

QByteArray syntheticSfntNameTable()
{
    struct NameRecord {
        quint16 platform;
        quint16 encoding;
        quint16 language;
        quint16 nameId;
        QByteArray value;
    };

    const NameRecord records[] = {
        {3, 1, 0x0409, 1, utf16be(QStringLiteral("TextFX Alias Test"))},
        {3, 1, 0x0409, 4, utf16be(QStringLiteral("000 TextFX Alias Test [Display]"))},
        {3, 1, 0x0409, 16, utf16be(QStringLiteral("TextFX Usable Family"))},
        {3, 1, 0x0409, 2, utf16be(QStringLiteral("Ignored Style Name"))},
    };

    auto appendU16 = [](QByteArray& data, quint16 value) {
        data.append(static_cast<char>((value >> 8) & 0xff));
        data.append(static_cast<char>(value & 0xff));
    };
    auto appendU32 = [&appendU16](QByteArray& data, quint32 value) {
        appendU16(data, static_cast<quint16>((value >> 16) & 0xffff));
        appendU16(data, static_cast<quint16>(value & 0xffff));
    };

    QByteArray strings;
    QByteArray nameTable;
    appendU16(nameTable, 0);
    appendU16(nameTable, std::size(records));
    appendU16(nameTable, 6 + std::size(records) * 12);

    for (const NameRecord& record : records) {
        appendU16(nameTable, record.platform);
        appendU16(nameTable, record.encoding);
        appendU16(nameTable, record.language);
        appendU16(nameTable, record.nameId);
        appendU16(nameTable, record.value.size());
        appendU16(nameTable, strings.size());
        strings.append(record.value);
    }
    nameTable.append(strings);

    QByteArray sfnt;
    appendU32(sfnt, 0x00010000);
    appendU16(sfnt, 1);
    appendU16(sfnt, 0);
    appendU16(sfnt, 0);
    appendU16(sfnt, 0);
    sfnt.append("name", 4);
    appendU32(sfnt, 0);
    appendU32(sfnt, 28);
    appendU32(sfnt, nameTable.size());
    sfnt.append(nameTable);
    return sfnt;
}

} // namespace textfx::test
