#include "app/EditorController.h"
#include "core/EffectLimits.h"
#include "fonts/FontResolver.h"
#include "fonts/SfntNames.h"
#include "render/RenderGraph.h"
#include "ui/OutlinedTextItem.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QFontDatabase>
#include <QFontInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTest>
#include <QUrl>
#include <qqml.h>

#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>

using namespace textfx;

namespace {
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

void touch(const QString& path, QSize size = {32, 24})
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
    return readQmlFile(QStringLiteral("Main.qml")) + QLatin1Char('\n') + readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
}

bool imageDiffers(const QString& a, const QString& b)
{
    const QImage first = QImage(a).convertToFormat(QImage::Format_RGBA8888);
    const QImage second = QImage(b).convertToFormat(QImage::Format_RGBA8888);
    if (first.isNull() || second.isNull()) return false;
    if (first.size() != second.size()) return true;
    for (int y = 0; y < first.height(); ++y) {
        if (std::memcmp(first.constScanLine(y), second.constScanLine(y), static_cast<std::size_t>(first.bytesPerLine())) != 0) return true;
    }
    return false;
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

int countPixels(const QImage& image, const auto& predicate)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (predicate(image.pixelColor(x, y))) ++count;
        }
    }
    return count;
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
} // namespace

class QtSmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void blurKernelRadiusIsCapped()
    {
        QCOMPARE(cappedBlurKernelRadius(0), 0);
        QCOMPARE(cappedBlurKernelRadius(1), 1);
        QCOMPARE(cappedBlurKernelRadius(MaxBlurKernelRadius), MaxBlurKernelRadius);
        QCOMPARE(cappedBlurKernelRadius(MaxBlurKernelRadius + 100), MaxBlurKernelRadius);
    }

    void noProjectDisablesProjectActions()
    {
        EditorController editor;

        QVERIFY(!editor.hasProject());
        QVERIFY(editor.actionEnabled(QStringLiteral("open")));
        QVERIFY(!editor.actionEnabled(QStringLiteral("save")));
        QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
    }

    void commandRefreshesDocumentState()
    {
        EditorController editor;
        QSignalSpy changed(&editor, &EditorController::documentChanged);

        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);

        QCOMPARE(editor.boxes().size(), 1);
        QCOMPARE(editor.selectedIndex(), 0);
        QVERIFY(editor.dirty());
        QVERIFY(changed.count() >= 1);
    }

    void createdBoxesAreEmptyExactDragWithTypeXMinimum()
    {
        EditorController editor;
        editor.newDocument();

        editor.createTextBox(10, 20, 13, 14);
        editor.createTextBox(30, 40, 1, 2);

        QCOMPARE(editor.boxes().size(), 1);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QString());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("w")).toDouble(), 13.0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("h")).toDouble(), 14.0);
    }

    void boxesExposeRequestedAndResolvedFontFamilies()
    {
        auto expectedResolvedFamily = [](const QString& family) {
            QFont font(family);
            return resolveFont(font).font.family();
        };

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);

        const QString missingFamily = QStringLiteral("TextFX Missing Font For Resolver Test");
        editor.setSelectedFontFamily(missingFamily);

        QVariantMap box = editor.boxes().at(0).toMap();
        QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), missingFamily);
        QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(), expectedResolvedFamily(missingFamily));
        QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() != missingFamily);

        const QString beatDown = QStringLiteral("000 BeatDownBB [TeddyBear]");
        QFont beatDownFont(beatDown);
        const ResolvedFont beatDownResolution = resolveFont(beatDownFont);
        if (beatDownResolution.requestedAvailable) {
            QCOMPARE(QFontInfo(beatDownResolution.font).family(), beatDown);

            editor.setSelectedFontFamily(beatDown);
            box = editor.boxes().at(0).toMap();

            QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), beatDown);
            QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(), beatDownResolution.font.family());
            QVERIFY(!box.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty());
        }
    }

    void activeTextEditingDefersModelResetUntilEditEnds()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.beginTextEdit();

        QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
        QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

        editor.updateSelectedText(QStringLiteral("abc"));

        QCOMPARE(documentChanged.count(), 0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("abc"));
        QVERIFY(editor.dirty());
        QVERIFY(stateChanged.count() >= 1);

        editor.endTextEdit();

        QCOMPARE(documentChanged.count(), 1);
    }

    void qmlTextEditOverlayKeepsFocusAndTextAcrossTyping()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 260, 90);
        editor.setSelectedFontSize(20);
        editor.setSelectedLineSpacing(7);
        editor.beginTextEdit();

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QObject* textArea = nullptr;
        QTRY_VERIFY(textArea = findVisualChildByName(window->contentItem(), QStringLiteral("boxTextArea")));
        QTRY_VERIFY(textArea->property("visible").toBool());
        QTRY_VERIFY(textArea->property("activeFocus").toBool());

        auto* quickTextDocument = textArea->property("textDocument").value<QObject*>();
        QVERIFY(quickTextDocument);
        auto* documentWrapper = qobject_cast<QQuickTextDocument*>(quickTextDocument);
        QVERIFY(documentWrapper);
        auto* document = documentWrapper->textDocument();
        QVERIFY(document);
        const auto currentDocument = [&]() -> QTextDocument* {
            auto* currentTextArea = findVisualChildByName(window->contentItem(), QStringLiteral("boxTextArea"));
            if (!currentTextArea) return nullptr;
            auto* currentQuickTextDocument = currentTextArea->property("textDocument").value<QObject*>();
            auto* currentDocumentWrapper = qobject_cast<QQuickTextDocument*>(currentQuickTextDocument);
            return currentDocumentWrapper ? currentDocumentWrapper->textDocument() : nullptr;
        };
        const auto lineSpacing = [&]() {
            auto* current = currentDocument();
            return current ? current->firstBlock().blockFormat().lineHeight() : std::numeric_limits<double>::quiet_NaN();
        };
        const auto lineSpacingType = [&]() {
            auto* current = currentDocument();
            return current ? current->firstBlock().blockFormat().lineHeightType() : -1;
        };
        QCOMPARE(lineSpacingType(), int(QTextBlockFormat::LineDistanceHeight));
        QCOMPARE(lineSpacing(), 7.0);

        typeText(window, u"abc");
        QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("abc"));
        QVERIFY(editor.editingText());
        QVERIFY(textArea->property("activeFocus").toBool());

        typeText(window, u"def");
        QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("abcdef"));
        QVERIFY(editor.editingText());
        QVERIFY(textArea->property("activeFocus").toBool());

        QVERIFY(window->setProperty("zoom", 2.0));
        QTRY_COMPARE(lineSpacing(), 14.0);

        editor.setSelectedLineSpacing(9);
        QTRY_COMPARE(lineSpacing(), 18.0);
    }

    void copyPastePreservesCopiedBox()
    {
        EditorController editor;

        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.updateSelectedText(QStringLiteral("Copied text"));
        editor.rotateSelected(12.5);
        editor.setPerspectiveHandle(QStringLiteral("ne"), 0.9, 0.1);
        editor.setSelectedPathMode(1);
        editor.setPathHandle(0, 0.2, 0.8);
        editor.copySelected();
        editor.pasteBox();

        const auto boxes = editor.boxes();
        QCOMPARE(boxes.size(), 2);
        const auto pasted = boxes.at(1).toMap();
        QCOMPARE(pasted.value(QStringLiteral("text")).toString(), QStringLiteral("Copied text"));
        QVERIFY(!pasted.value(QStringLiteral("text")).toString().startsWith(QLatin1Char('{')));
        QCOMPARE(pasted.value(QStringLiteral("x")).toDouble(), 26.0);
        QCOMPARE(pasted.value(QStringLiteral("y")).toDouble(), 36.0);
        QCOMPARE(pasted.value(QStringLiteral("rotation")).toDouble(), 12.5);
        QVERIFY(pasted.value(QStringLiteral("perspective")).toBool());
        const auto pastedNe = pasted.value(QStringLiteral("perspectiveNe")).toList();
        QCOMPARE(pastedNe.at(0).toDouble(), 0.9);
        QCOMPARE(pastedNe.at(1).toDouble(), 0.1);
        QVERIFY(pasted.value(QStringLiteral("path")).toBool());
        QCOMPARE(pasted.value(QStringLiteral("pathMode")).toInt(), 1);
        const auto pastedPathPoint = pasted.value(QStringLiteral("pathPoints")).toList().at(0).toList();
        QCOMPARE(pastedPathPoint.at(0).toDouble(), 0.2);
        QCOMPARE(pastedPathPoint.at(1).toDouble(), 0.8);
        QCOMPARE(editor.selectedIndex(), 1);
        QVERIFY(editor.dirty());

        QGuiApplication::clipboard()->setText(QStringLiteral("Plain text"));
        editor.pasteBox();

        QCOMPARE(editor.boxes().at(2).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Plain text"));
    }

    void openProjectLoadsFixtureImagesAndEmptyMissingData()
    {
        EditorController editor;
        editor.openProject(QStringLiteral(TEXTFX_FIXTURE_DIR "/project-workflow"));

        QVERIFY(editor.hasProject());
        QCOMPARE(editor.pageCount(), 2);
        QCOMPARE(editor.currentPageIndex(), 0);
        QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
        QVERIFY(editor.currentPageUrl().isLocalFile());
        QCOMPARE(editor.boxes().size(), 0);
        QVERIFY(!editor.dirty());
    }

    void openProjectUrlUsesQtLocalFileConversion()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString projectPath = dir.filePath(QStringLiteral("project with spaces"));
        QVERIFY(QDir().mkpath(projectPath));
        touch(projectPath + QStringLiteral("/page1.png"));

        EditorController editor;
        editor.openProjectUrl(QUrl::fromLocalFile(projectPath));

        QVERIFY(editor.hasProject());
        QCOMPARE(editor.pageCount(), 1);
        QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
        QVERIFY(editor.currentPageUrl().isLocalFile());
    }

    void openProjectUrlRejectsNonLocalUrls()
    {
        EditorController editor;
        editor.openProjectUrl(QUrl(QStringLiteral("https://example.invalid/project")));

        QVERIFY(!editor.hasProject());
        QCOMPARE(editor.notification(), QStringLiteral("Only local project folders can be opened."));
    }

    void navigationAutosavesAndReloadsCurrentPage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(10, 20, 100, 50);
        editor.updateSelectedText(QStringLiteral("Saved on navigation"));
        QVERIFY(editor.dirty());

        editor.nextPage();

        QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
        QVERIFY(!editor.dirty());
        const auto savedPath = dir.filePath(QStringLiteral(".typex/page1.json"));
        QVERIFY(QFile::exists(savedPath));
        QCOMPARE(readJson(savedPath).value(QStringLiteral("boxes")).toArray().size(), 1);

        editor.previousPage();

        QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
        QCOMPARE(editor.boxes().size(), 1);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Saved on navigation"));
    }

    void directPageSelectorAutosavesWithoutExportingPng()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));

        EditorController editor;
        editor.openProject(dir.path());
        QCOMPARE(editor.pageLabels(), QStringList({QStringLiteral("1 - page1.png"), QStringLiteral("2 - page2.png")}));
        editor.createTextBox(10, 20, 100, 50);
        editor.updateSelectedText(QStringLiteral("Jump saved"));

        editor.goToPage(1);

        QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".typex/page1.json"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("exported/page1.png"))));
        QCOMPARE(readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().size(), 1);
    }

    void openingProjectAbortsWhenCurrentAutosaveFails()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        touch(first.filePath(QStringLiteral("page1.png")));
        touch(second.filePath(QStringLiteral("page2.png")));

        EditorController editor;
        editor.openProject(first.path());
        editor.createTextBox(10, 20, 100, 50);
        QVERIFY(editor.dirty());

        QVERIFY(QFile(first.filePath(QStringLiteral(".typex"))).open(QIODevice::WriteOnly));
        editor.openProject(second.path());

        QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
        QVERIFY(editor.notification().contains(QStringLiteral(".typex")));
    }

    void saveWritesJsonAndCurrentExportedPng()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        editor.updateSelectedText(QStringLiteral("Saved page"));
        const auto output = dir.filePath(QStringLiteral("exported/page1.png"));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("exported")));
        QFile stale(output);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.write("stale export");
        stale.close();
        editor.save();

        QVERIFY(!editor.dirty());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".typex/page1.json"))));
        QCOMPARE(readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().size(), 1);

        QVERIFY2(hasPngMagic(output), qPrintable(editor.notification()));
        QVERIFY(imageDiffers(dir.filePath(QStringLiteral("page1.png")), output));
        QFile first(output);
        QVERIFY(first.open(QIODevice::ReadOnly));
        const QByteArray firstBytes = first.readAll();
        first.close();
        editor.updateSelectedText(QStringLiteral("Changed page"));
        editor.save();
        QFile second(output);
        QVERIFY(second.open(QIODevice::ReadOnly));
        QVERIFY(firstBytes != second.readAll());
        QVERIFY(editor.notification().contains(QStringLiteral("Saved boxes and exported PNG to")));
    }

    void saveReportsBoxSaveFailureLikeTypeX()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        QVERIFY(QFile(dir.filePath(QStringLiteral(".typex"))).open(QIODevice::WriteOnly));

        editor.save();

        QVERIFY(editor.notification().startsWith(QStringLiteral("Could not save boxes:")));
    }

    void saveAllExportsMultiplePagesWithSavedBoxes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));
        touch(dir.filePath(QStringLiteral("page3.png")));
        touch(dir.filePath(QStringLiteral("page4.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        editor.updateSelectedText(QStringLiteral("First page"));
        editor.save();
        editor.nextPage();
        editor.createTextBox(3, 4, 90, 50);
        editor.updateSelectedText(QStringLiteral("Second page"));
        QFile emptyPage(dir.filePath(QStringLiteral(".typex/page3.json")));
        QVERIFY(emptyPage.open(QIODevice::WriteOnly | QIODevice::Text));
        emptyPage.write(R"({"format":"typex.page-boxes.v1","page":"page3.png","boxes":[]})");
        emptyPage.close();
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("exported")));
        QFile stale(dir.filePath(QStringLiteral("exported/page3.png")));
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.write("stale export");
        stale.close();
        QFile staleMissingJson(dir.filePath(QStringLiteral("exported/page4.png")));
        QVERIFY(staleMissingJson.open(QIODevice::WriteOnly));
        staleMissingJson.write("stale export without json");
        staleMissingJson.close();
        editor.saveAll();

        QVERIFY(!editor.dirty());
        QCOMPARE(readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().size(), 1);
        QCOMPARE(readJson(dir.filePath(QStringLiteral(".typex/page2.json"))).value(QStringLiteral("boxes")).toArray().size(), 1);
        QCOMPARE(readJson(dir.filePath(QStringLiteral(".typex/page3.json"))).value(QStringLiteral("boxes")).toArray().size(), 0);

        QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("exported/page1.png"))), qPrintable(editor.notification()));
        QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("exported/page2.png"))), qPrintable(editor.notification()));
        QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("exported/page3.png"))), qPrintable(editor.notification()));
        QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("exported/page4.png"))), qPrintable(editor.notification()));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral(".typex/page4.json"))));
        QCOMPARE(editor.notification(), QStringLiteral("Exported 4 page(s)."));
    }

    void saveAllReportsPageExportFailures()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        editor.updateSelectedText(QStringLiteral("Missing source"));
        editor.save();
        editor.nextPage();
        editor.createTextBox(3, 4, 90, 50);
        editor.updateSelectedText(QStringLiteral("Still exports"));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("page1.png"))));

        editor.saveAll();

        QVERIFY(!editor.dirty());
        QVERIFY(editor.notification().contains(QStringLiteral("1 failed")));
        QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("exported/page2.png"))), qPrintable(editor.notification()));
    }

    void gradientAndPathUseLiveRendererWithoutPreviewArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 24, 18);
        editor.updateSelectedText(QStringLiteral("FX"));
        editor.setSelectedGradientEnabled(true);
        editor.setSelectedPathEnabled(true);

        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        QVERIFY(source.contains(QStringLiteral("gradientEnabled: modelData.gradient")));
        QVERIFY(source.contains(QStringLiteral("gradientDirection: modelData.gradientDirection")));
        QVERIFY(source.contains(QStringLiteral("gradientColorA: rootWindow.qmlColor(modelData.gradientColorA)")));
        QVERIFY(source.contains(QStringLiteral("gradientColorB: rootWindow.qmlColor(modelData.gradientColorB)")));
        QVERIFY(source.contains(QStringLiteral("pathEnabled: modelData.path")));
        QVERIFY(source.contains(QStringLiteral("pathMode: modelData.pathMode")));
        QVERIFY(source.contains(QStringLiteral("pathPoints: modelData.pathPoints")));
        QVERIFY(source.contains(QStringLiteral("id: pathGuide")));
        QVERIFY(source.contains(QStringLiteral("visible: boxRef.selected && boxRef.boxModel.path && boxRef.boxModel.pathPoints.length > 1")));
        QVERIFY(source.contains(QStringLiteral("transform: Matrix4x4 { matrix: pathGuide.rootWindow.perspectiveMatrix(pathGuide.boxRef.boxModel, pathGuide.width, pathGuide.height, pathGuide.rootWindow.viewDocScale(), pathGuide.boxRef.perspectiveActive) }")));
        QVERIFY(source.contains(QStringLiteral("function smoothGuidePoints(points)")));
        QVERIFY(source.contains(QStringLiteral("pathMode === 1 ? smoothGuidePoints(points) : points.map(point => guidePoint(point))")));
        QVERIFY(!source.contains(QStringLiteral("const baselineOffset = lineWidth / 2")));
        QVERIFY(source.contains(QStringLiteral("ctx.moveTo(guidePoints[0].x, guidePoints[0].y)")));
        QVERIFY(source.contains(QStringLiteral("for (let pass = 0; pass < 3 && result.length >= 3; ++pass)")));
        QVERIFY(source.contains(QStringLiteral("* 0.25")));
        QVERIFY(source.contains(QStringLiteral("* 0.75")));
        QVERIFY(source.contains(QStringLiteral("onPathModeChanged: requestPaint()")));
        QVERIFY(source.contains(QStringLiteral("onPathPointsChanged: requestPaint()")));
    }

    void qmlPathHandleDragUpdatesPathPoints()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 40, 160, 60);
        editor.updateSelectedText(QStringLiteral("Path"));
        editor.setSelectedPathEnabled(true);

        const QVariantList before = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("pathHandle")));
        auto* handle = qobject_cast<QQuickItem*>(object);
        QVERIFY(handle);
        QTRY_VERIFY(handle->isVisible());
        auto* box = handle->parentItem();
        QVERIFY(box);
        const double boxWidth = box->width();
        const double boxHeight = box->height();

        QSignalSpy changed(&editor, &EditorController::documentChanged);
        const QPoint start = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
        const QPoint end = start + QPoint(48, -24);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(12, -6));
        QTest::mouseMove(window, start + QPoint(24, -12));
        QTest::mouseMove(window, start + QPoint(36, -18));
        QTest::mouseMove(window, end);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);

        QTRY_VERIFY(changed.count() >= 1);
        const QVariantList after = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QVERIFY(after != before);
        const auto beforePoint = before.at(0).toList();
        const auto afterPoint = after.at(0).toList();
        const double beforeX = beforePoint.at(0).toDouble();
        const double beforeY = beforePoint.at(1).toDouble();
        const double afterX = afterPoint.at(0).toDouble();
        const double afterY = afterPoint.at(1).toDouble();
        const double expectedDx = 48.0 / boxWidth;
        const double expectedDy = -24.0 / boxHeight;
        QVERIFY(afterX - beforeX > 0.05);
        QVERIFY(beforeY - afterY > 0.05);
        QVERIFY(std::abs((afterX - beforeX) - expectedDx) < 0.08);
        QVERIFY(std::abs((afterY - beforeY) - expectedDy) < 0.08);

        QTest::mouseMove(window, end + QPoint(48, -24));
        QCoreApplication::processEvents();
        const QVariantList afterReleaseMove = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QCOMPARE(afterReleaseMove, after);
    }

    void qmlPathHandleFollowsPerspectivePlane()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 40, 160, 60);
        editor.setSelectedPathEnabled(true);
        editor.setPathHandle(0, 0.0, 0.0);
        editor.setPerspectiveHandle(QStringLiteral("nw"), 30.0, -10.0);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("pathHandle")));
        auto* handle = qobject_cast<QQuickItem*>(object);
        QVERIFY(handle);
        QTRY_VERIFY(handle->isVisible());
        auto* pathPlane = handle->parentItem();
        QVERIFY(pathPlane);
        auto* box = pathPlane->parentItem();
        QVERIFY(box);

        const QPointF raw = box->mapToScene(QPointF(0.0, 0.0));
        const QPointF expected = box->mapToScene(QPointF(30.0, -10.0));
        const QPointF actual = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2));

        QVERIFY(std::hypot(actual.x() - raw.x(), actual.y() - raw.y()) > 8.0);
        QVERIFY(std::hypot(actual.x() - expected.x(), actual.y() - expected.y()) < 2.0);

        const QVariantList before = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        const QPoint start = actual.toPoint();
        const QPoint end = start + QPoint(18, 0);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, end);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
        const QVariantList after = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QVERIFY(after != before);
        QVERIFY(after.at(0).toList().at(0).toDouble() > before.at(0).toList().at(0).toDouble());
    }

    void perspectiveUsesLiveClippedRendererWithoutPreviewArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")), {160, 80});

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(4, 4, 120, 40);
        editor.updateSelectedText(QStringLiteral("Perspective"));
        editor.setSelectedPerspectiveEnabled(true);

        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype delegateStart = source.indexOf(QStringLiteral("Rectangle {\n    id: boxDelegate"));
        const qsizetype textPerspectiveStart = source.indexOf(QStringLiteral("id: boxTextPerspective"), delegateStart);
        const qsizetype textAreaStart = source.indexOf(QStringLiteral("id: boxTextArea"), textPerspectiveStart);
        const qsizetype mouseAreaStart = source.indexOf(QStringLiteral("MouseArea {"), textAreaStart);
        QVERIFY(delegateStart >= 0);
        QVERIFY(textPerspectiveStart > delegateStart);
        QVERIFY(textAreaStart > textPerspectiveStart);
        QVERIFY(mouseAreaStart > textAreaStart);
        const QString delegateSource = source.mid(delegateStart, textAreaStart - delegateStart);
        const QString textPerspectiveSource = source.mid(textPerspectiveStart, textAreaStart - textPerspectiveStart);
        const QString textAreaSource = source.mid(textAreaStart, mouseAreaStart - textAreaStart);

        QVERIFY(delegateSource.contains(QStringLiteral("property bool perspectiveActive: boxModel.perspective && !editingSelected")));
        QVERIFY(textPerspectiveSource.contains(QStringLiteral("clip: true")));
        QVERIFY(textAreaSource.contains(QStringLiteral("clip: true")));
    }

    void blurUsesLiveRendererWithoutPreviewArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")), {160, 80});

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(4, 4, 120, 40);
        editor.updateSelectedText(QStringLiteral("Blur"));
        editor.setSelectedBlurEnabled(true);
        editor.setSelectedBlurSize(6);

        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        QVERIFY(!source.contains(QStringLiteral("import QtQuick.Effects")));
        QVERIFY(!source.contains(QStringLiteral("MultiEffect {")));
        QVERIFY(!source.contains(QStringLiteral("liveGpuBlurActive")));
        QVERIFY(source.contains(QStringLiteral("blurSize: modelData.blur && modelData.blurSize > 0 ? modelData.blurSize : 0")));
    }

    void shadowUsesLiveRendererWithoutPreviewArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")), {160, 80});

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(4, 4, 120, 40);
        editor.updateSelectedText(QStringLiteral("Shadow"));
        editor.setSelectedShadowEnabled(true);
        editor.setSelectedShadowBlurSize(6);

        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        QVERIFY(source.contains(QStringLiteral("shadowEnabled: modelData.shadow")));
        QVERIFY(source.contains(QStringLiteral("shadowBlurSize: modelData.shadow && modelData.shadowBlurSize > 0 ? modelData.shadowBlurSize : 0")));
    }

    void qmlBlurEnabledBoxKeepsOutlinedRendererVisibleUntilEditing()
    {
        registerQmlTypes();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")), {160, 80});

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(4, 4, 120, 40);
        editor.updateSelectedText(QStringLiteral("Blur"));
        editor.setSelectedBlurEnabled(true);
        editor.setSelectedBlurSize(6);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QObject* outlinedText = nullptr;
        QTRY_VERIFY(outlinedText = findVisualChildByName(window->contentItem(), QStringLiteral("boxOutlinedText")));
        QTRY_VERIFY(outlinedText->property("visible").toBool());
        QCOMPARE(outlinedText->property("blurSize").toInt(), 6);

        QObject* textArea = nullptr;
        QTRY_VERIFY(textArea = findVisualChildByName(window->contentItem(), QStringLiteral("boxTextArea")));
        editor.beginTextEdit();
        QTRY_VERIFY(textArea->property("visible").toBool());
        QTRY_VERIFY(outlinedText->property("visible").toBool());
    }

    void qmlOutlinedRendererVisualSizeTracksZoom()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(4, 4, 120, 40);
        editor.updateSelectedText(QStringLiteral("Scale"));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("boxOutlinedText")));
        auto* outlinedText = qobject_cast<QQuickItem*>(object);
        QVERIFY(outlinedText);
        auto* boxTextPerspective = outlinedText->parentItem();
        QVERIFY(boxTextPerspective);

        const auto nearlyEqual = [](qreal a, qreal b) { return std::abs(a - b) <= 0.5; };
        const auto verifyGeometry = [&]() {
            QVERIFY(nearlyEqual(outlinedText->width() * outlinedText->scale(), boxTextPerspective->width()));
            QVERIFY(nearlyEqual(outlinedText->height() * outlinedText->scale(), boxTextPerspective->height()));
            QVERIFY(outlinedText->property("renderScale").toReal() <= 1.0 + 0.001);
        };

        QTRY_VERIFY(boxTextPerspective->width() > 0);
        verifyGeometry();

        QVERIFY(window->setProperty("zoom", 2.0));
        QTRY_VERIFY(nearlyEqual(outlinedText->property("renderScale").toReal(), 1.0));
        verifyGeometry();

        QVERIFY(window->setProperty("zoom", 0.5));
        QTRY_VERIFY(outlinedText->property("renderScale").toReal() < 1.0);
        verifyGeometry();
    }

    void liveInteractionDefersPreviewButRefreshesModel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        QSignalSpy changed(&editor, &EditorController::documentChanged);

        editor.beginInteraction();
        editor.moveSelected(5, 7);

        QCOMPARE(changed.count(), 1);
        editor.endInteraction();

        QCOMPARE(changed.count(), 2);
    }

    void gradientPathInteractionDoesNotGeneratePreviewArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 24, 18);
        editor.updateSelectedText(QStringLiteral("FX"));
        editor.setSelectedGradientEnabled(true);
        editor.setSelectedPathEnabled(true);
        QSignalSpy changed(&editor, &EditorController::documentChanged);

        editor.beginInteraction();
        editor.moveSelected(5, 7);

        QCOMPARE(changed.count(), 1);
        editor.endInteraction();

        QCOMPARE(changed.count(), 2);
    }

    void transformChangesPersistOnSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(10, 20, 100, 50);
        editor.moveSelected(5, 7);
        editor.resizeSelected(10, 20);
        editor.rotateSelected(15);
        editor.save();

        const auto box = readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().at(0).toObject();
        QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 15.0);
        QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 27.0);
        QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 110.0);
        QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 70.0);
        QCOMPARE(box.value(QStringLiteral("rotation_degrees")).toDouble(), 15.0);
    }

    void selectedBoundsUseTypeXMinimum()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);

        editor.setSelectedBounds(30, 40, 4, 6);

        const auto box = editor.boxes().at(0).toMap();
        QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 30.0);
        QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 40.0);
        QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 12.0);
        QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 12.0);
    }

    void qmlEditorNamedConstantsKeepLegacyValues()
    {
        const QString source = readQmlFile(QStringLiteral("Main.qml"));

        QVERIFY(source.contains(QStringLiteral("readonly property int dragModeIdle: 0")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModePan: 2")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModeCreate: 3")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModeResize: 4")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModePerspective: 5")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModeRotate: 6")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModeMove: 7")));
        QVERIFY(source.contains(QStringLiteral("readonly property int dragModePathHandle: 8")));
        QVERIFY(source.contains(QStringLiteral("readonly property real minimumBoxSize: 12")));
        QVERIFY(source.contains(QStringLiteral("readonly property int minimumFontSize: 1")));
        QVERIFY(source.contains(QStringLiteral("readonly property int maximumFontSize: 512")));
        QVERIFY(source.contains(QStringLiteral("readonly property int minimumTextSpacing: -100")));
        QVERIFY(source.contains(QStringLiteral("readonly property int maximumTextSpacing: 300")));
        QVERIFY(source.contains(QStringLiteral("readonly property int minimumEffectSize: 0")));
        QVERIFY(source.contains(QStringLiteral("readonly property int maximumEffectSize: 128")));
        QVERIFY(source.contains(QStringLiteral("readonly property int minimumShadowOffset: -512")));
        QVERIFY(source.contains(QStringLiteral("readonly property int maximumShadowOffset: 512")));
    }

    void qmlNormalResizeUsesPressSnapshotSeparateFromPerspective()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype rotateStart = source.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateStart > resizeStart);
        const QString resizeSource = source.mid(resizeStart, rotateStart - resizeStart);

        QVERIFY(!resizeSource.contains(QStringLiteral("property real startX")));
        QVERIFY(!resizeSource.contains(QStringLiteral("property real startCanvasX")));
        QVERIFY(source.contains(QStringLiteral("const delta = rotatePoint((canvasX - resizeStartCanvasX) / viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("right = Math.max(resizeStartW + delta.x, left + editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("bottom = Math.max(resizeStartH + delta.y, top + editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("left = Math.min(delta.x, right - editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("top = Math.min(delta.y, bottom - editorLimits.minimumBoxSize)")));
        const qsizetype perspectiveBranch = resizeSource.indexOf(QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
        const qsizetype normalUpdate = resizeSource.indexOf(QStringLiteral("resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)"));
        QVERIFY(perspectiveBranch >= 0);
        QVERIFY(normalUpdate > perspectiveBranch);
        QVERIFY(resizeSource.mid(perspectiveBranch, normalUpdate - perspectiveBranch).contains(QStringLiteral("return")));
        QVERIFY(source.contains(QStringLiteral("window.editor.setSelectedBounds(resizeX, resizeY, resizeW, resizeH)")));
        QVERIFY(!resizeSource.contains(QStringLiteral("startLocalX")));
        QVERIFY(!resizeSource.contains(QStringLiteral("startLocalY")));
    }

    void qmlResizeHandleKeepsStableDragStateOutsideHandle()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype rotateStart = source.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateStart > resizeStart);
        const QString resizeSource = source.mid(resizeStart, rotateStart - resizeStart);

        QVERIFY(source.contains(QStringLiteral("property var activeResizeDelegate: null")));
        QVERIFY(source.contains(QStringLiteral("property string activeResizeHandle: \"\"")));
        QVERIFY(source.contains(QStringLiteral("function beginResizeDrag(delegate, handle, canvasX, canvasY)")));
        QVERIFY(source.contains(QStringLiteral("function updateResizeDrag(canvasX, canvasY)")));
        QVERIFY(source.contains(QStringLiteral("function endResizeDrag(commit)")));
        QVERIFY(resizeSource.contains(QStringLiteral("preventStealing: true")));
        QVERIFY(resizeSource.contains(QStringLiteral("acceptedButtons: Qt.LeftButton")));
        QVERIFY(resizeSource.contains(QStringLiteral("mouse.accepted = true")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.endResizeDrag(true)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.endPerspectiveDrag(true)")));
        QVERIFY(!resizeSource.contains(QStringLiteral("drag.target")));
    }

    void qmlBoxMoveUsesTransientWindowDragAndStableEditor()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype moveStart = source.indexOf(QStringLiteral("z: boxRef.selected && editorRef.editingText ? -1 : 10"));
        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"), moveStart);
        QVERIFY(moveStart >= 0);
        QVERIFY(resizeStart > moveStart);
        const QString moveSource = source.mid(moveStart, resizeStart - moveStart);

        const qsizetype updateMoveStart = source.indexOf(QStringLiteral("function updateMoveDrag(canvasX, canvasY)"));
        const qsizetype endMoveStart = source.indexOf(QStringLiteral("function endMoveDrag(commit)"));
        const qsizetype escapeStart = source.indexOf(QStringLiteral("function handleEscape()"));
        QVERIFY(updateMoveStart >= 0);
        QVERIFY(endMoveStart > updateMoveStart);
        QVERIFY(escapeStart > endMoveStart);
        const QString updateMove = source.mid(updateMoveStart, endMoveStart - updateMoveStart);
        const QString endMove = source.mid(endMoveStart, escapeStart - endMoveStart);

        QVERIFY(source.contains(QStringLiteral("readonly property var editor: Editor")));
        QVERIFY(source.contains(QStringLiteral("property var activeMoveDelegate: null")));
        QVERIFY(source.contains(QStringLiteral("function beginMoveDrag(delegate, index, canvasX, canvasY)")));
        QVERIFY(moveSource.contains(QStringLiteral("preventStealing: true")));
        QVERIFY(moveSource.contains(QStringLiteral("mouse.accepted = true")));
        QVERIFY(source.contains(QStringLiteral("function perspectiveVisualBounds(box, width, height)")));
        QVERIFY(source.contains(QStringLiteral("function pointInPerspectivePolygon(point, box, width, height)")));
        QVERIFY(moveSource.contains(QStringLiteral("readonly property var visualBounds: boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds(boxRef.boxModel, boxRef.width, boxRef.height)")));
        QVERIFY(moveSource.contains(QStringLiteral("x: visualBounds.x")));
        QVERIFY(moveSource.contains(QStringLiteral("width: visualBounds.width")));
        QVERIFY(moveSource.contains(QStringLiteral("if (boxRef.perspectiveActive && !rootWindow.pointInPerspectivePolygon(local, boxRef.boxModel, boxRef.width, boxRef.height))")));
        QVERIFY(source.contains(QStringLiteral("readonly property var rootWindow: ApplicationWindow.window")));
        QVERIFY(source.contains(QStringLiteral("readonly property var editorRef: rootWindow ? rootWindow.editor : null")));
        QVERIFY(source.contains(QStringLiteral("property real visualDocX: moveActive ? rootWindow.moveX : resizeActive ? rootWindow.resizeX : modelData.x")));
        QVERIFY(source.contains(QStringLiteral("x: rootWindow.documentToViewX(visualDocX)")));
        QVERIFY(moveSource.contains(QStringLiteral("editorRef.selectBox(boxRef.boxModel.index)")));
        QVERIFY(moveSource.contains(QStringLiteral("editorRef.beginInteraction()")));
        QVERIFY(moveSource.contains(QStringLiteral("rootWindow.beginMoveDrag(boxRef, boxRef.boxModel.index, point.x, point.y)")));
        QVERIFY(moveSource.contains(QStringLiteral("rootWindow.updateMoveDrag(point.x, point.y)")));
        QVERIFY(moveSource.contains(QStringLiteral("rootWindow.endMoveDrag(true); editorRef.endInteraction()")));
        QVERIFY(moveSource.contains(QStringLiteral("rootWindow.endMoveDrag(false); editorRef.endInteraction()")));
        QVERIFY(!moveSource.contains(QStringLiteral("Editor.moveSelected")));
        QVERIFY(!moveSource.contains(QStringLiteral("window.editor")));
        QVERIFY(!updateMove.contains(QStringLiteral("moveSelected")));
        QVERIFY(!updateMove.contains(QStringLiteral("activeMoveDelegate.x")));
        QVERIFY(endMove.contains(QStringLiteral("window.editor.moveSelected(moveX - moveStartX, moveY - moveStartY)")));
    }

    void qmlPerspectiveMoveHitAreaFollowsVisualPolygonBelowHandles()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype boundsStart = source.indexOf(QStringLiteral("function perspectiveVisualBounds(box, width, height)"));
        const qsizetype marginStart = source.indexOf(QStringLiteral("function perspectiveMargin(box)"), boundsStart);
        QVERIFY(boundsStart >= 0);
        QVERIFY(marginStart > boundsStart);
        const QString hitHelpers = source.mid(boundsStart, marginStart - boundsStart);

        QVERIFY(hitHelpers.contains(QStringLiteral("perspectiveCorner(box, \"nw\", width, height)")));
        QVERIFY(hitHelpers.contains(QStringLiteral("Math.min(nw.x, ne.x, se.x, sw.x)")));
        QVERIFY(hitHelpers.contains(QStringLiteral("Math.max(nw.y, ne.y, se.y, sw.y)")));
        QVERIFY(hitHelpers.contains(QStringLiteral("function pointOnSegment(point, a, b)")));
        QVERIFY(hitHelpers.contains(QStringLiteral("pointOnSegment(point, a, b)")));

        const qsizetype moveStart = source.indexOf(QStringLiteral("readonly property var visualBounds: boxRef.perspectiveActive"));
        const qsizetype handlesStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"), moveStart);
        QVERIFY(moveStart >= 0);
        QVERIFY(handlesStart > moveStart);
        const QString moveSource = source.mid(moveStart, handlesStart - moveStart);

        QVERIFY(moveSource.contains(QStringLiteral("boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds")));
        QVERIFY(moveSource.contains(QStringLiteral(": ({ x: 0, y: 0, width: boxRef.width, height: boxRef.height })")));
        QVERIFY(moveSource.contains(QStringLiteral("mouse.accepted = false")));
        QVERIFY(moveSource.contains(QStringLiteral("return")));
        QVERIFY(moveSource.contains(QStringLiteral("rootWindow.beginMoveDrag(boxRef, boxRef.boxModel.index, point.x, point.y)")));
        QVERIFY(!moveSource.contains(QStringLiteral("beginResizeDrag")));
        QVERIFY(!moveSource.contains(QStringLiteral("beginPerspectiveDrag")));

        const qsizetype handleZ = source.indexOf(QStringLiteral("z: 20"), handlesStart);
        QVERIFY(handleZ > handlesStart);
    }

    void qmlNestedInteractionHandlersUseStableEditorReference()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype pathStart = source.indexOf(QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"), resizeStart);
        const qsizetype pathEnd = source.size();
        const qsizetype rotateRectStart = source.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
        const qsizetype rotateStart = source.indexOf(QStringLiteral("rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateRectStart > resizeStart);
        QVERIFY(rotateStart > resizeStart);
        QVERIFY(pathStart > rotateStart);
        QVERIFY(pathEnd > pathStart);

        const QString resizeSource = source.mid(resizeStart, rotateStart - resizeStart);
        const QString rotateSource = source.mid(rotateRectStart, pathStart - rotateRectStart);
        const QString pathSource = source.mid(pathStart, pathEnd - pathStart);
        const qsizetype boxDelegateStart = source.indexOf(QStringLiteral("Rectangle {\n    id: boxDelegate"));
        const qsizetype boxDelegateEnd = source.size();
        QVERIFY(boxDelegateStart >= 0);
        QVERIFY(boxDelegateEnd > boxDelegateStart);
        const QString boxDelegateSource = source.mid(boxDelegateStart, boxDelegateEnd - boxDelegateStart);

        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.editorRef.endInteraction()")));
        QVERIFY(rotateSource.contains(QStringLiteral("rotateHandle.editorRef.endInteraction()")));
        QVERIFY(source.contains(QStringLiteral("readonly property var editor: Editor")));
        QVERIFY(boxDelegateSource.contains(QStringLiteral("readonly property var rootWindow: ApplicationWindow.window")));
        QVERIFY(!boxDelegateSource.contains(QStringLiteral("readonly property var rootWindow: window")));
        QVERIFY(!boxDelegateSource.contains(QStringLiteral("Editor.")));
        QVERIFY(!boxDelegateSource.contains(QStringLiteral("window.")));
        QVERIFY(!boxDelegateSource.contains(QStringLiteral("property var boxRef: boxDelegate")));
        QVERIFY(resizeSource.contains(QStringLiteral("property var boxRef: parent")));
        QVERIFY(resizeSource.contains(QStringLiteral("property var rootWindow: boxRef.rootWindow")));
        QVERIFY(resizeSource.contains(QStringLiteral("property var editorRef: boxRef.editorRef")));
        QVERIFY(rotateSource.contains(QStringLiteral("property var boxRef: parent")));
        QVERIFY(pathSource.contains(QStringLiteral("property var boxRef: pathPlane.boxRef")));
        QVERIFY(pathSource.contains(QStringLiteral("property var editorRef: boxRef.editorRef")));
        QVERIFY(!resizeSource.contains(QStringLiteral("if (boxDelegate.perspectiveActive)")));
        QVERIFY(!resizeSource.contains(QStringLiteral("boxDelegate.rootWindow.endResizeDrag")));
        QVERIFY(!resizeSource.contains(QStringLiteral("boxDelegate.rootWindow.endPerspectiveDrag")));
        QVERIFY(!rotateSource.contains(QStringLiteral("boxDelegate.rootWindow.endRotateDrag")));
        QVERIFY(!resizeSource.contains(QStringLiteral("boxDelegate.")));
        QVERIFY(!rotateSource.contains(QStringLiteral("boxDelegate.")));
        QVERIFY(!pathSource.contains(QStringLiteral("boxDelegate.")));
        QVERIFY(!pathSource.contains(QStringLiteral("onReleased: boxDelegate.editorRef")));
        QVERIFY(!pathSource.contains(QStringLiteral("onCanceled: boxDelegate.editorRef")));
        QVERIFY(!resizeSource.contains(QStringLiteral("Editor.endInteraction()")));
        QVERIFY(!rotateSource.contains(QStringLiteral("Editor.endInteraction()")));
        QVERIFY(!pathSource.contains(QStringLiteral("Editor.endInteraction()")));
        QVERIFY(!resizeSource.contains(QStringLiteral("window.")));
        QVERIFY(!rotateSource.contains(QStringLiteral("window.")));
        QVERIFY(!pathSource.contains(QStringLiteral("window.")));
        QVERIFY(!source.contains(QStringLiteral("model: [{name: \"nw\"}, {name: \"n\"}")));
    }

    void qmlSelectionGeometryAndRotateHandleUseCurrentVisualBounds()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("property bool moveActive: rootWindow.dragMode === interaction.dragModeMove && rootWindow.activeMoveIndex === modelData.index")));
        QVERIFY(source.contains(QStringLiteral("property bool resizeActive: rootWindow.dragMode === interaction.dragModeResize && rootWindow.activeResizeDelegate === boxDelegate")));
        QVERIFY(source.contains(QStringLiteral("property real visualDocW: resizeActive ? rootWindow.resizeW : modelData.w")));
        QVERIFY(source.contains(QStringLiteral("width: visualDocW * rootWindow.viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("function documentToViewLength(value) { return value * viewDocScale() }")));
        QVERIFY(source.contains(QStringLiteral("function handleSize() { return Math.max(1, documentToViewLength(editorLimits.minimumBoxSize)) }")));
        QVERIFY(source.contains(QStringLiteral("border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))")));
        QVERIFY(!source.contains(QStringLiteral("activeResizeDelegate.x = documentToViewX(resizeX)")));
        QVERIFY(!source.contains(QStringLiteral("activeMoveDelegate.x = documentToViewX(moveX)")));
        QVERIFY(source.contains(QStringLiteral("function topMiddleVisualPoint(box, width, height)")));
        QVERIFY(source.contains(QStringLiteral("const nw = perspectiveCorner(box, \"nw\", width, height)")));
        QVERIFY(source.contains(QStringLiteral("const ne = perspectiveCorner(box, \"ne\", width, height)")));
        QVERIFY(source.contains(QStringLiteral("return { x: (nw.x + ne.x) / 2, y: (nw.y + ne.y) / 2 }")));
        QVERIFY(source.contains(QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height)")));
    }

    void qmlHasEightResizeHandlesAndAngleRotation()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        for (const QString& handle : {QStringLiteral("nw"), QStringLiteral("n"), QStringLiteral("ne"), QStringLiteral("e"), QStringLiteral("se"), QStringLiteral("s"), QStringLiteral("sw"), QStringLiteral("w")}) {
            QVERIFY2(source.contains(QStringLiteral("name: \"") + handle + QStringLiteral("\"")), qPrintable(handle));
        }
        QVERIFY(source.contains(QStringLiteral("Math.atan2")));
        QVERIFY(source.contains(QStringLiteral("window.editor.setSelectedBounds")));
        QVERIFY(source.contains(QStringLiteral("rotateHandle.rootWindow.endRotateDrag(true)")));
        QVERIFY(!source.contains(QStringLiteral("Editor.rotateSelected(degrees)")));
    }

    void qmlPerspectiveAndRotateDragsCommitOnlyOnRelease()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype perspectiveUpdateStart = source.indexOf(QStringLiteral("function updatePerspectiveDrag(canvasX, canvasY)"));
        const qsizetype perspectiveEndStart = source.indexOf(QStringLiteral("function endPerspectiveDrag(commit)"));
        const qsizetype rotateStart = source.indexOf(QStringLiteral("function beginRotateDrag(delegate, canvasX, canvasY)"));
        const qsizetype rotateUpdateStart = source.indexOf(QStringLiteral("function updateRotateDrag(canvasX, canvasY)"));
        const qsizetype rotateEndStart = source.indexOf(QStringLiteral("function endRotateDrag(commit)"));
        const qsizetype escapeStart = source.indexOf(QStringLiteral("function handleEscape()"));
        QVERIFY(perspectiveUpdateStart >= 0);
        QVERIFY(perspectiveEndStart > perspectiveUpdateStart);
        QVERIFY(rotateStart > perspectiveEndStart);
        QVERIFY(rotateUpdateStart > rotateStart);
        QVERIFY(rotateEndStart > rotateUpdateStart);
        QVERIFY(escapeStart > rotateEndStart);

        const QString perspectiveUpdate = source.mid(perspectiveUpdateStart, perspectiveEndStart - perspectiveUpdateStart);
        const QString perspectiveEnd = source.mid(perspectiveEndStart, rotateStart - perspectiveEndStart);
        const QString rotateUpdate = source.mid(rotateUpdateStart, rotateEndStart - rotateUpdateStart);
        const QString rotateEnd = source.mid(rotateEndStart, escapeStart - rotateEndStart);

        QVERIFY(source.contains(QStringLiteral("property var activePerspectiveDelegate: null")));
        QVERIFY(source.contains(QStringLiteral("property var activeRotateDelegate: null")));
        QVERIFY(perspectiveUpdate.contains(QStringLiteral("perspectiveX = perspectiveStartX")));
        QVERIFY(perspectiveUpdate.contains(QStringLiteral("++perspectiveRevision")));
        QVERIFY(!perspectiveUpdate.contains(QStringLiteral("Editor.setPerspectiveHandle")));
        QVERIFY(perspectiveEnd.contains(QStringLiteral("if (dragMode === editorInteraction.dragModePerspective && commit)")));
        QVERIFY(perspectiveEnd.contains(QStringLiteral("window.editor.setPerspectiveHandle(activePerspectiveHandle, perspectiveX, perspectiveY)")));
        QVERIFY(rotateUpdate.contains(QStringLiteral("activeRotateDelegate.rotation = rotateDegrees")));
        QVERIFY(!rotateUpdate.contains(QStringLiteral("Editor.setSelectedRotation")));
        QVERIFY(rotateEnd.contains(QStringLiteral("if (dragMode === editorInteraction.dragModeRotate && commit)")));
        QVERIFY(rotateEnd.contains(QStringLiteral("window.editor.setSelectedRotation(rotateDegrees)")));
    }

    void qmlPerspectiveMidpointsAreDerivedAndAxisConstrained()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype visualStart = source.indexOf(QStringLiteral("function visualHandlePosition(box, name, width, height)"));
        const qsizetype topStart = source.indexOf(QStringLiteral("function topMiddleVisualPoint"), visualStart);
        QVERIFY(visualStart >= 0);
        QVERIFY(topStart > visualStart);
        const QString visualSource = source.mid(visualStart, topStart - visualStart);

        const qsizetype updateStart = source.indexOf(QStringLiteral("function updatePerspectiveDrag(canvasX, canvasY)"));
        const qsizetype commitStart = source.indexOf(QStringLiteral("function commitPerspectiveCorner"), updateStart);
        QVERIFY(updateStart >= 0);
        QVERIFY(commitStart > updateStart);
        const QString updateSource = source.mid(updateStart, commitStart - updateStart);

        QVERIFY(visualSource.contains(QStringLiteral("name === \"n\" || name === \"e\" || name === \"s\" || name === \"w\"")));
        QVERIFY(visualSource.contains(QStringLiteral("perspectiveCorner(box, \"nw\", width, height)")));
        QVERIFY(visualSource.contains(QStringLiteral("perspectiveCorner(box, \"ne\", width, height)")));
        QVERIFY(visualSource.contains(QStringLiteral("perspectiveCorner(box, \"se\", width, height)")));
        QVERIFY(visualSource.contains(QStringLiteral("perspectiveCorner(box, \"sw\", width, height)")));
        QVERIFY(visualSource.contains(QStringLiteral("return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }")));
        QVERIFY(updateSource.contains(QStringLiteral("activePerspectiveHandle === \"n\" || activePerspectiveHandle === \"s\" ? 0 : dx")));
        QVERIFY(updateSource.contains(QStringLiteral("activePerspectiveHandle === \"e\" || activePerspectiveHandle === \"w\" ? 0 : dy")));
    }

    void qmlPerspectiveMidpointDragMovesAdjacentCornersFromSnapshots()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype beginStart = source.indexOf(QStringLiteral("function beginPerspectiveDrag(delegate, handle, canvasX, canvasY)"));
        const qsizetype updateStart = source.indexOf(QStringLiteral("function updatePerspectiveDrag(canvasX, canvasY)"), beginStart);
        const qsizetype endStart = source.indexOf(QStringLiteral("function endPerspectiveDrag(commit)"), updateStart);
        const qsizetype rotateStart = source.indexOf(QStringLiteral("function beginRotateDrag"), endStart);
        QVERIFY(beginStart >= 0);
        QVERIFY(updateStart > beginStart);
        QVERIFY(endStart > updateStart);
        QVERIFY(rotateStart > endStart);
        const QString beginSource = source.mid(beginStart, updateStart - beginStart);
        const QString endSource = source.mid(endStart, rotateStart - endStart);

        for (const QString& corner : {QStringLiteral("Nw"), QStringLiteral("Ne"), QStringLiteral("Se"), QStringLiteral("Sw")}) {
            QVERIFY2(beginSource.contains(QStringLiteral("perspectiveStart") + corner + QStringLiteral("X = modelPerspectiveOffset")), qPrintable(corner));
            QVERIFY2(beginSource.contains(QStringLiteral("perspectiveStart") + corner + QStringLiteral("Y = modelPerspectiveOffset")), qPrintable(corner));
        }
        QVERIFY(endSource.contains(QStringLiteral("activePerspectiveHandle === \"n\"")));
        QVERIFY(endSource.contains(QStringLiteral("commitPerspectiveCorner(\"nw\", perspectiveStartNwX, perspectiveStartNwY + dy)")));
        QVERIFY(endSource.contains(QStringLiteral("commitPerspectiveCorner(\"ne\", perspectiveStartNeX, perspectiveStartNeY + dy)")));
        QVERIFY(endSource.contains(QStringLiteral("commitPerspectiveCorner(\"e\"")) == false);
        QVERIFY(endSource.contains(QStringLiteral("else window.editor.setPerspectiveHandle(activePerspectiveHandle, perspectiveX, perspectiveY)")));
    }

    void qmlPerspectiveAndRotateHandlesAcceptStableMouseCapture()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype pathStart = source.indexOf(QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"), resizeStart);
        const qsizetype rotateStart = source.indexOf(QStringLiteral("rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateStart > resizeStart);
        QVERIFY(pathStart > rotateStart);

        const QString resizeSource = source.mid(resizeStart, rotateStart - resizeStart);
        const QString rotateSource = source.mid(rotateStart - 800, pathStart - rotateStart + 800);

        QVERIFY(resizeSource.contains(QStringLiteral("preventStealing: true")));
        QVERIFY(resizeSource.contains(QStringLiteral("mouse.accepted = true")));
        QVERIFY(rotateSource.contains(QStringLiteral("preventStealing: true")));
        QVERIFY(rotateSource.contains(QStringLiteral("mouse.accepted = true")));
        QVERIFY(rotateSource.contains(QStringLiteral("rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef, point.x, point.y)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.endPerspectiveDrag(true)")));
        QVERIFY(!source.contains(QStringLiteral("model: [{name: \"nw\"}, {name: \"n\"}")));
    }

    void documentCoordinatesPersistAsCreated()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(12.5, 34.25, 123.75, 56.5);
        editor.moveSelected(3.5, -4.25);
        editor.resizeSelected(10.25, 20.5);
        editor.save();

        const auto box = readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().at(0).toObject();
        QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 16.0);
        QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 30.0);
        QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 134.0);
        QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 77.0);
    }

    void multilineEditUpdatesDocumentState()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(1, 2, 80, 40);
        editor.beginTextEdit();
        QVERIFY(editor.editingText());
        editor.updateSelectedText(QStringLiteral("Line 1\nLine 2"));
        editor.endTextEdit();

        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Line 1\nLine 2"));
        QVERIFY(!editor.editingText());
    }

    void selectedBoxCanBeDeselectedWithoutDeletion()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(1, 2, 80, 40);

        editor.selectBox(-1);

        QCOMPARE(editor.selectedIndex(), -1);
        QCOMPARE(editor.boxes().size(), 1);
    }

    void textPropertiesUpdateSelectedBox()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(1, 2, 80, 40);

        editor.setSelectedFontFamily(QStringLiteral("TextFX Custom Missing Family"));
        editor.setSelectedFontSize(42);
        editor.setSelectedTextColor(QStringLiteral("#ff00aa"));
        editor.setSelectedBold(true);
        editor.setSelectedItalic(true);
        editor.setSelectedUppercase(true);
        editor.setSelectedAlignment(1);
        editor.setSelectedLineSpacing(12);
        editor.setSelectedLetterSpacing(3);

        const auto box = editor.boxes().at(0).toMap();
        QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), QStringLiteral("TextFX Custom Missing Family"));
        QVERIFY(box.contains(QStringLiteral("resolvedFontFamily")));
        QVERIFY(!box.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty());
        QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() != box.value(QStringLiteral("fontFamily")).toString());
        QCOMPARE(box.value(QStringLiteral("fontSize")).toInt(), 42);
        QCOMPARE(box.value(QStringLiteral("color")).toString(), QStringLiteral("ff00aaff"));
        QVERIFY(box.value(QStringLiteral("bold")).toBool());
        QVERIFY(box.value(QStringLiteral("italic")).toBool());
        QVERIFY(box.value(QStringLiteral("uppercase")).toBool());
        QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 1);
        QCOMPARE(box.value(QStringLiteral("lineSpacing")).toInt(), 12);
        QCOMPARE(box.value(QStringLiteral("letterSpacing")).toInt(), 3);
        QVERIFY(editor.dirty());
    }

    void fontResolverUsesQtFamiliesAndDeterministicFallback()
    {
        QFont missing(QStringLiteral("TextFX Definitely Missing Font 9B33F761"));
        missing.setPixelSize(17);
        missing.setBold(true);

        const auto fallback = resolveFont(missing);
        QVERIFY(fallback.fellBack);
        QVERIFY(!fallback.font.family().isEmpty());
        QCOMPARE(fallback.font.pixelSize(), 17);
        QCOMPARE(fallback.font.bold(), true);
        QCOMPARE(resolveFont(missing).font.family(), fallback.font.family());
        QCOMPARE(missing.family(), QStringLiteral("TextFX Definitely Missing Font 9B33F761"));

        const QStringList families = QFontDatabase::families();
        QVERIFY(!families.isEmpty());
        QFont known(families.first());
        const auto resolved = resolveFont(known);
        QVERIFY(!resolved.fellBack);
        QVERIFY(resolved.requestedAvailable);
        QVERIFY(!QFontInfo(resolved.font).family().isEmpty());

        const QString beatDown = QStringLiteral("000 BeatDownBB [TeddyBear]");
        const bool beatDownAvailable = families.contains(beatDown) || families.contains(QStringLiteral("000 BeatDownBB"));
        if (beatDownAvailable) {
            const auto beatDownResolved = resolveFont(QFont(beatDown));
            QVERIFY(!beatDownResolved.fellBack);
            QVERIFY(beatDownResolved.requestedAvailable);
        }
    }

    void sfntFullAndTypographicNamesMapToUsableFamily()
    {
        const QStringList names = detail::fontNamesFromSfntData(syntheticSfntNameTable());
        QVERIFY(names.contains(QStringLiteral("TextFX Alias Test")));
        QVERIFY(names.contains(QStringLiteral("000 TextFX Alias Test [Display]")));
        QVERIFY(names.contains(QStringLiteral("TextFX Usable Family")));
        QVERIFY(!names.contains(QStringLiteral("Ignored Style Name")));

        const QHash<QString, QString> aliases = detail::aliasesForSfntNames(names, QStringLiteral("TextFX Usable Family"));
        QCOMPARE(aliases.value(QStringLiteral("000 TextFX Alias Test [Display]").toCaseFolded()), QStringLiteral("TextFX Usable Family"));
        QCOMPARE(aliases.value(QStringLiteral("textfx alias test")), QStringLiteral("TextFX Usable Family"));
        QCOMPARE(aliases.value(QStringLiteral("TextFX Usable Family").toCaseFolded()), QStringLiteral("TextFX Usable Family"));
    }

    void qmlTextStyleControlsUseNerdFontIconButtons()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        const qsizetype buttonStart = source.indexOf(QStringLiteral("component TextStyleButton: Button"));
        const qsizetype buttonEnd = source.indexOf(QStringLiteral("component ShortcutMenuItem: MenuItem"));
        QVERIFY(buttonStart >= 0);
        QVERIFY(buttonEnd > buttonStart);
        const QString buttonSource = source.mid(buttonStart, buttonEnd - buttonStart);

        QVERIFY(source.contains(QStringLiteral("component TextStyleButton: Button")));
        QVERIFY(buttonSource.contains(QStringLiteral("checkable: true")));
        QVERIFY(buttonSource.contains(QStringLiteral("property int buttonSize: 24")));
        QVERIFY(buttonSource.contains(QStringLiteral("width: buttonSize")));
        QVERIFY(buttonSource.contains(QStringLiteral("height: buttonSize")));
        QVERIFY(buttonSource.contains(QStringLiteral("implicitWidth: buttonSize")));
        QVERIFY(buttonSource.contains(QStringLiteral("implicitHeight: buttonSize")));
        QVERIFY(buttonSource.contains(QStringLiteral("Layout.preferredWidth: buttonSize")));
        QVERIFY(buttonSource.contains(QStringLiteral("Layout.preferredHeight: buttonSize")));
        QVERIFY(!buttonSource.contains(QStringLiteral("Layout.preferredWidth: 32")));
        QVERIFY(!buttonSource.contains(QStringLiteral("Layout.preferredWidth: 40")));
        QVERIFY(source.contains(QStringLiteral("font.family: \"Symbols Nerd Font\"")));
        QVERIFY(source.contains(QStringLiteral("Accessible.name: accessibleLabel")));
        QVERIFY(source.contains(QStringLiteral("ToolTip.text: accessibleLabel")));
        const qsizetype propertiesStart = source.indexOf(QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: true; enabled: textPropertiesSection.sectionReady }"));
        const qsizetype presetsStart = source.indexOf(QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; enabled: textPresetsSection.sectionReady }"));
        QVERIFY(propertiesStart >= 0);
        QVERIFY(presetsStart > propertiesStart);
        const QString propertiesSource = source.mid(propertiesStart, presetsStart - propertiesStart);

        QVERIFY(propertiesSource.contains(QStringLiteral("GroupBox")));
        QVERIFY(propertiesSource.contains(QStringLiteral("ColumnLayout {")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("GridLayout")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("columns: 2")));
        const qsizetype fontLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Font Family\") }"));
        const qsizetype fontCombo = propertiesSource.indexOf(QStringLiteral("ComboBox {"), fontLabel);
        const qsizetype fontSizeLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Size\") }"));
        const qsizetype fontSizeSpin = propertiesSource.indexOf(QStringLiteral("SpinBox {"), fontSizeLabel);
        const qsizetype leadingLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Leading\") }"));
        const qsizetype leadingSpin = propertiesSource.indexOf(QStringLiteral("SpinBox {"), leadingLabel);
        const qsizetype letterSpacingLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Tracking\") }"));
        const qsizetype letterSpacingSpin = propertiesSource.indexOf(QStringLiteral("SpinBox {"), letterSpacingLabel);
        const qsizetype colorLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Text Color\") }"));
        const qsizetype colorButton = propertiesSource.indexOf(QStringLiteral("ColorButton {"), colorLabel);
        QVERIFY(fontLabel >= 0);
        QVERIFY(fontCombo > fontLabel);
        QVERIFY(fontSizeLabel > fontCombo);
        QVERIFY(fontSizeSpin > fontSizeLabel);
        QVERIFY(leadingLabel > fontSizeLabel);
        QVERIFY(leadingSpin > leadingLabel);
        QVERIFY(letterSpacingLabel > leadingSpin);
        QVERIFY(letterSpacingSpin > letterSpacingLabel);
        QVERIFY(colorLabel > letterSpacingSpin);
        QVERIFY(colorButton > colorLabel);
        QVERIFY(propertiesSource.contains(QStringLiteral("model: window.fontFamilyOptions(window.selectedBox() ? window.selectedBox().fontFamily : \"\")")));
        QVERIFY(propertiesSource.contains(QStringLiteral("ComboBox {\n                                            id: fontFamilyCombo\n                                            Layout.fillWidth: true\n                                            Layout.minimumWidth: 0\n                                            model: window.fontFamilyOptions")));
        const qsizetype fontComboEnd = propertiesSource.indexOf(QStringLiteral("onActivated: Editor.setSelectedFontFamily(currentText)"), fontCombo);
        QVERIFY(fontComboEnd > fontCombo);
        const QString fontComboSource = propertiesSource.mid(fontCombo, fontComboEnd - fontCombo);
        QVERIFY(fontComboSource.contains(QStringLiteral("contentItem: Label {")));
        QVERIFY(fontComboSource.contains(QStringLiteral("text: fontFamilyCombo.displayText")));
        QVERIFY(fontComboSource.contains(QStringLiteral("font: fontFamilyCombo.font")));
        QVERIFY(fontComboSource.contains(QStringLiteral("enabled: fontFamilyCombo.enabled")));
        QVERIFY(fontComboSource.contains(QStringLiteral("elide: Text.ElideRight")));
        QVERIFY(fontComboSource.contains(QStringLiteral("verticalAlignment: Text.AlignVCenter")));
        QVERIFY(propertiesSource.contains(QStringLiteral("onActivated: Editor.setSelectedFontFamily(currentText)")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("onEditingFinished: Editor.setSelectedFontFamily")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("placeholderText: qsTr(\"Font\")")));
        QVERIFY(propertiesSource.contains(QStringLiteral("Label { text: qsTr(\"Style\") }")));
        QVERIFY(propertiesSource.contains(QStringLiteral("Flow {")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0264); accessibleLabel: qsTr(\"Bold\"); checked: window.selectedBox() ? window.selectedBox().bold : false; onClicked: Editor.setSelectedBold(checked)")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0277); accessibleLabel: qsTr(\"Italic\"); checked: window.selectedBox() ? window.selectedBox().italic : false; onClicked: Editor.setSelectedItalic(checked)")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0b36); accessibleLabel: qsTr(\"Uppercase\"); checked: window.selectedBox() ? window.selectedBox().uppercase : false; onClicked: Editor.setSelectedUppercase(checked)")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("Label { text: qsTr(\"Bold\") }")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("Label { text: qsTr(\"Italic\") }")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("Label { text: qsTr(\"Uppercase\") }")));
        QVERIFY(propertiesSource.contains(QStringLiteral("Label { text: qsTr(\"Paragraph\") }")));
        QVERIFY(!propertiesSource.contains(QStringLiteral("Layout.columnSpan: 2")));
        const qsizetype styleLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Style\") }"));
        const qsizetype styleFlow = propertiesSource.indexOf(QStringLiteral("Flow {"), styleLabel);
        const qsizetype alignmentLabel = propertiesSource.indexOf(QStringLiteral("Label { text: qsTr(\"Paragraph\") }"));
        const qsizetype alignmentFlow = propertiesSource.indexOf(QStringLiteral("Flow {"), alignmentLabel);
        QVERIFY(styleLabel >= 0);
        QVERIFY(styleFlow > styleLabel);
        QVERIFY(alignmentLabel > styleFlow);
        QVERIFY(alignmentFlow > alignmentLabel);
        QVERIFY(!propertiesSource.contains(QStringLiteral("model: [qsTr(\"Left\"), qsTr(\"Center\"), qsTr(\"Right\")]")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0262); accessibleLabel: qsTr(\"Align Left\"); checked: window.selectedBox() ? window.selectedBox().alignment === 0 : false; onClicked: Editor.setSelectedAlignment(0)")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0260); accessibleLabel: qsTr(\"Align Center\"); checked: window.selectedBox() ? window.selectedBox().alignment === 1 : false; onClicked: Editor.setSelectedAlignment(1)")));
        QVERIFY(propertiesSource.contains(QStringLiteral("text: String.fromCodePoint(0xf0263); accessibleLabel: qsTr(\"Align Right\"); checked: window.selectedBox() ? window.selectedBox().alignment === 2 : false; onClicked: Editor.setSelectedAlignment(2)")));
        QVERIFY(!source.contains(QStringLiteral("CheckBox { checked: window.selectedBox() ? window.selectedBox().bold")));
        QVERIFY(!source.contains(QStringLiteral("CheckBox { checked: window.selectedBox() ? window.selectedBox().italic")));
        QVERIFY(!source.contains(QStringLiteral("CheckBox { checked: window.selectedBox() ? window.selectedBox().uppercase")));
        QVERIFY(!buttonSource.contains(QStringLiteral("background:")));
        QVERIFY(!buttonSource.contains(QStringLiteral("contentItem:")));
        QVERIFY(!buttonSource.contains(QStringLiteral("indicator:")));
        QVERIFY(!buttonSource.contains(QStringLiteral("color:")));
        QVERIFY(!buttonSource.contains(QStringLiteral("border.")));
        QVERIFY(!buttonSource.contains(QStringLiteral("palette.")));
    }

    void qmlTextPresetsUseComboBoxSelector()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        const qsizetype propertiesStart = source.indexOf(QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: true; enabled: textPropertiesSection.sectionReady }"));
        const qsizetype presetsStart = source.indexOf(QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; enabled: textPresetsSection.sectionReady }"));
        const qsizetype pageTextsStart = source.indexOf(QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; enabled: pageTextsSection.sectionReady }"));
        QVERIFY(propertiesStart >= 0);
        QVERIFY(presetsStart >= 0);
        QVERIFY(pageTextsStart > presetsStart);
        const QString propertiesSource = source.mid(propertiesStart, presetsStart - propertiesStart);
        const QString presetsSource = source.mid(presetsStart, pageTextsStart - presetsStart);

        QVERIFY(propertiesSource.contains(QStringLiteral("GroupBox {")));
        QVERIFY(source.contains(QStringLiteral("id: textPropertiesSection")));
        QVERIFY(source.contains(QStringLiteral("readonly property bool sectionReady: Editor.selectedIndex >= 0")));
        QVERIFY(propertiesSource.contains(QStringLiteral("enabled: textPropertiesSection.sectionReady")));
        QVERIFY(presetsSource.contains(QStringLiteral("GroupBox {")));
        QVERIFY(source.contains(QStringLiteral("id: textPresetsSection")));
        QVERIFY(source.contains(QStringLiteral("readonly property bool sectionReady: Editor.hasProject && Editor.selectedIndex >= 0")));
        QVERIFY(presetsSource.contains(QStringLiteral("enabled: textPresetsSection.sectionReady")));
        QVERIFY(presetsSource.contains(QStringLiteral("TextField { id: presetNameField")));
        QVERIFY(presetsSource.contains(QStringLiteral("ComboBox {")));
        QVERIFY(presetsSource.contains(QStringLiteral("id: presetSelect")));
        QVERIFY(presetsSource.contains(QStringLiteral("id: presetSelect\n                                                Layout.fillWidth: true\n                                                Layout.minimumWidth: 0")));
        QVERIFY(presetsSource.contains(QStringLiteral("model: Editor.presets")));
        QVERIFY(presetsSource.contains(QStringLiteral("textRole: \"name\"")));
        QVERIFY(presetsSource.contains(QStringLiteral("currentIndex: Editor.selectedPresetIndex >= 0 ? Editor.selectedPresetIndex : (Editor.presets.length > 0 ? 0 : -1)")));
        const qsizetype presetComboEnd = presetsSource.indexOf(QStringLiteral("onActivated: index => Editor.selectPreset(index)"));
        QVERIFY(presetComboEnd > 0);
        const QString presetComboSource = presetsSource.mid(presetsSource.indexOf(QStringLiteral("id: presetSelect")), presetComboEnd);
        QVERIFY(presetComboSource.contains(QStringLiteral("contentItem: Label {")));
        QVERIFY(presetComboSource.contains(QStringLiteral("text: presetSelect.displayText")));
        QVERIFY(presetComboSource.contains(QStringLiteral("font: presetSelect.font")));
        QVERIFY(presetComboSource.contains(QStringLiteral("enabled: presetSelect.enabled")));
        QVERIFY(presetComboSource.contains(QStringLiteral("elide: Text.ElideRight")));
        QVERIFY(presetComboSource.contains(QStringLiteral("verticalAlignment: Text.AlignVCenter")));
        QVERIFY(presetsSource.contains(QStringLiteral("Button { text: qsTr(\"Apply\"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0; onClicked: Editor.applySelectedPreset() }")));
        QVERIFY(presetsSource.contains(QStringLiteral("Button { text: qsTr(\"Rename\"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0")));
        QVERIFY(presetsSource.contains(QStringLiteral("Button { text: qsTr(\"Delete\"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0")));
        QVERIFY(presetsSource.contains(QStringLiteral("onActivated: index => Editor.selectPreset(index)")));
        QVERIFY(!presetsSource.contains(QStringLiteral("ListView")));
        QVERIFY(presetsSource.contains(QStringLiteral("Editor.applySelectedPreset()")));
        QVERIFY(presetsSource.contains(QStringLiteral("text: qsTr(\"Apply\")")));
        QVERIFY(presetsSource.contains(QStringLiteral("Flow {")));
        QVERIFY(presetsSource.contains(QStringLiteral("Editor.addPreset(presetNameField.text)")));
        QVERIFY(presetsSource.contains(QStringLiteral("text: qsTr(\"Create\")")));
        QVERIFY(presetsSource.contains(QStringLiteral("Editor.updateSelectedPreset()")));
        QVERIFY(presetsSource.contains(QStringLiteral("Editor.renameSelectedPreset(presetNameField.text)")));
        QVERIFY(presetsSource.contains(QStringLiteral("Editor.deleteSelectedPreset()")));
        const qsizetype crudFlowStart = presetsSource.indexOf(QStringLiteral("Flow {"));
        const qsizetype createButton = presetsSource.indexOf(QStringLiteral("text: qsTr(\"Create\")"), crudFlowStart);
        const qsizetype updateButton = presetsSource.indexOf(QStringLiteral("text: qsTr(\"Update\")"), createButton);
        const qsizetype renameButton = presetsSource.indexOf(QStringLiteral("text: qsTr(\"Rename\")"), updateButton);
        const qsizetype deleteButton = presetsSource.indexOf(QStringLiteral("text: qsTr(\"Delete\")"), renameButton);
        QVERIFY(crudFlowStart >= 0);
        QVERIFY(createButton > crudFlowStart);
        QVERIFY(updateButton > createButton);
        QVERIFY(renameButton > updateButton);
        QVERIFY(deleteButton > renameButton);

        const qsizetype nextSection = source.indexOf(QStringLiteral("id: canvas"), pageTextsStart);
        QVERIFY(nextSection > pageTextsStart);
        const QString pageTextsSource = source.mid(pageTextsStart, nextSection - pageTextsStart);
        QVERIFY(source.contains(QStringLiteral("id: pageTextsSection")));
        QVERIFY(source.contains(QStringLiteral("readonly property bool sectionReady: Editor.hasProject && Editor.pageTexts.length > 0")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; enabled: pageTextsSection.sectionReady }")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("Frame {")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("id: pageTextsFrame")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("background: Rectangle { color: pageTextsFrame.palette.base; border.color: pageTextsFrame.palette.mid; border.width: 1 }")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("horizontalAlignment: Text.AlignRight; text: Editor.pageTexts.length; enabled: pageTextsSection.sectionReady")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("enabled: pageTextsSection.sectionReady")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("ListView {")));
        QVERIFY(!pageTextsSource.contains(QStringLiteral("texts.txt")));
        const qsizetype headerCount = pageTextsSource.indexOf(QStringLiteral("horizontalAlignment: Text.AlignRight; text: Editor.pageTexts.length; enabled: pageTextsSection.sectionReady"));
        const qsizetype pageTextsBox = pageTextsSource.indexOf(QStringLiteral("Frame {"), headerCount);
        const qsizetype listView = pageTextsSource.indexOf(QStringLiteral("ListView {"), pageTextsBox);
        QVERIFY(pageTextsBox > headerCount);
        QVERIFY(listView > pageTextsBox);
        QVERIFY(listView > headerCount);
        QVERIFY(!pageTextsSource.mid(pageTextsBox, listView - pageTextsBox).contains(QStringLiteral("Page Texts")));
        QVERIFY(!pageTextsSource.mid(pageTextsBox, listView - pageTextsBox).contains(QStringLiteral("Editor.pageTexts.length")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("Layout.fillHeight: true")));
        QVERIFY(!pageTextsSource.contains(QStringLiteral("Layout.minimumHeight: 160")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("readonly property real minimumListHeight: 200")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("Layout.minimumHeight: minimumListHeight + topPadding + bottomPadding")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("Layout.preferredHeight: pageTextsFrame.minimumListHeight")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("model: Editor.pageTexts")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("delegate: ItemDelegate {")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("hoverEnabled: true")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("background: Rectangle { color: pageTextDelegate.down || pageTextDelegate.hovered ? Qt.alpha(pageTextDelegate.palette.highlight")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("contentItem: Label {")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("elide: Text.ElideRight")));
        QVERIFY(pageTextsSource.contains(QStringLiteral("onClicked: Editor.applyPageText(index)")));
    }

    void effectControlsUpdateSelectedBox()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(1, 2, 80, 40);

        editor.setSelectedRotation(25.0);
        editor.rotateSelected(5.0);
        editor.setSelectedOutlineEnabled(true);
        editor.setSelectedOutlineColor(QStringLiteral("#112233"));
        editor.setSelectedOutlineSize(7);
        editor.setSelectedBlurEnabled(true);
        editor.setSelectedBlurSize(8);
        editor.setSelectedShadowEnabled(true);
        editor.setSelectedShadowColor(QStringLiteral("#445566"));
        editor.setSelectedShadowOffsetX(-9);
        editor.setSelectedShadowOffsetY(10);
        editor.setSelectedShadowBlurSize(11);
        editor.setSelectedGradientEnabled(true);
        editor.setSelectedGradientDirection(1);
        editor.setSelectedGradientColorA(QStringLiteral("#abcdef"));
        editor.setSelectedGradientColorB(QStringLiteral("#123456"));
        editor.setSelectedPerspectiveEnabled(true);
        editor.setPerspectiveHandle(QStringLiteral("ne"), 18.0, -2.0);
        editor.setPerspectiveHandle(QStringLiteral("se"), 7.0, 9.0);
        editor.setPerspectiveHandle(QStringLiteral("sw"), 1.0, 8.0);
        editor.setPerspectiveHandle(QStringLiteral("n"), 0.0, 6.0);
        editor.setPerspectiveHandle(QStringLiteral("e"), 22.0, 0.0);
        editor.setSelectedPathEnabled(true);
        editor.setSelectedPathMode(1);
        editor.addSelectedPathPoint();

        const auto box = editor.boxes().at(0).toMap();
        QCOMPARE(box.value(QStringLiteral("rotation")).toDouble(), 30.0);
        QVERIFY(box.value(QStringLiteral("outline")).toBool());
        QCOMPARE(box.value(QStringLiteral("outlineColor")).toString(), QStringLiteral("112233ff"));
        QCOMPARE(box.value(QStringLiteral("outlineSize")).toInt(), 7);
        QVERIFY(box.value(QStringLiteral("blur")).toBool());
        QCOMPARE(box.value(QStringLiteral("blurSize")).toInt(), 8);
        QVERIFY(box.value(QStringLiteral("shadow")).toBool());
        QCOMPARE(box.value(QStringLiteral("shadowColor")).toString(), QStringLiteral("445566ff"));
        QCOMPARE(box.value(QStringLiteral("shadowOffsetX")).toInt(), -9);
        QCOMPARE(box.value(QStringLiteral("shadowOffsetY")).toInt(), 10);
        QCOMPARE(box.value(QStringLiteral("shadowBlurSize")).toInt(), 11);
        QVERIFY(box.value(QStringLiteral("gradient")).toBool());
        QCOMPARE(box.value(QStringLiteral("gradientDirection")).toInt(), 1);
        QCOMPARE(box.value(QStringLiteral("gradientColorA")).toString(), QStringLiteral("abcdefff"));
        QCOMPARE(box.value(QStringLiteral("gradientColorB")).toString(), QStringLiteral("123456ff"));
        QVERIFY(box.value(QStringLiteral("perspective")).toBool());
        QCOMPARE(box.value(QStringLiteral("perspectiveNe")).toList().at(0).toDouble(), 22.0);
        QCOMPARE(box.value(QStringLiteral("perspectiveNe")).toList().at(1).toDouble(), 6.0);
        QCOMPARE(box.value(QStringLiteral("perspectiveNw")).toList().at(1).toDouble(), 6.0);
        QCOMPARE(box.value(QStringLiteral("perspectiveSe")).toList().at(0).toDouble(), 22.0);
        QCOMPARE(box.value(QStringLiteral("perspectiveSe")).toList().at(1).toDouble(), 9.0);
        QCOMPARE(box.value(QStringLiteral("perspectiveSw")).toList().at(0).toDouble(), 1.0);
        QVERIFY(box.value(QStringLiteral("path")).toBool());
        QCOMPARE(box.value(QStringLiteral("pathMode")).toInt(), 1);
        QCOMPARE(box.value(QStringLiteral("pathPoints")).toList().size(), 4);
        editor.setPathHandle(0, -0.3, 1.4);
        const auto clampedPath = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().at(0).toList();
        QCOMPARE(clampedPath.at(0).toDouble(), 0.0);
        QCOMPARE(clampedPath.at(1).toDouble(), 1.0);
    }

    void pathPointsInsertIntoLongestSegmentInOrder()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(0, 0, 100, 40);
        editor.setSelectedPathEnabled(true);

        editor.addSelectedPathPoint();
        auto points = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QCOMPARE(points.size(), 4);
        QCOMPARE(points.at(1).toList().at(0).toDouble(), 0.25);
        QCOMPARE(points.at(1).toList().at(1).toDouble(), 0.5);

        editor.addSelectedPathPoint();
        points = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QCOMPARE(points.size(), 5);
        QCOMPARE(points.at(3).toList().at(0).toDouble(), 0.75);
        QCOMPARE(points.at(3).toList().at(1).toDouble(), 0.5);
    }

    void straightPathHandleMovementMatchesTypeXFreeDrag()
    {
        EditorController editor;
        editor.newDocument();
        editor.createTextBox(0, 0, 100, 40);
        editor.setSelectedPathEnabled(true);
        editor.setSelectedPathMode(0);
        editor.setPathHandle(1, 0.25, 0.9);

        auto point = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().at(1).toList();
        QCOMPARE(point.at(0).toDouble(), 0.25);
        QCOMPARE(point.at(1).toDouble(), 0.9);

        editor.setPathHandle(0, 0.8, 0.2);
        point = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().at(0).toList();
        QCOMPARE(point.at(0).toDouble(), 0.8);
        QCOMPARE(point.at(1).toDouble(), 0.2);

        editor.setSelectedPathMode(1);
        editor.setPathHandle(1, 0.25, 0.9);
        point = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().at(1).toList();
        QCOMPARE(point.at(0).toDouble(), 0.25);
        QCOMPARE(point.at(1).toDouble(), 0.9);
    }

    void effectsPersistAsTypeXFields()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        editor.setSelectedUppercase(true);
        editor.setSelectedAlignment(2);
        editor.setSelectedLineSpacing(9);
        editor.setSelectedLetterSpacing(4);
        editor.setSelectedShadowBlurSize(12);
        editor.setSelectedGradientEnabled(true);
        editor.setSelectedGradientDirection(1);
        editor.setSelectedGradientColorA(QStringLiteral("#ff0000"));
        editor.setSelectedGradientColorB(QStringLiteral("#0000ff"));
        editor.setPerspectiveHandle(QStringLiteral("nw"), 10.0, 20.0);
        editor.setSelectedPathEnabled(true);
        editor.setSelectedPathMode(1);
        editor.save();

        const auto box = readJson(dir.filePath(QStringLiteral(".typex/page1.json"))).value(QStringLiteral("boxes")).toArray().at(0).toObject();
        QVERIFY(box.value(QStringLiteral("uppercase")).toBool());
        QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 2);
        QCOMPARE(box.value(QStringLiteral("line_spacing")).toInt(), 9);
        QCOMPARE(box.value(QStringLiteral("letter_spacing")).toInt(), 4);
        QCOMPARE(box.value(QStringLiteral("shadow_blur_size")).toInt(), 12);
        QCOMPARE(box.value(QStringLiteral("gradient_direction")).toInt(), 1);
        QCOMPARE(box.value(QStringLiteral("gradient_color_a")).toString(), QStringLiteral("ff0000ff"));
        QCOMPARE(box.value(QStringLiteral("gradient_color_b")).toString(), QStringLiteral("0000ffff"));
        QVERIFY(box.value(QStringLiteral("perspective_offsets")).toObject().contains(QStringLiteral("nw")));
        QCOMPARE(box.value(QStringLiteral("perspective_offsets")).toObject().value(QStringLiteral("nw")).toArray().at(0).toDouble(), 10.0);
        QCOMPARE(box.value(QStringLiteral("perspective_offsets")).toObject().value(QStringLiteral("nw")).toArray().at(1).toDouble(), 20.0);
        QCOMPARE(box.value(QStringLiteral("path_points")).toArray().size(), 3);
        QCOMPARE(box.value(QStringLiteral("path_mode")).toInt(), 1);
    }

    void pageTextsLoadAndApplyToSelectedBox()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));
        QFile texts(dir.filePath(QStringLiteral("texts.txt")));
        QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
        texts.write("[page1.png]\nFirst\nSecond\n[page2.png]\nOther\n");
        texts.close();

        EditorController editor;
        editor.openProject(dir.path());
        QCOMPARE(editor.pageTexts(), QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
        QVERIFY(!editor.applyNextPageText());
        editor.createTextBox(1, 2, 80, 40);

        QVERIFY(editor.applyNextPageText());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("First"));
        QVERIFY(editor.applyNextPageText());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Second"));
        QVERIFY(!editor.applyNextPageText());
        editor.nextPage();
        QCOMPARE(editor.pageTexts(), QStringList({QStringLiteral("Other")}));
    }

    void textPresetsApplyAddUpdateRenameAndDelete()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        QVERIFY(editor.presets().size() >= 17);
        QCOMPARE(editor.selectedPresetIndex(), 0);
        editor.createTextBox(1, 2, 80, 40);

        QVERIFY(editor.applySelectedPreset());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("fontFamily")).toString(), QStringLiteral("Back Issues BB"));

        editor.setSelectedFontFamily(QStringLiteral("Inter"));
        editor.setSelectedFontSize(31);
        editor.setSelectedBold(true);
        QVERIFY(editor.addPreset(QStringLiteral("Narration")));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".typex/presets.json"))));
        QCOMPARE(editor.presets().last().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Narration"));

        editor.setSelectedFontSize(44);
        QVERIFY(editor.updateSelectedPreset());
        QCOMPARE(editor.presets().last().toMap().value(QStringLiteral("fontSize")).toInt(), 44);

        QVERIFY(editor.renameSelectedPreset(QStringLiteral("Caption")));
        QCOMPARE(editor.presets().last().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Caption"));
        QVERIFY(editor.deleteSelectedPreset());
        QVERIFY(editor.presets().last().toMap().value(QStringLiteral("name")).toString() != QStringLiteral("Caption"));
    }

    void defaultPresetUpdateCreatesOverrideButCannotRenameOrDelete()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(1, 2, 80, 40);
        editor.selectPreset(0);

        QVERIFY(!editor.renameSelectedPreset(QStringLiteral("Renamed default")));
        QVERIFY(!editor.deleteSelectedPreset());
        editor.setSelectedFontFamily(QStringLiteral("Custom Bubble"));
        QVERIFY(editor.updateSelectedPreset());

        QCOMPARE(editor.presets().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Globo normal"));
        QCOMPARE(editor.presets().first().toMap().value(QStringLiteral("fontFamily")).toString(), QStringLiteral("Custom Bubble"));
        const auto saved = readJson(dir.filePath(QStringLiteral(".typex/presets.json"))).value(QStringLiteral("presets")).toArray();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("Globo normal"));
    }

    void qmlCtrlSpaceIgnoresFocusedSidePanelTextInputs()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("sidePanelFocusedTextInputs")));
        QVERIFY(source.contains(QStringLiteral("enabled: !Editor.editingText && window.sidePanelFocusedTextInputs === 0")));
        QVERIFY(source.contains(QStringLiteral("onActiveFocusChanged: window.noteSidePanelTextInputFocus(activeFocus)")));
    }

    void qmlEscapeCancelsCurrentContextInOrder()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype handlerStart = source.indexOf(QStringLiteral("function handleEscape()"));
        const qsizetype handlerEnd = source.indexOf(QStringLiteral("Connections {"), handlerStart);
        QVERIFY(handlerStart >= 0);
        QVERIFY(handlerEnd > handlerStart);
        const QString handler = source.mid(handlerStart, handlerEnd - handlerStart);

        const qsizetype edit = handler.indexOf(QStringLiteral("if (Editor.editingText)"));
        const qsizetype transient = handler.indexOf(QStringLiteral("else if (dragMode !== editorInteraction.dragModeIdle)"));
        const qsizetype deselect = handler.indexOf(QStringLiteral("else if (Editor.selectedIndex >= 0)"));
        QVERIFY(edit >= 0);
        QVERIFY(transient > edit);
        QVERIFY(deselect > transient);
        QVERIFY(handler.contains(QStringLiteral("Editor.endTextEdit()")));
        QVERIFY(handler.contains(QStringLiteral("canvas.forceActiveFocus()")));
        QVERIFY(handler.contains(QStringLiteral("endResizeDrag(false)")));
        QVERIFY(handler.contains(QStringLiteral("dragMode = editorInteraction.dragModeIdle")));
        QVERIFY(handler.contains(QStringLiteral("Editor.selectBox(-1)")));
        QVERIFY(source.contains(QStringLiteral("Shortcut { sequence: \"Esc\"; context: Qt.ApplicationShortcut; onActivated: window.handleEscape() }")));
        QVERIFY(source.contains(QStringLiteral("if (event.key === Qt.Key_Escape) { window.handleEscape(); event.accepted = true; return }")));
        QVERIFY(source.contains(QStringLiteral("if (event.key === Qt.Key_Escape) { rootWindow.handleEscape(); event.accepted = true }")));
        QVERIFY(!handler.contains(QStringLiteral("Editor.deleteSelected")));
    }

    void qmlColorControlsUseNativeColorDialog()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("import QtQuick.Dialogs")));
        QVERIFY(source.contains(QStringLiteral("ColorDialog")));
        QVERIFY(source.contains(QStringLiteral("color: colorButton.enabled ? colorButton.swatchColor : colorButton.palette.mid")));
        QVERIFY(source.contains(QStringLiteral("Label { text: colorButton.swatchText; enabled: colorButton.enabled; Layout.fillWidth: true }")));
        QVERIFY(source.contains(QStringLiteral("selectedColor")));
        QVERIFY(source.contains(QStringLiteral("dialogColorHex(selectedColor)")));
        QVERIFY(source.contains(QStringLiteral("Editor.setSelectedTextColor(hex)")));
        QVERIFY(source.contains(QStringLiteral("Editor.setSelectedOutlineColor(hex)")));
        QVERIFY(source.contains(QStringLiteral("Editor.setSelectedShadowColor(hex)")));
        QVERIFY(source.contains(QStringLiteral("Editor.setSelectedGradientColorA(hex)")));
        QVERIFY(source.contains(QStringLiteral("Editor.setSelectedGradientColorB(hex)")));
        QVERIFY(!source.contains(QStringLiteral("inputMask")));
        QVERIFY(!source.contains(QStringLiteral("onEditingFinished: Editor.setSelectedTextColor")));
        QVERIFY(!source.contains(QStringLiteral("onEditingFinished: Editor.setSelectedOutlineColor")));
        QVERIFY(!source.contains(QStringLiteral("onEditingFinished: Editor.setSelectedShadowColor")));
        QVERIFY(!source.contains(QStringLiteral("onEditingFinished: Editor.setSelectedGradientColorA")));
        QVERIFY(!source.contains(QStringLiteral("onEditingFinished: Editor.setSelectedGradientColorB")));
    }

    void qmlChromeUsesPaletteAndResponsivePanels()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        for (const QString& removedChromeColor : {
                 QStringLiteral("#202124"), QStringLiteral("#24282f"), QStringLiteral("#111318"),
                 QStringLiteral("#c9d1d9"), QStringLiteral("#8b949e"), QStringLiteral("#30363d"),
                 QStringLiteral("#58a6ff"), QStringLiteral("#58a6ff22")}) {
            QVERIFY2(!source.contains(removedChromeColor), qPrintable(removedChromeColor));
        }

        QVERIFY(source.contains(QStringLiteral("color: window.palette.window")));
        QVERIFY(source.contains(QStringLiteral("color: window.palette.base")));
        QVERIFY(source.contains(QStringLiteral("window.palette.highlight")));
        QVERIFY(source.contains(QStringLiteral("window.palette.highlightedText")));
        QVERIFY(source.contains(QStringLiteral("SplitView {")));
        QVERIFY(source.contains(QStringLiteral("orientation: Qt.Horizontal")));
        QVERIFY(source.contains(QStringLiteral("SplitView.minimumWidth: 240")));
        QVERIFY(source.contains(QStringLiteral("SplitView.preferredWidth: 280")));
        QVERIFY(source.contains(QStringLiteral("SplitView.fillWidth: true")));
        QVERIFY(source.contains(QStringLiteral("id: sidePanel")));
        QVERIFY(source.contains(QStringLiteral("id: canvasSlot")));
        QVERIFY(source.contains(QStringLiteral("id: canvas")));
        QVERIFY(source.contains(QStringLiteral("id: rightPanel")));
        QVERIFY(source.contains(QStringLiteral("x: -canvasSlot.x")));
        QVERIFY(source.contains(QStringLiteral("width: mainSplit.width")));

        const qsizetype sidePanelStart = source.indexOf(QStringLiteral("id: sidePanel"));
        const qsizetype canvasStart = source.indexOf(QStringLiteral("id: canvas\n"), sidePanelStart);
        QVERIFY(sidePanelStart >= 0);
        QVERIFY(canvasStart > sidePanelStart);
        const qsizetype canvasSlotStart = source.indexOf(QStringLiteral("id: canvasSlot"), sidePanelStart);
        const qsizetype rightPanelStart = source.indexOf(QStringLiteral("id: rightPanel"), canvasStart);
        QVERIFY(canvasSlotStart > sidePanelStart);
        QVERIFY(canvasStart > canvasSlotStart);
        QVERIFY(rightPanelStart > canvasStart);
        QVERIFY(source.mid(sidePanelStart, source.indexOf(QStringLiteral("SplitView.minimumWidth: 240"), sidePanelStart) - sidePanelStart).contains(QStringLiteral("z: 1")));
        QVERIFY(source.mid(canvasSlotStart, canvasStart - canvasSlotStart).contains(QStringLiteral("z: 0")));
        QVERIFY(source.mid(canvasStart, source.indexOf(QStringLiteral("x: -canvasSlot.x"), canvasStart) - canvasStart).contains(QStringLiteral("z: 0")));
        QVERIFY(source.mid(rightPanelStart, source.indexOf(QStringLiteral("SplitView.minimumWidth: 180"), rightPanelStart) - rightPanelStart).contains(QStringLiteral("z: 1")));
        const QString sidePanelSource = source.mid(sidePanelStart, canvasStart - sidePanelStart);
        QVERIFY(source.lastIndexOf(QStringLiteral("Pane {"), sidePanelStart) >= 0);
        QVERIFY(sidePanelSource.contains(QStringLiteral("GroupBox")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: true; enabled: textPropertiesSection.sectionReady }")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; enabled: textPresetsSection.sectionReady }")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; enabled: pageTextsSection.sectionReady }")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("ScrollBar.vertical.policy: ScrollBar.AlwaysOff")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("ScrollBar.horizontal.policy: ScrollBar.AlwaysOff")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("Layout.minimumHeight: 160")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("Layout.minimumHeight: minimumListHeight + topPadding + bottomPadding")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("id: leftPanelScroll")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("contentWidth: availableWidth")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("width: leftPanelScroll.availableWidth")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("height: Math.max(implicitHeight, leftPanelScroll.availableHeight)")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("height: Math.max(implicitHeight, sidePanel.availableHeight)")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("TextField { id: presetNameField; Layout.fillWidth: true; Layout.minimumWidth: 0")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("RowLayout {\n                                            Layout.fillWidth: true\n                                            Layout.minimumWidth: 0\n                                            spacing: 8")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: editorLimits.minimumFontSize; to: editorLimits.maximumFontSize")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: editorLimits.minimumTextSpacing; to: editorLimits.maximumTextSpacing")));
        QVERIFY(sidePanelSource.count(QStringLiteral("Flow {\n                                            Layout.fillWidth: true\n                                            Layout.minimumWidth: 0")) >= 3);
        const qsizetype propertiesStart = sidePanelSource.indexOf(QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: true; enabled: textPropertiesSection.sectionReady }"));
        const qsizetype presetsStart = sidePanelSource.indexOf(QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; enabled: textPresetsSection.sectionReady }"));
        const qsizetype pageTextsStart = sidePanelSource.indexOf(QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; enabled: pageTextsSection.sectionReady }"));
        QVERIFY(propertiesStart >= 0);
        QVERIFY(presetsStart > propertiesStart);
        QVERIFY(pageTextsStart > presetsStart);
        QVERIFY(sidePanelSource.mid(propertiesStart, presetsStart - propertiesStart).contains(QStringLiteral("Layout.fillWidth: true")));
        QVERIFY(sidePanelSource.mid(presetsStart, pageTextsStart - presetsStart).contains(QStringLiteral("Layout.fillWidth: true")));
        QVERIFY(sidePanelSource.mid(pageTextsStart).contains(QStringLiteral("Layout.fillWidth: true")));
        const qsizetype pageTextsBox = sidePanelSource.indexOf(QStringLiteral("id: pageTextsFrame"), pageTextsStart);
        const qsizetype pageTextsList = sidePanelSource.indexOf(QStringLiteral("id: pageTextsList"), pageTextsBox);
        QVERIFY(pageTextsBox > pageTextsStart);
        QVERIFY(pageTextsList > pageTextsBox);
        QVERIFY(sidePanelSource.mid(pageTextsBox, pageTextsList - pageTextsBox).contains(QStringLiteral("Layout.fillHeight: true")));
        QVERIFY(sidePanelSource.mid(pageTextsList, sidePanelSource.indexOf(QStringLiteral("delegate: ItemDelegate"), pageTextsList) - pageTextsList).contains(QStringLiteral("Layout.fillHeight: true")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("color: \"#")));
        QVERIFY(!sidePanelSource.contains(QStringLiteral("border.color: \"#")));
    }

    void qmlPageScaleIsNotBoundToCanvasViewport()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("property real pageBaseScale: 1.0")));
        QVERIFY(source.contains(QStringLiteral("function fitPageScale()")));
        QVERIFY(source.contains(QStringLiteral("function pageScale() { return pageBaseScale }")));
        QVERIFY(source.contains(QStringLiteral("onStatusChanged: if (status === Image.Ready) window.pageBaseScale = window.fitPageScale()")));

        const qsizetype pageScaleStart = source.indexOf(QStringLiteral("function pageScale()"));
        const qsizetype pageScaleEnd = source.indexOf(QStringLiteral("function pageDisplayWidth()"), pageScaleStart);
        QVERIFY(pageScaleStart >= 0);
        QVERIFY(pageScaleEnd > pageScaleStart);
        const QString pageScaleSource = source.mid(pageScaleStart, pageScaleEnd - pageScaleStart);
        QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.width")));
        QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.height")));
        QVERIFY(source.contains(QStringLiteral("function pageDisplayWidth() { return pageImage.sourceSize.width * pageScale() }")));
        QVERIFY(source.contains(QStringLiteral("function pageDisplayHeight() { return pageImage.sourceSize.height * pageScale() }")));
    }

    void qmlRightPanelUsesSectionTabsAndNoHorizontalOverflow()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        const qsizetype rightPanelStart = source.indexOf(QStringLiteral("id: rightPanel"));
        const qsizetype popupStart = source.indexOf(QStringLiteral("Popup {"), rightPanelStart);
        QVERIFY(rightPanelStart >= 0);
        QVERIFY(popupStart > rightPanelStart);
        const QString rightPanelSource = source.mid(rightPanelStart, popupStart - rightPanelStart);

        QVERIFY(rightPanelSource.contains(QStringLiteral("id: rightPanelScroll")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("contentWidth: availableWidth")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("width: rightPanelScroll.availableWidth")));
        QVERIFY(!rightPanelSource.contains(QStringLiteral("width: Math.max(1, rightPanel.availableWidth)")));

        for (const QString& sectionId : {QStringLiteral("navigationSection"), QStringLiteral("boxEffectsSection"), QStringLiteral("textEffectsSection"), QStringLiteral("layersSection")}) {
            QVERIFY2(rightPanelSource.contains(QStringLiteral("id: ") + sectionId), qPrintable(sectionId));
        }
        QVERIFY(rightPanelSource.contains(QStringLiteral("Label { text: qsTr(\"Navigation\"); font.bold: true; enabled: navigationSection.sectionReady }")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("Label { text: qsTr(\"Box Effects\"); font.bold: true; enabled: boxEffectsSection.sectionReady }")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("Label { text: qsTr(\"Text Effects\"); font.bold: true; enabled: textEffectsSection.sectionReady }")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("Label { text: qsTr(\"Layers\"); font.bold: true; enabled: layersSection.sectionReady }")));
        QVERIFY(rightPanelSource.count(QStringLiteral("GroupBox {")) >= 4);
        QVERIFY(rightPanelSource.count(QStringLiteral("Layout.minimumWidth: 0")) >= 20);
        QVERIFY(rightPanelSource.contains(QStringLiteral("contentItem: Label { text: pageSelect.displayText")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("contentItem: Label { text: layerDelegate.text")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("elide: Text.ElideRight")));

        const qsizetype boxStart = rightPanelSource.indexOf(QStringLiteral("id: boxEffectsSection"));
        const qsizetype textStart = rightPanelSource.indexOf(QStringLiteral("id: textEffectsSection"), boxStart);
        QVERIFY(boxStart >= 0);
        QVERIFY(textStart > boxStart);
        const QString boxEffectsSource = rightPanelSource.mid(boxStart, textStart - boxStart);
        QVERIFY(boxEffectsSource.contains(QStringLiteral("id: boxEffectsTabs")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("StackLayout")));
        QVERIFY(!boxEffectsSource.contains(QStringLiteral("TabButton { width:")));
        QVERIFY(!boxEffectsSource.contains(QStringLiteral("boxEffectsTabs.width")));
        QVERIFY(!boxEffectsSource.contains(QStringLiteral("parent.width")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("TabButton { text: qsTr(\"Rotation\") }")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("TabButton { text: qsTr(\"Perspective\") }")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("Editor.setSelectedRotation(value)")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("Editor.setSelectedPerspectiveEnabled(checked)")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("Editor.resetSelectedPerspective()")));

        const qsizetype layersStart = rightPanelSource.indexOf(QStringLiteral("id: layersSection"), textStart);
        QVERIFY(layersStart > textStart);
        const QString textEffectsSource = rightPanelSource.mid(textStart, layersStart - textStart);
        QVERIFY(textEffectsSource.contains(QStringLiteral("id: textEffectsTabs")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("StackLayout")));
        QVERIFY(!textEffectsSource.contains(QStringLiteral("TabButton { width:")));
        QVERIFY(!textEffectsSource.contains(QStringLiteral("textEffectsTabs.width")));
        QVERIFY(!textEffectsSource.contains(QStringLiteral("parent.width")));
        for (const QString& feature : {QStringLiteral("Outline"), QStringLiteral("Blur"), QStringLiteral("Shadow"), QStringLiteral("Gradient"), QStringLiteral("Path")}) {
            QVERIFY2(textEffectsSource.contains(QStringLiteral("TabButton { text: qsTr(\"") + feature + QStringLiteral("\") }")), qPrintable(feature));
        }
        QVERIFY(textEffectsSource.contains(QStringLiteral("Editor.setSelectedOutlineEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("Editor.setSelectedBlurEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("Editor.setSelectedShadowEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("Editor.setSelectedGradientEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("Editor.setSelectedPathEnabled(checked)")));
    }

    void qmlMenusOwnPrimaryControlsAndToolbarIsRemoved()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();
        const qsizetype menuItemStart = source.indexOf(QStringLiteral("component ShortcutMenuItem: MenuItem"));
        const qsizetype menuItemEnd = source.indexOf(QStringLiteral("function pageScale()"));
        QVERIFY(menuItemStart >= 0);
        QVERIFY(menuItemEnd > menuItemStart);
        const QString menuItemSource = source.mid(menuItemStart, menuItemEnd - menuItemStart);

        QVERIFY(source.contains(QStringLiteral("component ShortcutMenuItem: MenuItem")));
        QVERIFY(source.contains(QStringLiteral("contentItem: RowLayout")));
        QVERIFY(source.contains(QStringLiteral("Layout.fillWidth: true")));
        QVERIFY(source.contains(QStringLiteral("shortcutMenuItem.shortcutLabel")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("indicator:")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("shortcutMenuItem.checked")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("✓")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("Layout.preferredWidth: 16")));
        QVERIFY(!source.contains(QStringLiteral("Layout.preferredWidth: shortcutMenuItem.checkable ? 16 : 0")));
        QVERIFY(source.contains(QStringLiteral("Action { id: openAction; text: qsTr(\"Open\"); shortcut: StandardKey.Open")));
        QVERIFY(source.contains(QStringLiteral("Action { id: saveAction; text: qsTr(\"Save\"); shortcut: StandardKey.Save")));
        QVERIFY(source.contains(QStringLiteral("Action { id: saveAllAction; text: qsTr(\"Save All\"); shortcut: \"Ctrl+Shift+S\"")));
        QVERIFY(source.contains(QStringLiteral("Action { id: quitAction; text: qsTr(\"Quit\"); onTriggered: Qt.quit() }")));
        QVERIFY(source.contains(QStringLiteral("Action { id: copyAction; text: qsTr(\"Copy\"); shortcut: StandardKey.Copy")));
        QVERIFY(source.contains(QStringLiteral("Action { id: pasteAction; text: qsTr(\"Paste\"); shortcut: StandardKey.Paste")));
        QVERIFY(source.contains(QStringLiteral("Action { id: deleteAction; text: qsTr(\"Delete\"); shortcut: StandardKey.Delete")));
        QVERIFY(source.contains(QStringLiteral("Action { id: duplicateAction; text: qsTr(\"Duplicate\"); enabled:")));
        QVERIFY(source.contains(QStringLiteral("Action { id: zoomInAction; text: qsTr(\"Zoom In\"); shortcut: \"Ctrl++\"")));
        QVERIFY(source.contains(QStringLiteral("Action { id: zoomOutAction; text: qsTr(\"Zoom Out\"); shortcut: \"Ctrl+-\"")));
        QVERIFY(source.contains(QStringLiteral("Action { id: resetZoomAction; text: qsTr(\"Reset Zoom\"); shortcut: \"Ctrl+0\"")));
        QVERIFY(source.contains(QStringLiteral("Action { id: rawOverlayAction; text: qsTr(\"Show Raw Overlay\"); checkable: true; checked: Editor.rawVisible; shortcut: \"Ctrl+H\"")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: openAction; shortcutLabel: \"Ctrl+O\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: saveAction; shortcutLabel: \"Ctrl+S\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: saveAllAction; shortcutLabel: \"Ctrl+Shift+S\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: quitAction }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: copyAction; shortcutLabel: \"Ctrl+C\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: pasteAction; shortcutLabel: \"Ctrl+V\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: deleteAction; shortcutLabel: \"Del\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: zoomInAction; shortcutLabel: \"Ctrl++\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: zoomOutAction; shortcutLabel: \"Ctrl+-\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: resetZoomAction; shortcutLabel: \"Ctrl+0\" }")));
        QVERIFY(source.contains(QStringLiteral("ShortcutMenuItem { action: rawOverlayAction; shortcutLabel: \"Ctrl+H\" }")));

        QVERIFY(source.contains(QStringLiteral("menuBar: MenuBar")));
        QVERIFY(!source.contains(QStringLiteral("menuBarVisible")));
        QVERIFY(!source.contains(QStringLiteral("menuBarAction")));
        QVERIFY(!source.contains(QStringLiteral("Hide Menu Bar")));
        QVERIFY(!source.contains(QStringLiteral("Show Menu Bar")));
        QVERIFY(!source.contains(QStringLiteral("Shortcut { sequence: \"Alt\"")));
        QVERIFY(!source.contains(QStringLiteral("ToolBar")));
        QVERIFY(!source.contains(QStringLiteral("ToolButton")));
        QVERIFY(!source.contains(QStringLiteral("Show Top Bar")));
        QVERIFY(!source.contains(QStringLiteral("Hide Top Bar")));
        QVERIFY(!source.contains(QStringLiteral("shortcut: \"Ctrl+D\"")));
        QVERIFY(!source.contains(QStringLiteral("StandardKey.Quit")));
        QVERIFY(!source.contains(QStringLiteral("Ctrl+Q")));
        QVERIFY(!source.contains(QStringLiteral("Ctrl+Alt+M")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("CheckBox")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("CheckIndicator")));
        QVERIFY(!source.contains(QStringLiteral("Duplicate\"), \"Ctrl+")));
        QVERIFY(!source.contains(QStringLiteral("ShortcutMenuItem { action: duplicateAction; shortcutLabel:")));
        QVERIFY(!source.contains(QStringLiteral("qsTr(\"Open Ctrl+")));
        QVERIFY(!source.contains(QStringLiteral("qsTr(\"Save Ctrl+")));
        QVERIFY(!source.contains(QStringLiteral("qsTr(\"Save All Ctrl+")));
        QVERIFY(!source.contains(QStringLiteral("qsTr(\"Open Project…\")")));
        QVERIFY(!source.contains(QStringLiteral("qsTr(\"Export\")")));
        QVERIFY(!source.contains(QStringLiteral("Editor.newDocument()")));
        QVERIFY(!source.contains(QStringLiteral("Editor.exportPng()")));
        QVERIFY(!source.contains(QStringLiteral("localPathFromUrl")));
        QVERIFY(source.contains(QStringLiteral("Editor.openProjectUrl(selectedFolder)")));
    }

    void qmlHasPageSelectorAndTypeXEffectControls()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("model: Editor.pageLabels")));
        QVERIFY(source.contains(QStringLiteral("Editor.goToPage(index)")));
        QVERIFY(source.contains(QStringLiteral("model: [qsTr(\"Vertical\"), qsTr(\"Horizontal\")]")));
        QVERIFY(source.contains(QStringLiteral("model: [qsTr(\"Straight\"), qsTr(\"Smooth\")]")));
        QVERIFY(source.contains(QStringLiteral("Editor.addSelectedPathPoint()")));
        QVERIFY(source.contains(QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints")));
        QVERIFY(!source.contains(QStringLiteral("Double-click canvas to create text")));
        QVERIFY(!source.contains(QStringLiteral("Live editing uses QML layout; rendered effects apply on export.")));
    }

    void qmlUsesOneOutlinedTextItemDisplayRenderer()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(!source.contains(QStringLiteral("id: effectsPreviewImage")));
        QVERIFY(!source.contains(QStringLiteral("source: Editor.previewImageUrl")));
        QVERIFY(!source.contains(QStringLiteral("effectsPreviewDisplayable")));
        QVERIFY(source.contains(QStringLiteral("visible: source.toString().length > 0")));
        QVERIFY(!source.contains(QStringLiteral("function boxHasRenderEffects(box)")));
        QVERIFY(!source.contains(QStringLiteral("function selectedBoxHasRenderEffects()")));
        QVERIFY(!source.contains(QStringLiteral("function boxNeedsPreviewArtifact(box)")));
        QVERIFY(!source.contains(QStringLiteral("function anyBoxNeedsPreviewArtifact()")));
        QVERIFY(source.contains(QStringLiteral("visible: boxRef.selected && editorRef.editingText")));
        QVERIFY(source.contains(QStringLiteral("OutlinedTextItem {")));
        QCOMPARE(source.count(QStringLiteral("OutlinedTextItem {")), 1);
        QVERIFY(source.contains(QStringLiteral("id: boxOutlinedText")));
        QVERIFY(source.contains(QStringLiteral("width: boxRef.visualDocW * rootWindow.livePreviewScale()")));
        QVERIFY(source.contains(QStringLiteral("height: boxRef.visualDocH * rootWindow.livePreviewScale()")));
        QVERIFY(source.contains(QStringLiteral("scale: rootWindow.viewDocScale() / rootWindow.livePreviewScale()")));
        QVERIFY(source.contains(QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
        QVERIFY(!source.contains(QStringLiteral("liveGpuBlurActive")));
        QVERIFY(!source.contains(QStringLiteral("MultiEffect {")));
        QVERIFY(!source.contains(QStringLiteral("source: boxOutlinedText")));
        QVERIFY(source.contains(QStringLiteral("outlineSize: modelData.outline && modelData.outlineSize > 0 ? modelData.outlineSize : 0")));
        QVERIFY(source.contains(QStringLiteral("blurSize: modelData.blur && modelData.blurSize > 0 ? modelData.blurSize : 0")));
        QVERIFY(source.contains(QStringLiteral("shadowEnabled: modelData.shadow")));
        QVERIFY(source.contains(QStringLiteral("shadowBlurSize: modelData.shadow && modelData.shadowBlurSize > 0 ? modelData.shadowBlurSize : 0")));
        QVERIFY(source.contains(QStringLiteral("gradientEnabled: modelData.gradient")));
        QVERIFY(source.contains(QStringLiteral("pathEnabled: modelData.path")));
        QVERIFY(source.contains(QStringLiteral("pathPoints: modelData.pathPoints")));
        QVERIFY(source.contains(QStringLiteral("lineSpacing: modelData.lineSpacing")));
        QVERIFY(source.contains(QStringLiteral("outlineColor: rootWindow.qmlColor(modelData.outlineColor)")));
        QVERIFY(source.indexOf(QStringLiteral("id: boxOutlinedText")) < source.indexOf(QStringLiteral("TextArea {")));
        QVERIFY(source.contains(QStringLiteral("editorRef.beginInteraction()")));
        QVERIFY(source.contains(QStringLiteral("editorRef.endInteraction()")));
    }

    void qmlEditingKeepsOutlinedRendererVisible()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const int renderer = source.indexOf(QStringLiteral("id: boxOutlinedText"));
        const int editor = source.indexOf(QStringLiteral("id: boxTextArea"));
        QVERIFY(renderer >= 0);
        QVERIFY(editor > renderer);

        const QString rendererBlock = source.mid(renderer, editor - renderer);
        QVERIFY(!rendererBlock.contains(QStringLiteral("effectsPreviewDisplayable")));
        QVERIFY(rendererBlock.contains(QStringLiteral("text: boxRef.selected && editorRef.editingText ? boxTextArea.text : (modelData.uppercase ? String(modelData.text).toUpperCase() : modelData.text)")));
        QVERIFY(rendererBlock.contains(QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
        QVERIFY(rendererBlock.contains(QStringLiteral("outlineSize: modelData.outline && modelData.outlineSize > 0 ? modelData.outlineSize : 0")));
        QVERIFY(rendererBlock.contains(QStringLiteral("blurSize: modelData.blur && modelData.blurSize > 0 ? modelData.blurSize : 0")));
        QVERIFY(rendererBlock.contains(QStringLiteral("shadowEnabled: modelData.shadow")));
        QVERIFY(rendererBlock.contains(QStringLiteral("shadowBlurSize: modelData.shadow && modelData.shadowBlurSize > 0 ? modelData.shadowBlurSize : 0")));
        QVERIFY(rendererBlock.contains(QStringLiteral("gradientEnabled: modelData.gradient")));
        QVERIFY(rendererBlock.contains(QStringLiteral("pathEnabled: modelData.path")));
        QVERIFY(rendererBlock.contains(QStringLiteral("pathPoints: modelData.pathPoints")));
        QVERIFY(rendererBlock.contains(QStringLiteral("lineSpacing: modelData.lineSpacing")));

        const QString editorBlock = source.mid(editor, source.indexOf(QStringLiteral("MouseArea {"), editor) - editor);
        QVERIFY(editorBlock.contains(QStringLiteral("visible: boxRef.selected && editorRef.editingText")));
        QVERIFY(editorBlock.contains(QStringLiteral("color: \"transparent\"")));
        QVERIFY(editorBlock.contains(QStringLiteral("selectedTextColor: \"transparent\"")));
        QVERIFY(editorBlock.contains(QStringLiteral("selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)")));
        QVERIFY(editorBlock.contains(QStringLiteral("property real editLineSpacing: modelData.lineSpacing * rootWindow.viewDocScale()")));
        QVERIFY(editorBlock.contains(QStringLiteral("applyTextLineSpacing(textDocument, editLineSpacing)")));
        QVERIFY(editorBlock.contains(QStringLiteral("onEditLineSpacingChanged: applyLineSpacing()")));
        QVERIFY(editorBlock.contains(QStringLiteral("cursorDelegate: Rectangle")));
        QVERIFY(editorBlock.contains(QStringLiteral("onTextChanged: { applyLineSpacing(); if (activeFocus && editorRef) editorRef.updateSelectedText(text) }")));
    }

    void qmlPerspectiveWarpsLiveOutlinedRenderer()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("function perspectiveMatrix(box, width, height, renderScale, enabled)")));
        QVERIFY(source.contains(QStringLiteral("function perspectiveLayoutCorner(box, name, width, height)")));
        QVERIFY(source.contains(QStringLiteral("function perspectiveCorner(box, name, width, height)")));
        QVERIFY(source.contains(QStringLiteral("function visualHandlePosition(box, name, width, height)")));
        QVERIFY(source.contains(QStringLiteral("const corner = perspectiveLayoutCorner(box, name, width / scale, height / scale)")));
        QVERIFY(source.contains(QStringLiteral("x: corner.x * scale")));
        QVERIFY(source.contains(QStringLiteral("property bool editingSelected: selected && editorRef.editingText")));
        QVERIFY(source.contains(QStringLiteral("property bool perspectiveActive: boxModel.perspective && !editingSelected")));
        QVERIFY(source.contains(QStringLiteral("id: boxTextPerspective")));
        QVERIFY(source.contains(QStringLiteral("transform: Matrix4x4 { matrix: boxTextPerspective.rootWindow.perspectiveMatrix(boxTextPerspective.boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, boxTextPerspective.rootWindow.viewDocScale(), boxTextPerspective.boxRef.perspectiveActive) }")));
        QVERIFY(!source.contains(QStringLiteral("transform: Matrix4x4 { matrix: rootWindow.perspectiveMatrix(boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, rootWindow.viewDocScale(), boxRef.perspectiveActive) }")));
        QVERIFY(source.contains(QStringLiteral("rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).x")));
        QVERIFY(source.contains(QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)")));
        QVERIFY(source.contains(QStringLiteral("resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)")));
        QVERIFY(source.contains(QStringLiteral("perspectiveStartX = modelPerspectiveOffset(delegate.boxModel, handle, 0)")));
        QVERIFY(source.contains(QStringLiteral("const dx = (local.x - startLocal.x) / viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("perspectiveX = perspectiveStartX + (activePerspectiveHandle === \"n\" || activePerspectiveHandle === \"s\" ? 0 : dx)")));
        QVERIFY(source.contains(QStringLiteral("const delta = rotatePoint((canvasX - resizeStartCanvasX) / viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("right = Math.max(resizeStartW + delta.x, left + editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("bottom = Math.max(resizeStartH + delta.y, top + editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("left = Math.min(delta.x, right - editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("top = Math.min(delta.y, bottom - editorLimits.minimumBoxSize)")));
        QVERIFY(source.contains(QStringLiteral("const startLocal = activePerspectiveDelegate.mapFromItem(canvas, perspectiveStartCanvasX, perspectiveStartCanvasY)")));
        QVERIFY(!source.contains(QStringLiteral("offsetFromCenter")));
        QVERIFY(!source.contains(QStringLiteral("dx / length * 16")));
        QVERIFY(!source.contains(QStringLiteral("drag.target: parent; onPressed: Editor.beginInteraction(); onReleased: Editor.endInteraction(); onCanceled: Editor.endInteraction(); onPositionChanged: Editor.setPerspectiveHandle")));
        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype pathStart = source.indexOf(QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(pathStart > resizeStart);
        const QString manualHandleSource = source.mid(resizeStart, pathStart - resizeStart);
        QVERIFY(!manualHandleSource.contains(QStringLiteral("drag.target")));
        QVERIFY(manualHandleSource.contains(QStringLiteral("mapToItem(canvasItem, mouse.x, mouse.y)")));
        QVERIFY(source.contains(QStringLiteral("activePerspectiveDelegate.mapFromItem(canvas")));
        QVERIFY(source.contains(QStringLiteral("window.editor.setSelectedBounds(resizeX, resizeY, resizeW, resizeH)")));
        QVERIFY(source.contains(QStringLiteral("window.editor.setPerspectiveHandle")));
        QVERIFY(source.contains(QStringLiteral("border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))")));
        QVERIFY(source.contains(QStringLiteral("id: perspectiveBorder")));
        QVERIFY(source.contains(QStringLiteral("visible: boxRef.selected && boxRef.perspectiveActive")));
        QVERIFY(source.contains(QStringLiteral("return Qt.matrix4x4(1, 0, 0, 0,")));

        const qsizetype wrapper = source.indexOf(QStringLiteral("id: boxTextPerspective"));
        const qsizetype renderer = source.indexOf(QStringLiteral("id: boxOutlinedText"), wrapper);
        const qsizetype pathGuide = source.indexOf(QStringLiteral("id: pathGuide"), renderer);
        const qsizetype editor = source.indexOf(QStringLiteral("TextArea {"), pathGuide);
        QVERIFY(wrapper >= 0);
        QVERIFY(renderer > wrapper);
        QVERIFY(pathGuide > renderer);
        QVERIFY(editor > renderer);
        QVERIFY(source.mid(renderer, pathGuide - renderer).contains(QStringLiteral("transform: Matrix4x4")) == false);
    }

    void qmlPerspectiveResizeDragStartsFromVisualHandle()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype rotateUse = source.indexOf(QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height)"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateUse > resizeStart);
        const QString resizeSource = source.mid(resizeStart, rotateUse - resizeStart);

        QVERIFY(resizeSource.contains(QStringLiteral("x: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).x - width / 2")));
        QVERIFY(resizeSource.contains(QStringLiteral("const point = mapToItem(canvasItem, mouse.x, mouse.y)")));
        const qsizetype perspectiveBranch = resizeSource.indexOf(QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
        const qsizetype perspectiveBegin = resizeSource.indexOf(QStringLiteral("resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)"));
        const qsizetype normalBegin = resizeSource.indexOf(QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)"));
        QVERIFY(perspectiveBranch >= 0);
        QVERIFY(perspectiveBegin > perspectiveBranch);
        QVERIFY(normalBegin > perspectiveBegin);
        QVERIFY(source.contains(QStringLiteral("perspectiveStartCanvasX = canvasX")));
        QVERIFY(source.contains(QStringLiteral("perspectiveStartCanvasY = canvasY")));
        QVERIFY(source.contains(QStringLiteral("window.editor.setPerspectiveHandle(activePerspectiveHandle,")));
    }

    void qmlPerspectiveDragDoesNotApplyVisualOffsetToGeometryDelta()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype updateStart = source.indexOf(QStringLiteral("function updatePerspectiveDrag(canvasX, canvasY)"));
        const qsizetype updateEnd = source.indexOf(QStringLiteral("function endPerspectiveDrag(commit)"), updateStart);
        QVERIFY(updateStart >= 0);
        QVERIFY(updateEnd > updateStart);
        const QString updateSource = source.mid(updateStart, updateEnd - updateStart);

        QVERIFY(updateSource.contains(QStringLiteral("canvasX - perspectiveStartCanvasX")) == false);
        QVERIFY(updateSource.contains(QStringLiteral("const dx = (local.x - startLocal.x) / viewDocScale()")));
        QVERIFY(updateSource.contains(QStringLiteral("perspectiveX = perspectiveStartX + (activePerspectiveHandle === \"n\" || activePerspectiveHandle === \"s\" ? 0 : dx)")));
        QVERIFY(!updateSource.contains(QStringLiteral("visualHandlePosition")));
        QVERIFY(!updateSource.contains(QStringLiteral("offsetFromCenter")));
        QVERIFY(!updateSource.contains(QStringLiteral("perspectiveHandleOffset")));
    }

    void qmlNormalResizePathRemainsSeparateFromPerspectiveDrag()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype rotateUse = source.indexOf(QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height)"), resizeStart);
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateUse > resizeStart);
        const QString resizeSource = source.mid(resizeStart, rotateUse - resizeStart);

        const qsizetype perspectiveBranch = resizeSource.indexOf(QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
        const qsizetype perspectiveBegin = resizeSource.indexOf(QStringLiteral("resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)"));
        const qsizetype normalBegin = resizeSource.indexOf(QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)"));
        QVERIFY(perspectiveBranch >= 0);
        QVERIFY(perspectiveBegin > perspectiveBranch);
        QVERIFY(normalBegin > perspectiveBegin);
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.updatePerspectiveDrag(point.x, point.y)")));
        QVERIFY(resizeSource.contains(QStringLiteral("resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)")));
        QVERIFY(source.contains(QStringLiteral("function beginResizeDrag(delegate, handle, canvasX, canvasY)")));
        QVERIFY(source.contains(QStringLiteral("function beginPerspectiveDrag(delegate, handle, canvasX, canvasY)")));
    }

    void qmlPerspectiveHandlesStayOnGeometry()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(!source.contains(QStringLiteral("perspectiveHandleOffset")));
        QVERIFY(!source.contains(QStringLiteral("function offsetFromCenter(point, width, height)")));
        QVERIFY(source.contains(QStringLiteral("return perspectiveCorner(box, name, width, height)")));
        QVERIFY(source.contains(QStringLiteral("function rotateHandlePosition(box, width, height)")));
        QVERIFY(source.contains(QStringLiteral("return { x: top.x, y: top.y - rotateHandleDistance() }")));

        const qsizetype resizeStart = source.indexOf(QStringLiteral("model: [\n            {name: \"nw\"}"));
        const qsizetype rotateUse = source.indexOf(QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height)"));
        QVERIFY(resizeStart >= 0);
        QVERIFY(rotateUse > resizeStart);
        QVERIFY(source.mid(resizeStart, rotateUse - resizeStart).contains(QStringLiteral("rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).x")));
        QVERIFY(source.mid(resizeStart, rotateUse - resizeStart).contains(QStringLiteral("width: rootWindow.handleSize(); height: width; radius: width / 2")));

        const qsizetype borderStart = source.indexOf(QStringLiteral("id: perspectiveBorder"));
        const qsizetype borderEnd = source.indexOf(QStringLiteral("OutlinedTextItem"), borderStart);
        QVERIFY(borderStart >= 0);
        QVERIFY(borderEnd > borderStart);
        const QString borderSource = source.mid(borderStart, borderEnd - borderStart);
        QVERIFY(borderSource.contains(QStringLiteral("rootWindow.perspectiveCorner(boxRef.boxModel, \"nw\", boxRef.width, boxRef.height)")));
        QVERIFY(!borderSource.contains(QStringLiteral("visualHandlePosition")));
        QVERIFY(!borderSource.contains(QStringLiteral("offsetFromCenter")));
        QVERIFY(!borderSource.contains(QStringLiteral("perspectiveHandleOffset")));

        QVERIFY(!source.contains(QStringLiteral("resizeHandleOffset")));
    }

    void qmlZoomUsesSingleDocumentToViewScaleContract()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype renderer = source.indexOf(QStringLiteral("id: boxOutlinedText"));
        const qsizetype editor = source.indexOf(QStringLiteral("TextArea {"), renderer);
        QVERIFY(renderer >= 0);
        QVERIFY(editor > renderer);
        const QString rendererBlock = source.mid(renderer, editor - renderer);

        QVERIFY(source.contains(QStringLiteral("function documentToViewLength(value) { return value * viewDocScale() }")));
        QVERIFY(source.contains(QStringLiteral("width: visualDocW * rootWindow.viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("height: visualDocH * rootWindow.viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("function livePreviewScale() { return Math.min(1.0, viewDocScale()) }")));
        QVERIFY(rendererBlock.contains(QStringLiteral("width: boxRef.visualDocW * rootWindow.livePreviewScale()")));
        QVERIFY(rendererBlock.contains(QStringLiteral("height: boxRef.visualDocH * rootWindow.livePreviewScale()")));
        QVERIFY(rendererBlock.contains(QStringLiteral("scale: rootWindow.viewDocScale() / rootWindow.livePreviewScale()")));
        QVERIFY(rendererBlock.contains(QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
        QVERIFY(rendererBlock.contains(QStringLiteral("pixelSize: Math.max(1, modelData.fontSize)")));
        QVERIFY(rendererBlock.contains(QStringLiteral("letterSpacing: modelData.letterSpacing")));
        QVERIFY(rendererBlock.contains(QStringLiteral("lineSpacing: modelData.lineSpacing")));
        QVERIFY(!rendererBlock.contains(QStringLiteral("modelData.fontSize * rootWindow.viewDocScale()")));
        QVERIFY(!rendererBlock.contains(QStringLiteral("modelData.letterSpacing * rootWindow.viewDocScale()")));
        QVERIFY(!rendererBlock.contains(QStringLiteral("modelData.lineSpacing * rootWindow.viewDocScale()")));
        QVERIFY(!rendererBlock.contains(QStringLiteral("modelData.outlineSize * rootWindow.viewDocScale()")));
        QVERIFY(source.contains(QStringLiteral("transform: Matrix4x4 { matrix: boxTextPerspective.rootWindow.perspectiveMatrix(boxTextPerspective.boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, boxTextPerspective.rootWindow.viewDocScale(), boxTextPerspective.boxRef.perspectiveActive) }")));
        QVERIFY(!source.contains(QStringLiteral("transform: Matrix4x4 { matrix: rootWindow.perspectiveMatrix(boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, rootWindow.viewDocScale(), boxRef.perspectiveActive) }")));
        QVERIFY(source.contains(QStringLiteral("const layoutWidth = width / scale")));
        QVERIFY(source.contains(QStringLiteral("const layoutHeight = height / scale")));
        QVERIFY(source.contains(QStringLiteral("const p0 = perspectiveLayoutCorner(box, \"nw\", layoutWidth, layoutHeight)")));
        QVERIFY(source.contains(QStringLiteral("x: perspectiveBase(name, 0) * width + perspectiveOffset(box, name, 0)")));
        QVERIFY(source.contains(QStringLiteral("x: corner.x * scale")));
        QVERIFY(source.contains(QStringLiteral("border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))")));
        QVERIFY(source.contains(QStringLiteral("width: rootWindow.handleSize(); height: width; radius: width / 2")));
        QVERIFY(source.contains(QStringLiteral("ctx.lineWidth = rootWindow.selectionLineWidth()")));
        QVERIFY(!source.contains(QStringLiteral("viewDocScale() * window.viewDocScale()")));
        QVERIFY(!source.contains(QStringLiteral("viewDocScale() * rootWindow.viewDocScale()")));
        QVERIFY(!source.contains(QStringLiteral("window.zoom * window.zoom")));
    }

    void outlinedTextItemWrapsInDocumentUnitsAcrossRenderScale()
    {
        auto linesAtScale = [](qreal scale) {
            OutlinedTextItem item;
            item.setWidth(96 * scale);
            item.setHeight(120 * scale);
            item.setText(QStringLiteral("DICHO QUE ME"));
            item.setPixelSize(24);
            item.setLetterSpacing(1);
            item.setLineSpacing(2);
            item.setOutlineSize(4);
            item.setRenderScale(scale);
            return item.wrappedLinesForTesting();
        };

        const QStringList expected = linesAtScale(1.0);
        QVERIFY(expected.size() >= 2);
        QCOMPARE(linesAtScale(0.75), expected);
        QCOMPARE(linesAtScale(1.5), expected);
    }

    void outlinedTextItemUsesOutlineSizeWhenPainting()
    {
        auto render = [](qreal outlineSize) {
            OutlinedTextItem item;
            item.setWidth(180);
            item.setHeight(80);
            item.setText(QStringLiteral("Outline"));
            item.setPixelSize(42);
            item.setColor(Qt::black);
            item.setOutlineColor(Qt::red);
            item.setOutlineSize(outlineSize);

            QImage image(180, 80, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };
        auto redPixels = [](const QImage& image) {
            int count = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (c.red() > 80 && c.green() < 40 && c.blue() < 40 && c.alpha() > 0) ++count;
                }
            }
            return count;
        };

        QVERIFY(redPixels(render(8)) > redPixels(render(1)));
    }

    void outlinedTextItemCentersTextVertically()
    {
        const QColor background(Qt::transparent);
        OutlinedTextItem item;
        item.setWidth(180);
        item.setHeight(100);
        item.setText(QStringLiteral("Center"));
        item.setPixelSize(32);
        item.setColor(Qt::black);

        QImage image(180, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(background);
        QPainter painter(&image);
        item.paint(&painter);

        const QRect bounds = visibleBounds(image, background);
        QVERIFY(!bounds.isEmpty());
        QVERIFY2(std::abs(bounds.center().y() - image.rect().center().y()) <= 4, qPrintable(QStringLiteral("bounds=%1,%2 %3x%4").arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height())));
    }

    void outlinedTextItemBlurSoftensRenderedText()
    {
        auto render = [](int blurSize) {
            OutlinedTextItem item;
            item.setWidth(180);
            item.setHeight(80);
            item.setText(QStringLiteral("Blur"));
            item.setPixelSize(46);
            item.setColor(Qt::black);
            item.setBlurSize(blurSize);

            QImage image(180, 80, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };
        const QImage sharp = render(0);
        const QImage blurred = render(6);
        const int hardSharpPixels = countPixels(sharp, [](const QColor& color) { return color.alpha() > 220; });
        const int hardBlurredPixels = countPixels(blurred, [](const QColor& color) { return color.alpha() > 220; });
        const int softBlurredPixels = countPixels(blurred, [](const QColor& color) { return color.alpha() > 20 && color.alpha() < 220; });

        QVERIFY(imagesDiffer(sharp, blurred));
        QVERIFY(hardBlurredPixels < hardSharpPixels);
        QVERIFY(softBlurredPixels > 200);
    }

    void outlinedTextItemShadowBlurSoftensRenderedShadow()
    {
        auto render = [](int shadowBlurSize) {
            OutlinedTextItem item;
            item.setWidth(200);
            item.setHeight(90);
            item.setText(QStringLiteral("Shadow"));
            item.setPixelSize(42);
            item.setColor(Qt::transparent);
            item.setShadowEnabled(true);
            item.setShadowColor(Qt::black);
            item.setShadowOffsetX(8);
            item.setShadowOffsetY(6);
            item.setShadowBlurSize(shadowBlurSize);

            QImage image(200, 90, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };
        const QImage sharp = render(0);
        const QImage blurred = render(6);
        const int hardSharpPixels = countPixels(sharp, [](const QColor& color) { return color.alpha() > 220; });
        const int hardBlurredPixels = countPixels(blurred, [](const QColor& color) { return color.alpha() > 220; });
        const int softBlurredPixels = countPixels(blurred, [](const QColor& color) { return color.alpha() > 20 && color.alpha() < 220; });

        QVERIFY(imagesDiffer(sharp, blurred));
        QVERIFY(hardBlurredPixels < hardSharpPixels);
        QVERIFY(softBlurredPixels > 200);
    }

    void outlinedTextItemGradientAndPathAffectRenderedText()
    {
        auto render = [](bool gradient, bool path) {
            OutlinedTextItem item;
            item.setWidth(200);
            item.setHeight(100);
            item.setText(QStringLiteral("GradientPath"));
            item.setPixelSize(28);
            item.setColor(Qt::black);
            item.setGradientEnabled(gradient);
            item.setGradientDirection(1);
            item.setGradientColorA(Qt::red);
            item.setGradientColorB(Qt::blue);
            item.setPathEnabled(path);
            item.setPathPoints(QVariantList{QVariantList{0.0, 0.85}, QVariantList{0.5, 0.2}, QVariantList{1.0, 0.85}});

            QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };

        const QImage plain = render(false, false);
        const QImage gradient = render(true, false);
        const QImage path = render(false, true);
        QVERIFY(imagesDiffer(plain, gradient));
        QVERIFY(imagesDiffer(plain, path));
    }

    void outlinedTextItemGradientPathMatchesExportSemantics()
    {
        const QColor background(240, 240, 240, 255);
        constexpr int width = 200;
        constexpr int height = 100;
        const QVariantList points{QVariantList{0.0, 0.85}, QVariantList{0.5, 0.2}, QVariantList{1.0, 0.85}};

        OutlinedTextItem item;
        item.setWidth(width);
        item.setHeight(height);
        item.setText(QStringLiteral("GradientPath"));
        item.setFontFamily(QStringLiteral("sans-serif"));
        item.setPixelSize(28);
        item.setColor(Qt::black);
        item.setGradientEnabled(true);
        item.setGradientDirection(1);
        item.setGradientColorA(Qt::red);
        item.setGradientColorB(Qt::blue);
        item.setPathEnabled(true);
        item.setPathPoints(points);

        QImage live(width, height, QImage::Format_ARGB32_Premultiplied);
        live.fill(background);
        QPainter painter(&live);
        item.paint(&painter);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pagePath = dir.filePath(QStringLiteral("page.png"));
        const QString exportPath = dir.filePath(QStringLiteral("export.png"));
        QImage page(width, height, QImage::Format_RGBA8888);
        page.fill(background);
        QVERIFY(page.save(pagePath, "PNG"));

        DocumentModel document;
        TextBox box;
        box.text = "GradientPath";
        box.bounds = {0.0, 0.0, width, height};
        box.style.fontSize = 28;
        box.style.textColor = "000000ff";
        box.effects.gradientEnabled = true;
        box.effects.gradientDirection = 1;
        box.effects.gradientColorA = "ff0000ff";
        box.effects.gradientColorB = "0000ffff";
        box.effects.pathEnabled = true;
        box.effects.pathPoints = {{0.0, 0.85}, {0.5, 0.2}, {1.0, 0.85}};
        document.addTextBox(box);

        std::string error;
        const RenderGraph graph;
        QVERIFY2(graph.exportPagePng(document, pagePath.toStdString(), exportPath.toStdString(), &error), error.c_str());
        const QImage exported(exportPath);
        QVERIFY(!exported.isNull());

        const QRect liveBounds = visibleBounds(live, background);
        const QRect exportBounds = visibleBounds(exported, background);
        QVERIFY(!liveBounds.isEmpty());
        QVERIFY(!exportBounds.isEmpty());
        QVERIFY(std::abs(liveBounds.center().x() - exportBounds.center().x()) <= 8);
        QVERIFY(std::abs(liveBounds.center().y() - exportBounds.center().y()) <= 8);

        const int liveRed = countPixels(live, [&](const QColor& color) { return color != background && color.red() > color.blue() + 30; });
        const int liveBlue = countPixels(live, [&](const QColor& color) { return color != background && color.blue() > color.red() + 30; });
        const int exportRed = countPixels(exported, [&](const QColor& color) { return color != background && color.red() > color.blue() + 30; });
        const int exportBlue = countPixels(exported, [&](const QColor& color) { return color != background && color.blue() > color.red() + 30; });
        QVERIFY(liveRed > 0);
        QVERIFY(liveBlue > 0);
        QVERIFY(exportRed > 0);
        QVERIFY(exportBlue > 0);
    }

    void outlinedTextItemPathPreservesMultilineTextOnFlatPath()
    {
        const QColor background(Qt::transparent);
        auto render = [background]() {
            OutlinedTextItem item;
            item.setWidth(260);
            item.setHeight(120);
            item.setText(QStringLiteral("HELLO\nWEAVER!"));
            item.setPixelSize(34);
            item.setLetterSpacing(1.5);
            item.setColor(Qt::black);
            item.setPathEnabled(true);
            item.setPathPoints(QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});

            QImage image(260, 120, QImage::Format_ARGB32_Premultiplied);
            image.fill(background);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };

        const QRect pathBounds = visibleBounds(render(), background);
        QVERIFY(!pathBounds.isEmpty());
        QVERIFY2(pathBounds.height() > 50, qPrintable(QStringLiteral("path bounds %1x%2").arg(pathBounds.width()).arg(pathBounds.height())));
        QVERIFY2(pathBounds.width() > 120 && pathBounds.width() < 260, qPrintable(QStringLiteral("path bounds %1x%2").arg(pathBounds.width()).arg(pathBounds.height())));
    }

    void outlinedTextItemDefaultFlatPathKeepsPlainVisibleBounds()
    {
        const QColor background(Qt::transparent);
        auto render = [background](bool pathEnabled) {
            OutlinedTextItem item;
            item.setWidth(150);
            item.setHeight(120);
            item.setText(QStringLiteral("HELLO WEAVER!"));
            item.setPixelSize(34);
            item.setLetterSpacing(1);
            item.setColor(Qt::black);
            item.setHorizontalAlignment(Qt::AlignHCenter);
            item.setPathEnabled(pathEnabled);
            item.setPathPoints(QVariantList{QVariantList{0.0, 0.5}, QVariantList{0.5, 0.5}, QVariantList{1.0, 0.5}});

            QImage image(150, 120, QImage::Format_ARGB32_Premultiplied);
            image.fill(background);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };

        const QImage plain = render(false);
        const QImage path = render(true);
        const QRect plainBounds = visibleBounds(plain, background);
        const QRect pathBounds = visibleBounds(path, background);
        QVERIFY(!plainBounds.isEmpty());
        QVERIFY(!pathBounds.isEmpty());
        QVERIFY2(!imagesDiffer(plain, path), "default flat path must render exactly like plain text");
        QVERIFY2(std::abs(plainBounds.left() - pathBounds.left()) <= 3, qPrintable(QStringLiteral("plain=%1 path=%2").arg(plainBounds.left()).arg(pathBounds.left())));
        QVERIFY2(std::abs(plainBounds.top() - pathBounds.top()) <= 3, qPrintable(QStringLiteral("plain=%1 path=%2").arg(plainBounds.top()).arg(pathBounds.top())));
        QVERIFY2(std::abs(plainBounds.width() - pathBounds.width()) <= 6, qPrintable(QStringLiteral("plain=%1 path=%2").arg(plainBounds.width()).arg(pathBounds.width())));
        QVERIFY2(std::abs(plainBounds.height() - pathBounds.height()) <= 6, qPrintable(QStringLiteral("plain=%1 path=%2").arg(plainBounds.height()).arg(pathBounds.height())));
    }

    void outlinedTextItemPathUsesVisualWrappedLinesOnFlatPath()
    {
        const QColor background(Qt::transparent);
        OutlinedTextItem item;
        item.setWidth(150);
        item.setHeight(120);
        item.setText(QStringLiteral("HELLO WEAVER!"));
        item.setPixelSize(34);
        item.setLetterSpacing(1.5);
        item.setColor(Qt::black);
        item.setPathEnabled(true);
        item.setPathPoints(QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});

        QImage image(150, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(background);
        QPainter painter(&image);
        item.paint(&painter);

        const QRect pathBounds = visibleBounds(image, background);
        QVERIFY(!pathBounds.isEmpty());
        QVERIFY2(pathBounds.height() > 50, qPrintable(QStringLiteral("path bounds %1x%2").arg(pathBounds.width()).arg(pathBounds.height())));
        QVERIFY2(pathBounds.width() < 150, qPrintable(QStringLiteral("path bounds %1x%2").arg(pathBounds.width()).arg(pathBounds.height())));
    }

    void outlinedTextItemPathCentersLinesLikeTypeX()
    {
        const QColor background(Qt::transparent);
        auto render = [background](int alignment) {
            OutlinedTextItem item;
            item.setWidth(240);
            item.setHeight(100);
            item.setText(QStringLiteral("SHORT"));
            item.setPixelSize(30);
            item.setColor(Qt::black);
            item.setHorizontalAlignment(alignment);
            item.setPathEnabled(true);
            item.setPathPoints(QVariantList{QVariantList{0.0, 0.60}, QVariantList{1.0, 0.60}});

            QImage image(240, 100, QImage::Format_ARGB32_Premultiplied);
            image.fill(background);
            QPainter painter(&image);
            item.paint(&painter);
            return visibleBounds(image, background);
        };

        const QRect left = render(Qt::AlignLeft);
        const QRect center = render(Qt::AlignHCenter);
        const QRect right = render(Qt::AlignRight);
        QVERIFY(!left.isEmpty());
        QVERIFY(!center.isEmpty());
        QVERIFY(!right.isEmpty());
        QVERIFY2(std::abs(left.center().x() - center.center().x()) <= 3, qPrintable(QStringLiteral("left=%1 center=%2").arg(left.center().x()).arg(center.center().x())));
        QVERIFY2(std::abs(right.center().x() - center.center().x()) <= 3, qPrintable(QStringLiteral("center=%1 right=%2").arg(center.center().x()).arg(right.center().x())));
    }

    void outlinedTextItemPathPointDoesNotStretchHorizontalSpacing()
    {
        const QColor background(Qt::transparent);
        auto render = [background](const QVariantList& points) {
            OutlinedTextItem item;
            item.setWidth(240);
            item.setHeight(120);
            item.setText(QStringLiteral("EVEN SPACING"));
            item.setPixelSize(30);
            item.setColor(Qt::black);
            item.setPathEnabled(true);
            item.setPathPoints(points);

            QImage image(240, 120, QImage::Format_ARGB32_Premultiplied);
            image.fill(background);
            QPainter painter(&image);
            item.paint(&painter);
            return visibleBounds(image, background);
        };

        const QRect flat = render(QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});
        const QRect curved = render(QVariantList{QVariantList{0.0, 0.65}, QVariantList{0.1, 0.20}, QVariantList{1.0, 0.65}});
        QVERIFY(!flat.isEmpty());
        QVERIFY(!curved.isEmpty());
        QVERIFY2(std::abs(flat.left() - curved.left()) <= 16, qPrintable(QStringLiteral("flat=%1 curved=%2").arg(flat.left()).arg(curved.left())));
        QVERIFY2(std::abs(flat.right() - curved.right()) <= 24, qPrintable(QStringLiteral("flat=%1 curved=%2").arg(flat.right()).arg(curved.right())));
        QVERIFY2(std::abs(flat.width() - curved.width()) <= 32, qPrintable(QStringLiteral("flat=%1 curved=%2").arg(flat.width()).arg(curved.width())));
    }

    void outlinedTextItemSamplesStraightPathByDistance()
    {
        OutlinedTextItem item;
        item.setPathEnabled(true);
        item.setPathPoints(QVariantList{QVariantList{0.0, 0.8}, QVariantList{0.2, 0.2}, QVariantList{1.0, 0.8}});

        const QPointF sample = item.pathBaselinePointForTesting(100.0, 200.0, 100.0);
        QVERIFY(std::abs(sample.x() - 66.1) < 0.2);
        QVERIFY(std::abs(sample.y() - 29.8) < 0.2);
    }

    void outlinedTextItemWrapsWithinOutlineInset()
    {
        QFile source(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../src/ui/OutlinedTextItem.cpp"));
        QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString code = QString::fromUtf8(source.readAll());
        QFile layoutSource(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../src/render/RenderTextLayout.cpp"));
        QVERIFY(layoutSource.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString layoutCode = QString::fromUtf8(layoutSource.readAll());

        QVERIFY(code.contains(QStringLiteral("gaussianBlurred")));
        QVERIFY(code.contains(QStringLiteral("blurCacheKey")));
        QVERIFY(code.contains(QStringLiteral("blurSize_")));
        QVERIFY(code.contains(QStringLiteral("const qreal layoutWidth = std::max<qreal>(1.0, width() / scale);")));
        QVERIFY(code.contains(QStringLiteral("target.scale(scale, scale);")));
        QVERIFY(layoutCode.contains(QStringLiteral("const qreal paintWidth = std::max<qreal>(1.0, options.width - options.inset * 2.0);")));
        QVERIFY(layoutCode.contains(QStringLiteral("line.setLineWidth(paintWidth);")));
        QVERIFY(layoutCode.contains(QStringLiteral("options.height - options.inset * 2.0 - blockHeight")));
        QVERIFY(layoutCode.contains(QStringLiteral("qreal x = options.inset;")));
        QVERIFY(!layoutCode.contains(QStringLiteral("line.setLineWidth(options.width);")));
    }

    void outlinedTextItemBlurCacheInvalidatesOnFractionalResize()
    {
        auto configure = [](OutlinedTextItem& item, qreal width) {
            item.setWidth(width);
            item.setHeight(90.1);
            item.setText(QStringLiteral("MMMMMMMMMMMMMMMMMMMMMMMM"));
            item.setPixelSize(24);
            item.setLetterSpacing(0.37);
            item.setColor(Qt::black);
            item.setBlurSize(5);
        };
        auto render = [&](OutlinedTextItem& item, qreal width) {
            configure(item, width);
            QImage image(180, 120, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            item.paint(&painter);
            return image;
        };
        auto freshRender = [&](qreal width) {
            OutlinedTextItem item;
            return render(item, width);
        };

        qreal firstWidth = 0.0;
        qreal secondWidth = 0.0;
        QImage firstFresh;
        QImage secondFresh;
        for (int width = 48; width < 160 && firstWidth == 0.0; ++width) {
            const qreal a = width + 0.1;
            const qreal b = width + 0.9;
            QImage aImage = freshRender(a);
            QImage bImage = freshRender(b);
            if (imagesDiffer(aImage, bImage)) {
                firstWidth = a;
                secondWidth = b;
                firstFresh = aImage;
                secondFresh = bImage;
            }
        }
        QVERIFY2(firstWidth > 0.0, "test setup did not find a same-ceil fractional width that changes rendered text");

        OutlinedTextItem reusedItem;
        const QImage firstReused = render(reusedItem, firstWidth);
        const QImage secondReused = render(reusedItem, secondWidth);

        QVERIFY(!imagesDiffer(firstReused, firstFresh));
        QVERIFY(!imagesDiffer(secondReused, secondFresh));
        QVERIFY(imagesDiffer(firstReused, secondReused));
    }

    void outlinedTextItemFitsStrokeBoundsBeforePainting()
    {
        QFile source(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../src/ui/OutlinedTextItem.cpp"));
        QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString code = QString::fromUtf8(source.readAll());

        QVERIFY(code.contains(QStringLiteral("paintedBounds = paintedBounds.united(stroker.createStroke(path).boundingRect());")));
        QVERIFY(code.contains(QStringLiteral("if (paintedBounds.left() < 0.0) dx = -paintedBounds.left();")));
        QVERIFY(code.contains(QStringLiteral("if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();")));
        QVERIFY(code.contains(QStringLiteral("path.translate(dx, dy);")));
    }

    void qmlHasEightPerspectiveHandlesAndQPainterUsesFontResolver()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString qmlSource = ::qmlSource();

        for (const QString& handle : {QStringLiteral("nw"), QStringLiteral("n"), QStringLiteral("ne"), QStringLiteral("e"), QStringLiteral("se"), QStringLiteral("s"), QStringLiteral("sw"), QStringLiteral("w")}) {
            QVERIFY2(qmlSource.contains(QStringLiteral("name: \"") + handle + QStringLiteral("\"")), qPrintable(handle));
        }
        QVERIFY(qmlSource.contains(QStringLiteral("perspectiveOffset")));
        QVERIFY(!qmlSource.contains(QStringLiteral("perspectiveHandleOffset")));
        QVERIFY(!qmlSource.contains(QStringLiteral("visible: !canvas.effectsPreviewDisplayable || Editor.editingText")));

        QFile render(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../src/render/RenderGraph.cpp"));
        QVERIFY(render.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString renderSource = QString::fromUtf8(render.readAll());
        QVERIFY(renderSource.contains(QStringLiteral("return resolveFont(font).font;")));
        QVERIFY(renderSource.contains(QStringLiteral("font.setBold(box.style.bold)")));
        QVERIFY(!renderSource.contains(QStringLiteral("SkFontMgr_New_Custom_Directory")));
    }

    void qmlUsesRawOverlayAndTopLayerFirstList()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("id: rawOverlayImage")));
        QVERIFY(source.contains(QStringLiteral("opacity: 0.45")));
        QVERIFY(!source.contains(QStringLiteral("Editor.rawVisible && Editor.rawPageUrl.toString().length > 0 ? Editor.rawPageUrl : Editor.currentPageUrl")));
        QVERIFY(source.contains(QStringLiteral("model: Editor.layers")));
        QVERIFY(source.contains(QStringLiteral("text: qsTr(\"Up\"); enabled: Editor.selectedIndex >= 0 && Editor.selectedIndex < Editor.boxes.length - 1; onClicked: Editor.moveLayer(Editor.selectedIndex + 1)")));
        QVERIFY(source.contains(QStringLiteral("text: qsTr(\"Down\"); enabled: Editor.selectedIndex > 0; onClicked: Editor.moveLayer(Editor.selectedIndex - 1)")));
    }
};

QTEST_MAIN(QtSmokeTests)

#include "qt_smoke_tests.moc"
