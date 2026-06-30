#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringView>

class QObject;
class QQuickItem;
class QWindow;

namespace textfx::test {

void registerQmlTypes();
void typeText(QWindow* window, QStringView text);
QObject* findVisualChildByName(QQuickItem* item, const QString& objectName);

void touch(const QString& path, QSize size = {32, 24});
QJsonObject readJson(const QString& path);
bool hasPngMagic(const QString& path);

QString readQmlFile(const QString& name);
QString qmlSource();

bool imageDiffers(const QString& a, const QString& b);
bool imagesDiffer(const QImage& first, const QImage& second);
QRect visibleBounds(const QImage& image, const QColor& background);

template <typename Predicate>
int countPixels(const QImage& image, const Predicate& predicate)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (predicate(image.pixelColor(x, y))) ++count;
        }
    }
    return count;
}

QByteArray utf16be(const QString& text);
QByteArray syntheticSfntNameTable();

} // namespace textfx::test
