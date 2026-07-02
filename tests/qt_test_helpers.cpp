#include "qt_test_helpers.h"

#include "ui/OutlinedTextItem.h"

#include <QFile>
#include <QJsonDocument>
#include <QQuickItem>
#include <QTest>
#include <QVector>
#include <QWindow>
#include <qqml.h>

#include <algorithm>
#include <cstring>
#include <iterator>

namespace textfx::test {

namespace {

bool isSourceLayoutChar(const QChar ch) { return ch.isSpace() || ch == u';'; }

bool isIdentifierChar(const QChar ch) {
  return ch == u'_' || ch.isLetterOrNumber();
}

qsizetype quotedIdentifierKeyEnd(QStringView source, qsizetype quoteIndex) {
  if (quoteIndex >= source.size() || source.at(quoteIndex) != u'"')
    return -1;

  qsizetype index = quoteIndex + 1;
  bool escaped = false;
  while (index < source.size()) {
    const QChar ch = source.at(index);
    if (escaped) {
      escaped = false;
    } else if (ch == u'\\') {
      escaped = true;
    } else if (ch == u'"') {
      break;
    }
    ++index;
  }
  if (index >= source.size() || index == quoteIndex + 1)
    return -1;
  for (qsizetype keyIndex = quoteIndex + 1; keyIndex < index; ++keyIndex) {
    if (!isIdentifierChar(source.at(keyIndex)))
      return -1;
  }

  qsizetype colonIndex = index + 1;
  while (colonIndex < source.size() &&
         isSourceLayoutChar(source.at(colonIndex)))
    ++colonIndex;
  return colonIndex < source.size() && source.at(colonIndex) == u':' ? index
                                                                     : -1;
}

void updateStringState(const QChar ch, bool &inString, bool &escaped) {
  if (!inString) {
    if (ch == u'"')
      inString = true;
    return;
  }

  if (escaped) {
    escaped = false;
    return;
  }
  if (ch == u'\\') {
    escaped = true;
    return;
  }
  if (ch == u'"')
    inString = false;
}

QString normalizedSource(QStringView source, QVector<qsizetype> *sourceMap) {
  QString result;
  result.reserve(source.size());
  bool inString = false;
  bool escaped = false;
  for (qsizetype i = 0; i < source.size(); ++i) {
    const QChar ch = source.at(i);
    if (!inString) {
      const qsizetype keyEnd = quotedIdentifierKeyEnd(source, i);
      if (keyEnd >= 0) {
        for (qsizetype keyIndex = i + 1; keyIndex < keyEnd; ++keyIndex) {
          result.append(source.at(keyIndex));
          if (sourceMap)
            sourceMap->append(keyIndex);
        }
        i = keyEnd;
        continue;
      }
    }
    if (inString || !isSourceLayoutChar(ch)) {
      result.append(ch);
      if (sourceMap)
        sourceMap->append(i);
    }
    updateStringState(ch, inString, escaped);
  }
  return result;
}

} // namespace

void registerQmlTypes() {
  static const int registered = qmlRegisterType<textfx::OutlinedTextItem>(
      "TextFX.Ui", 1, 0, "OutlinedTextItem");
  Q_UNUSED(registered);
}

void typeText(QWindow *window, QStringView text) {
  for (const QChar ch : text)
    QTest::keyClick(window, ch.toLatin1());
}

QObject *findVisualChildByName(QQuickItem *item, const QString &objectName) {
  if (!item)
    return nullptr;
  if (item->objectName() == objectName)
    return item;
  for (QQuickItem *child : item->childItems()) {
    if (QObject *found = findVisualChildByName(child, objectName))
      return found;
  }
  return nullptr;
}

void touch(const QString &path, QSize size) {
  QImage image(size, QImage::Format_RGBA8888);
  image.fill(QColor(240, 240, 240, 255));
  QVERIFY(image.save(path, "PNG"));
}

QJsonObject readJson(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}

bool hasPngMagic(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) &&
         file.read(8) == QByteArray::fromHex("89504e470d0a1a0a");
}

