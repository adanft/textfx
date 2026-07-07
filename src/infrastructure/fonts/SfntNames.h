#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QStringList>

namespace textfx::detail {

inline quint16 sfntU16(const QByteArray &data, qsizetype offset) {
  return (static_cast<quint8>(data.at(offset)) << 8) |
         static_cast<quint8>(data.at(offset + 1));
}

inline quint32 sfntU32(const QByteArray &data, qsizetype offset) {
  return (static_cast<quint32>(sfntU16(data, offset)) << 16) |
         sfntU16(data, offset + 2);
}

inline QString sfntUtf16be(const QByteArray &bytes) {
  QString text;
  text.reserve(bytes.size() / 2);
  for (qsizetype i = 0; i + 1 < bytes.size(); i += 2)
    text.append(QChar(sfntU16(bytes, i)));
  return text.trimmed();
}

inline QString sfntNameRecord(const QByteArray &bytes, quint16 platformId) {
  if (platformId == 0 || platformId == 3)
    return sfntUtf16be(bytes);
  return QString::fromLatin1(bytes).trimmed();
}

inline QStringList fontNamesFromSfntData(const QByteArray &data) {
  if (data.size() < 12)
    return {};

  const quint16 tableCount = sfntU16(data, 4);
  qsizetype nameOffset = -1;
  qsizetype nameLength = 0;
  for (quint16 i = 0; i < tableCount; ++i) {
    const qsizetype record = 12 + i * 16;
    if (record + 16 > data.size())
      return {};
    if (data.sliced(record, 4) == "name") {
      nameOffset = sfntU32(data, record + 8);
      nameLength = sfntU32(data, record + 12);
      break;
    }
  }
  if (nameOffset < 0 || nameOffset + nameLength > data.size() || nameLength < 6)
    return {};

  const quint16 count = sfntU16(data, nameOffset + 2);
  const qsizetype stringBase = nameOffset + sfntU16(data, nameOffset + 4);
  QSet<QString> names;
  for (quint16 i = 0; i < count; ++i) {
    const qsizetype record = nameOffset + 6 + static_cast<qsizetype>(i) * 12;
    if (record + 12 > data.size())
      break;
    const quint16 platformId = sfntU16(data, record);
    const quint16 nameId = sfntU16(data, record + 6);
    if (nameId != 1 && nameId != 4 && nameId != 16)
      continue;

    const quint16 length = sfntU16(data, record + 8);
    const quint16 offset = sfntU16(data, record + 10);
    if (stringBase + offset + length > data.size())
      continue;

    const QString name =
        sfntNameRecord(data.sliced(stringBase + offset, length), platformId);
    if (!name.isEmpty())
      names.insert(name);
  }
  return QStringList(names.begin(), names.end());
}

inline QHash<QString, QString>
aliasesForSfntNames(const QStringList &names, const QString &usableFamily) {
  QHash<QString, QString> aliases;
  if (names.size() < 2 || usableFamily.isEmpty())
    return aliases;

  const QSet<QString> nameSet(names.begin(), names.end());
  if (!nameSet.contains(usableFamily))
    return aliases;

  for (const QString &name : names)
    aliases.insert(name.toCaseFolded(), usableFamily);
  return aliases;
}

} // namespace textfx::detail
