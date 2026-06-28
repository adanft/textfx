#include "fonts/FontResolver.h"

#include <QColor>
#include <QDir>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

namespace {
QString quote(const QString& value)
{
    QString escaped = value;
    escaped.replace(u'\\', QStringLiteral("\\\\"));
    escaped.replace(u'\n', QStringLiteral("\\n"));
    escaped.replace(u'\"', QStringLiteral("\\\""));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}

QStringList matchingFamilies(const QString& requested)
{
    QStringList matches;
    const QString needle = requested.trimmed();
    for (const QString& family : QFontDatabase::families()) {
        if (family.compare(needle, Qt::CaseInsensitive) == 0 || family.contains(needle, Qt::CaseInsensitive) || needle.contains(family, Qt::CaseInsensitive)) {
            matches.push_back(family);
        }
    }
    matches.removeDuplicates();
    return matches;
}

void printFontInfo(QTextStream& out, const QString& label, const QFont& font)
{
    const QFontInfo info(font);
    out << label << ":\n";
    out << "  requestedFamily: " << quote(font.family()) << "\n";
    out << "  requestedStyleName: " << quote(font.styleName()) << "\n";
    out << "  resolvedFamily: " << quote(info.family()) << "\n";
    out << "  resolvedStyleName: " << quote(info.styleName()) << "\n";
    out << "  exactMatch: " << (info.exactMatch() ? "true" : "false") << "\n";
}

bool renderSample(const QString& path, const QString& title, const QFont& font)
{
    QImage image(QSize(760, 180), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(30, 30, 30));

    QFont labelFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    labelFont.setPixelSize(18);
    painter.setFont(labelFont);
    painter.drawText(QRect(20, 16, 720, 30), Qt::AlignLeft, title);

    QFont sampleFont = font;
    sampleFont.setPixelSize(72);
    painter.setFont(sampleFont);
    painter.drawText(QRect(20, 60, 720, 95), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("TextFX font sample 0123"));
    painter.end();

    return image.save(path, "PNG");
}
} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (app.arguments().contains(QStringLiteral("--help")) || app.arguments().contains(QStringLiteral("-h"))) {
        out << "Usage: " << app.applicationName() << " [font family] [output-dir]\n";
        out << "Defaults to the first Qt font database family when no family is provided.\n";
        out << "Set TEXTFX_DIAGNOSTIC_FONT_FILE to load a specific font file before resolving.\n";
        return 0;
    }

    const QStringList allFamilies = QFontDatabase::families();
    const QString requested = app.arguments().size() > 1 ? app.arguments().at(1) : allFamilies.value(0, QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
    const QString outputDir = app.arguments().size() > 2 ? app.arguments().at(2) : QString();

    const QString fontFile = QProcessEnvironment::systemEnvironment().value(QStringLiteral("TEXTFX_DIAGNOSTIC_FONT_FILE"));
    if (!fontFile.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontFile);
        out << "addApplicationFont: id=" << id << " file=" << quote(fontFile) << " families=[";
        const QStringList appFamilies = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList{};
        for (qsizetype i = 0; i < appFamilies.size(); ++i) out << (i == 0 ? "" : ", ") << quote(appFamilies.at(i));
        out << "]\n";
    }

    out << "TextFX font diagnostic\n";
    out << "requested: " << quote(requested) << "\n";
    const QStringList substitutes = QFont::substitutes(requested);
    out << "QFont substitutes: [";
    for (qsizetype i = 0; i < substitutes.size(); ++i) out << (i == 0 ? "" : ", ") << quote(substitutes.at(i));
    out << "]\n";

    const QStringList matches = matchingFamilies(requested);
    out << "QFontDatabase matches: " << matches.size() << "\n";
    for (const QString& family : matches) {
        out << "  family: " << quote(family) << "\n";
        const QStringList styles = QFontDatabase::styles(family);
        out << "    styles: [";
        for (qsizetype i = 0; i < styles.size(); ++i) out << (i == 0 ? "" : ", ") << quote(styles.at(i));
        out << "]\n";
    }

    QFont requestedFont(requested);
    requestedFont.setPixelSize(72);
    const QString styleName = QProcessEnvironment::systemEnvironment().value(QStringLiteral("TEXTFX_DIAGNOSTIC_STYLE_NAME"));
    if (!styleName.isEmpty()) requestedFont.setStyleName(styleName);
    printFontInfo(out, QStringLiteral("QFontInfo requested"), requestedFont);

    const textfx::ResolvedFont resolved = textfx::resolveFont(requestedFont);
    out << "TextFX resolveFont:\n";
    out << "  requestedFamily: " << quote(resolved.requestedFamily) << "\n";
    out << "  resolvedFontFamily: " << quote(resolved.font.family()) << "\n";
    out << "  qFontInfoFamily: " << quote(QFontInfo(resolved.font).family()) << "\n";
    out << "  requestedAvailable: " << (resolved.requestedAvailable ? "true" : "false") << "\n";
    out << "  exactMatch: " << (resolved.exactMatch ? "true" : "false") << "\n";
    out << "  fellBack: " << (resolved.fellBack ? "true" : "false") << "\n";
    out << "render/export would use fallback: " << (resolved.fellBack ? "true" : "false") << "\n";

    printFontInfo(out, QStringLiteral("QFontInfo TextFX resolved"), resolved.font);

    if (!outputDir.isEmpty()) {
        QDir dir(outputDir);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            err << "Could not create output directory: " << outputDir << "\n";
            return 2;
        }
        const QString requestedPath = dir.filePath(QStringLiteral("font-requested.png"));
        const QString resolvedPath = dir.filePath(QStringLiteral("font-resolved.png"));
        const bool requestedPng = renderSample(requestedPath, QStringLiteral("Requested: %1").arg(requested), requestedFont);
        const bool resolvedPng = renderSample(resolvedPath, QStringLiteral("TextFX resolved: %1").arg(resolved.font.family()), resolved.font);
        out << "diagnostic PNG requested: " << quote(requestedPath) << " saved=" << (requestedPng ? "true" : "false") << "\n";
        out << "diagnostic PNG resolved: " << quote(resolvedPath) << " saved=" << (resolvedPng ? "true" : "false") << "\n";
    }

    return 0;
}
