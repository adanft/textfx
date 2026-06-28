#include "fonts/FontResolver.h"

#include "fonts/SfntNames.h"

#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>

namespace textfx {
namespace {
QStringList fontNamesFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return detail::fontNamesFromSfntData(file.readAll());
}

QStringList fontSearchRoots()
{
    QSet<QString> roots;
    for (const QString& path : QStandardPaths::standardLocations(QStandardPaths::FontsLocation)) roots.insert(QDir::cleanPath(path));
#ifdef Q_OS_UNIX
    roots.insert(QDir::homePath() + QStringLiteral("/.fonts"));
    roots.insert(QDir::homePath() + QStringLiteral("/.local/share/fonts"));
    roots.insert(QStringLiteral("/usr/local/share/fonts"));
    roots.insert(QStringLiteral("/usr/share/fonts"));
#endif
    return QStringList(roots.begin(), roots.end());
}

bool fontResolvesToKnownName(const QString& family, const QSet<QString>& names)
{
    QFont font(family);
    const QString resolved = QFontInfo(font).family().trimmed();
    for (const QString& name : names) {
        if (resolved.compare(name, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

bool fontResolvesToSelf(const QString& family)
{
    QFont font(family);
    const QFontInfo info(font);
    return info.exactMatch() || info.family().compare(family, Qt::CaseInsensitive) == 0;
}

QHash<QString, QString> fontNameAliases()
{
    QHash<QString, QString> aliases;
    for (const QString& root : fontSearchRoots()) {
        if (!QDir(root).exists()) continue;
        QDirIterator files(root, {QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QStringList names = fontNamesFromFile(files.next());
            if (names.size() < 2) continue;

            const QSet<QString> nameSet(names.begin(), names.end());
            QString usableFamily;
            for (const QString& name : names) {
                if (fontResolvesToKnownName(name, nameSet)) {
                    usableFamily = name;
                    break;
                }
            }
            if (usableFamily.isEmpty()) continue;

            aliases.insert(detail::aliasesForSfntNames(names, usableFamily));
        }
    }
    return aliases;
}

QString availableFamily(const QString& requested)
{
    const QStringList families = QFontDatabase::families();
    if (families.contains(requested)) return requested;
    for (const QString& family : families) {
        if (family.compare(requested, Qt::CaseInsensitive) == 0) return family;
    }
    return {};
}

QString bracketAlias(const QString& requested)
{
    if (!requested.endsWith(QLatin1Char(']'))) return {};
    const qsizetype open = requested.lastIndexOf(QLatin1Char('['));
    if (open <= 0) return {};
    return requested.left(open).trimmed();
}

QString availableFamilyOrAlias(const QString& requested)
{
    if (requested.isEmpty()) return {};

    if (const QString family = availableFamily(requested); !family.isEmpty() && fontResolvesToSelf(family)) return family;

    static const QHash<QString, QString> aliases = fontNameAliases();
    if (const QString alias = aliases.value(requested.toCaseFolded()); !alias.isEmpty()) return alias;

    const QFontInfo info{QFont(requested)};
    if (info.exactMatch()) {
        if (const QString family = availableFamily(info.family()); !family.isEmpty()) return family;
        return info.family();
    }

    for (const QString& substitute : QFont::substitutes(requested)) {
        if (const QString family = availableFamily(substitute.trimmed()); !family.isEmpty()) return family;
    }

    if (const QString alias = bracketAlias(requested); !alias.isEmpty()) {
        if (const QString family = availableFamily(alias); !family.isEmpty()) return family;
        for (const QString& substitute : QFont::substitutes(alias)) {
            if (const QString family = availableFamily(substitute.trimmed()); !family.isEmpty()) return family;
        }
    }

    return {};
}

QString fallbackFamily()
{
    const QString family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family().trimmed();
    return family.isEmpty() ? QStringLiteral("sans-serif") : family;
}

bool isGenericFamily(const QString& family)
{
    return family == QStringLiteral("sans-serif") || family == QStringLiteral("serif") || family == QStringLiteral("monospace");
}
} // namespace

ResolvedFont resolveFont(QFont requested)
{
    ResolvedFont result;
    result.requestedFamily = requested.family().trimmed();

    const QString listedFamily = availableFamilyOrAlias(result.requestedFamily);
    if (!listedFamily.isEmpty()) requested.setFamily(listedFamily);

    const QFontInfo requestedInfo(requested);
    result.requestedAvailable = !listedFamily.isEmpty() || requestedInfo.exactMatch() || isGenericFamily(result.requestedFamily);
    result.exactMatch = requestedInfo.exactMatch();

    if (!result.requestedFamily.isEmpty() && !result.requestedAvailable) {
        requested.setFamily(fallbackFamily());
        result.fellBack = true;
    }

    result.font = requested;
    result.resolvedFamily = QFontInfo(result.font).family();
    return result;
}

} // namespace textfx