QString readQmlFile(const QString &name) {
  QFile file(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/") + name);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return QString::fromUtf8(file.readAll());
}

QString qmlSource() {
  return readQmlFile(QStringLiteral("Main.qml")) + QLatin1Char('\n') +
         readQmlFile(QStringLiteral("BoxEffectsSection.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("BoxMoveInteractionState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("BoxRotateInteractionState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("BoxResizeInteractionState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("CanvasInteractionState.qml")) +
         QLatin1Char('\n') + readQmlFile(QStringLiteral("ColorButton.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("CentralCanvasShell.qml")) +
          QLatin1Char('\n') + readQmlFile(QStringLiteral("EditorChrome.qml")) +
          QLatin1Char('\n') +
           readQmlFile(QStringLiteral("LeftInspectorPanel.qml")) +
           QLatin1Char('\n') +
            readQmlFile(QStringLiteral("TextPropertiesSection.qml")) +
            QLatin1Char('\n') +
            readQmlFile(QStringLiteral("TextPresetsSection.qml")) +
           QLatin1Char('\n') + readQmlFile(QStringLiteral("LayersSection.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("NavigationSection.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("PageTextsSection.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("PathHandleInteractionState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("PerspectiveGeometry.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("PerspectiveInteractionState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("RightInspectorPanel.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("ShortcutMenuItem.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("SelectedBoxState.qml")) +
         QLatin1Char('\n') +
         readQmlFile(QStringLiteral("TextEffectsSection.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("TextBoxDelegate.qml")) +
          QLatin1Char('\n') + readQmlFile(QStringLiteral("TextEditOverlay.qml")) +
          QLatin1Char('\n') + readQmlFile(QStringLiteral("TextPathGuide.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("TextResizeHandles.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("TextRotateHandle.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("TextPathHandles.qml")) +
          QLatin1Char('\n') +
          readQmlFile(QStringLiteral("TextStyleButton.qml")) +
         QLatin1Char('\n') + readQmlFile(QStringLiteral("ViewportMetrics.qml"));
}

QString compactSource(QStringView source) {
  return normalizedSource(source, nullptr);
}

bool sourceContainsIgnoringWhitespace(const QString &source,
                                      const QString &needle) {
  return compactSource(source).contains(compactSource(needle));
}

qsizetype indexOfIgnoringWhitespace(const QString &source,
                                    const QString &needle, qsizetype from) {
  QVector<qsizetype> sourceMap;
  const QString compactedSource = normalizedSource(source, &sourceMap);
  const QString compactedNeedle = compactSource(needle);
  const qsizetype start = std::lower_bound(sourceMap.cbegin(), sourceMap.cend(),
                                           std::max<qsizetype>(0, from)) -
                          sourceMap.cbegin();
  const qsizetype compactedIndex =
      compactedSource.indexOf(compactedNeedle, start);
  return compactedIndex >= 0 && compactedIndex < sourceMap.size()
             ? sourceMap.at(compactedIndex)
             : -1;
}

bool imageDiffers(const QString &a, const QString &b) {
  const QImage first = QImage(a).convertToFormat(QImage::Format_RGBA8888);
  const QImage second = QImage(b).convertToFormat(QImage::Format_RGBA8888);
  if (first.isNull() || second.isNull())
    return false;
  return imagesDiffer(first, second);
}

bool imagesDiffer(const QImage &first, const QImage &second) {
  if (first.size() != second.size())
    return true;
  for (int y = 0; y < first.height(); ++y) {
    if (std::memcmp(first.constScanLine(y), second.constScanLine(y),
                    static_cast<std::size_t>(first.bytesPerLine())) != 0)
      return true;
  }
  return false;
}

QRect visibleBounds(const QImage &image, const QColor &background) {
  QRect bounds;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor color = image.pixelColor(x, y);
      if (color.alpha() > 0 && color != background)
        bounds = bounds.isNull() ? QRect(x, y, 1, 1)
                                 : bounds.united(QRect(x, y, 1, 1));
    }
  }
  return bounds;
}

QByteArray utf16be(const QString &text) {
  QByteArray bytes;
  bytes.reserve(text.size() * 2);
  for (const QChar ch : text) {
    const ushort value = ch.unicode();
    bytes.append(static_cast<char>((value >> 8) & 0xff));
    bytes.append(static_cast<char>(value & 0xff));
  }
  return bytes;
}

QByteArray syntheticSfntNameTable() {
  struct NameRecord {
    quint16 platform;
    quint16 encoding;
    quint16 language;
    quint16 nameId;
    QByteArray value;
  };

  const NameRecord records[] = {
      {3, 1, 0x0409, 1, utf16be(QStringLiteral("TextFX Alias Test"))},
      {3, 1, 0x0409, 4,
       utf16be(QStringLiteral("000 TextFX Alias Test [Display]"))},
      {3, 1, 0x0409, 16, utf16be(QStringLiteral("TextFX Usable Family"))},
      {3, 1, 0x0409, 2, utf16be(QStringLiteral("Ignored Style Name"))},
  };

  auto appendU16 = [](QByteArray &data, quint16 value) {
    data.append(static_cast<char>((value >> 8) & 0xff));
    data.append(static_cast<char>(value & 0xff));
  };
  auto appendU32 = [&appendU16](QByteArray &data, quint32 value) {
    appendU16(data, static_cast<quint16>((value >> 16) & 0xffff));
    appendU16(data, static_cast<quint16>(value & 0xffff));
  };

  QByteArray strings;
  QByteArray nameTable;
  appendU16(nameTable, 0);
  appendU16(nameTable, std::size(records));
  appendU16(nameTable, 6 + std::size(records) * 12);

  for (const NameRecord &record : records) {
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
