#include "app/EditorController.h"
#include "fonts/FontResolver.h"
#include "qt_test_helpers.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace textfx;
using namespace textfx::test;

namespace {
void writeJsonFile(const QString &path, const QJsonObject &object) {
  QFileInfo info(path);
  QDir().mkpath(info.absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write(QJsonDocument(object).toJson()),
           QJsonDocument(object).toJson().size());
}
} // namespace

class EditorControllerWorkflowTests final : public QObject {
  Q_OBJECT

private slots:
  void noProjectDisablesProjectActions() {
    EditorController editor;

    QVERIFY(!editor.hasProject());
    QVERIFY(editor.actionEnabled(QStringLiteral("new")));
    QVERIFY(editor.actionEnabled(QStringLiteral("open")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("save")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
  }

  void commandRefreshesDocumentState() {
    EditorController editor;
    QSignalSpy changed(&editor, &EditorController::documentChanged);

    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(editor.selectedIndex(), 0);
    QVERIFY(editor.dirty());
    QVERIFY(changed.count() >= 1);
  }

  void createdBoxesAreEmptyExactDragWithTypeXMinimum() {
    EditorController editor;
    editor.newDocument();

    editor.createTextBox(10, 20, 13, 14);
    editor.createTextBox(30, 40, 1, 2);

    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QString());
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("w")).toDouble(),
             13.0);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("h")).toDouble(),
             14.0);
  }

  void boxesExposeRequestedAndResolvedFontFamilies() {
    auto expectedResolvedFamily = [](const QString &family) {
      QFont font(family);
      return resolveFont(font).font.family();
    };

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    const QString missingFamily =
        QStringLiteral("TextFX Missing Font For Resolver Test");
    editor.setSelectedFontFamily(missingFamily);

    QVariantMap box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), missingFamily);
    QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(),
             expectedResolvedFamily(missingFamily));
    QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() !=
            missingFamily);

    const QString beatDown = QStringLiteral("000 BeatDownBB [TeddyBear]");
    QFont beatDownFont(beatDown);
    const ResolvedFont beatDownResolution = resolveFont(beatDownFont);
    if (beatDownResolution.requestedAvailable) {
      QCOMPARE(QFontInfo(beatDownResolution.font).family(), beatDown);

      editor.setSelectedFontFamily(beatDown);
      box = editor.boxes().at(0).toMap();

      QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), beatDown);
      QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(),
               beatDownResolution.font.family());
      QVERIFY(!box.value(QStringLiteral("resolvedFontFamily"))
                   .toString()
                   .isEmpty());
    }
  }

  void activeTextEditingDefersModelResetUntilEditEnds() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.beginTextEdit();

    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

    editor.updateSelectedText(QStringLiteral("abc"));

    QCOMPARE(documentChanged.count(), 0);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("abc"));
    QVERIFY(editor.dirty());
    QVERIFY(stateChanged.count() >= 1);

    editor.endTextEdit();

    QCOMPARE(documentChanged.count(), 1);
  }

  void copyPastePreservesCopiedBox() {
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
    QCOMPARE(pasted.value(QStringLiteral("text")).toString(),
             QStringLiteral("Copied text"));
    QVERIFY(!pasted.value(QStringLiteral("text"))
                 .toString()
                 .startsWith(QLatin1Char('{')));
    QCOMPARE(pasted.value(QStringLiteral("x")).toDouble(), 26.0);
    QCOMPARE(pasted.value(QStringLiteral("y")).toDouble(), 36.0);
    QCOMPARE(pasted.value(QStringLiteral("rotation")).toDouble(), 12.5);
    QVERIFY(pasted.value(QStringLiteral("perspective")).toBool());
    const auto pastedNe =
        pasted.value(QStringLiteral("perspectiveNe")).toList();
    QCOMPARE(pastedNe.at(0).toDouble(), 0.9);
    QCOMPARE(pastedNe.at(1).toDouble(), 0.1);
    QVERIFY(pasted.value(QStringLiteral("path")).toBool());
    QCOMPARE(pasted.value(QStringLiteral("pathMode")).toInt(), 1);
    const auto pastedPathPoint =
        pasted.value(QStringLiteral("pathPoints")).toList().at(0).toList();
    QCOMPARE(pastedPathPoint.at(0).toDouble(), 0.2);
    QCOMPARE(pastedPathPoint.at(1).toDouble(), 0.8);
    QCOMPARE(editor.selectedIndex(), 1);
    QVERIFY(editor.dirty());

    QGuiApplication::clipboard()->setText(QStringLiteral("Plain text"));
    editor.pasteBox();

    QCOMPARE(
        editor.boxes().at(2).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Plain text"));
  }
  void openProjectLoadsFixtureImagesAndEmptyMissingData() {
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

  void openProjectUrlUsesQtLocalFileConversion() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath =
        dir.filePath(QStringLiteral("project with spaces"));
    QVERIFY(QDir().mkpath(projectPath));
    touch(projectPath + QStringLiteral("/page1.png"));

    EditorController editor;
    editor.openProjectUrl(QUrl::fromLocalFile(projectPath));

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.pageCount(), 1);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QVERIFY(editor.currentPageUrl().isLocalFile());
  }

  void openProjectUrlRejectsNonLocalUrls() {
    EditorController editor;
    editor.openProjectUrl(
        QUrl(QStringLiteral("https://example.invalid/project")));

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(),
             QStringLiteral("Only local project folders can be opened."));
  }

  void newProjectCreatesCanonicalStructure() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));

    EditorController editor;
    editor.newProject(projectPath);

    QVERIFY(editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Cleaned")));
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Raw")));
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Typeset")));
    QVERIFY(QFile::exists(projectPath + QStringLiteral("/Texts.txt")));
    QCOMPARE(editor.pageCount(), 0);
    QCOMPARE(editor.notification(), QStringLiteral("New project created"));
  }

  void newProjectRejectsDirectoryAtTextsPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));
    QVERIFY(QDir().mkpath(projectPath + QStringLiteral("/Texts.txt")));

    EditorController editor;
    editor.newProject(projectPath);

    QVERIFY(!editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Texts.txt")));
    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not create project:")));
    QVERIFY(editor.notification().contains(QStringLiteral("Texts.txt")));
  }

  void newProjectRejectsUnreadableTextsFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));
    QVERIFY(QDir().mkpath(projectPath));

    const QString textsPath = projectPath + QStringLiteral("/Texts.txt");
    QFile texts(textsPath);
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page.png]\nExisting text\n");
    texts.close();
    QVERIFY(QFile::setPermissions(textsPath, QFileDevice::WriteOwner));

    QFile readProbe(textsPath);
    if (readProbe.open(QIODevice::ReadOnly)) {
      readProbe.close();
      QFile::setPermissions(textsPath, QFileDevice::ReadOwner |
                                           QFileDevice::WriteOwner);
      QSKIP("Unreadable regular files are still readable in this environment.");
    }

    EditorController editor;
    editor.newProject(projectPath);

    QFile::setPermissions(textsPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QFile preserved(textsPath);
    QVERIFY(preserved.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(preserved.readAll()),
             QStringLiteral("[page.png]\nExisting text\n"));
    QVERIFY(!editor.hasProject());
    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not create project:")));
    QVERIFY(editor.notification().contains(QStringLiteral("Texts.txt")));
  }

  void openProjectRejectsUnreadableTextsFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page.png]\nExisting text\n");
    texts.close();
    QVERIFY(QFile::setPermissions(texts.fileName(), QFileDevice::WriteOwner));

    QFile readProbe(texts.fileName());
    if (readProbe.open(QIODevice::ReadOnly)) {
      readProbe.close();
      QFile::setPermissions(texts.fileName(), QFileDevice::ReadOwner |
                                                  QFileDevice::WriteOwner);
      QSKIP("Unreadable regular files are still readable in this environment.");
    }

    EditorController editor;
    editor.openProject(dir.path());

    QFile::setPermissions(texts.fileName(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(), QStringLiteral("Could not open Texts.txt."));
  }

  void openProjectRejectsDirectoryAtTextsPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Texts.txt")));

    EditorController editor;
    editor.openProject(dir.path());

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(), QStringLiteral("Could not open Texts.txt."));
  }

  void newProjectUrlUsesQtLocalFileConversion() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath =
        dir.filePath(QStringLiteral("project with spaces"));

    EditorController editor;
    editor.newProjectUrl(QUrl::fromLocalFile(projectPath));

    QVERIFY(editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Cleaned")));
    QVERIFY(QFile::exists(projectPath + QStringLiteral("/Texts.txt")));
  }

  void openProjectLoadsCanonicalCleanedImagesAndTexts() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Cleaned")));
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("Cleaned/page2.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page2.png]\nCanonical text\n");
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.pageCount(), 1);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QCOMPARE(editor.pageTexts(),
             QStringList({QStringLiteral("Canonical text")}));
  }

  void navigationAutosavesAndReloadsCurrentPage() {
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
    const auto savedPath = dir.filePath(QStringLiteral(".textfx/page1.json"));
    QVERIFY(QFile::exists(savedPath));
    QCOMPARE(
        readJson(savedPath).value(QStringLiteral("boxes")).toArray().size(), 1);

    editor.previousPage();

    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Saved on navigation"));
  }

  void directPageSelectorAutosavesWithoutExportingPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    QCOMPARE(editor.pageLabels(),
             QStringList({QStringLiteral("1 - page1.png"),
                          QStringLiteral("2 - page2.png")}));
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Jump saved"));

    editor.goToPage(1);

    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".textfx/page1.json"))));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("Typeset/page1.png"))));
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
  }

  void openingProjectAbortsWhenCurrentAutosaveFails() {
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

    QVERIFY(QFile(first.filePath(QStringLiteral(".textfx")))
                .open(QIODevice::WriteOnly));
    editor.openProject(second.path());

    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QVERIFY(editor.notification().contains(QStringLiteral(".textfx")));
  }

  void saveWritesJsonAndCurrentExportedPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.updateSelectedText(QStringLiteral("Saved page"));
    const auto output = dir.filePath(QStringLiteral("Typeset/page1.png"));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Typeset")));
    QFile stale(output);
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("stale export");
    stale.close();
    editor.save();

    QVERIFY(!editor.dirty());
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".textfx/page1.json"))));
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);

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
    QVERIFY(editor.notification().contains(
        QStringLiteral("Saved boxes and exported PNG to")));
  }

  void saveReportsBoxSaveFailure() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    QVERIFY(QFile(dir.filePath(QStringLiteral(".textfx")))
                .open(QIODevice::WriteOnly));

    editor.save();

    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not save boxes:")));
  }

  void saveAllExportsMultiplePagesWithSavedBoxes() {
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
    QFile emptyPage(dir.filePath(QStringLiteral(".textfx/page3.json")));
    QVERIFY(emptyPage.open(QIODevice::WriteOnly | QIODevice::Text));
    emptyPage.write(
        R"({"format":"textfx.page-boxes.v1","page":"page3.png","boxes":[]})");
    emptyPage.close();
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Typeset")));
    QFile stale(dir.filePath(QStringLiteral("Typeset/page3.png")));
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("stale export");
    stale.close();
    QFile staleMissingJson(dir.filePath(QStringLiteral("Typeset/page4.png")));
    QVERIFY(staleMissingJson.open(QIODevice::WriteOnly));
    staleMissingJson.write("stale export without json");
    staleMissingJson.close();
    editor.saveAll();

    QVERIFY(!editor.dirty());
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page2.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page3.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             0);

    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page1.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page2.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page3.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page4.png"))),
             qPrintable(editor.notification()));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral(".textfx/page4.json"))));
    QCOMPARE(editor.notification(), QStringLiteral("Exported 4 page(s)."));
  }

  void saveAllReportsPageExportFailures() {
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
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page2.png"))),
             qPrintable(editor.notification()));
  }
  void liveInteractionDefersPreviewButRefreshesModel() {
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

  void gradientPathInteractionDoesNotGeneratePreviewArtifact() {
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

  void transformChangesPersistOnSave() {
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

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 15.0);
    QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 27.0);
    QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 110.0);
    QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 70.0);
    QCOMPARE(box.value(QStringLiteral("rotation_degrees")).toDouble(), 15.0);
  }
  void selectedBoundsUseTypeXMinimum() {
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
  void documentCoordinatesPersistAsCreated() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(12.5, 34.25, 123.75, 56.5);
    editor.moveSelected(3.5, -4.25);
    editor.resizeSelected(10.25, 20.5);
    editor.save();

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 16.0);
    QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 30.0);
    QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 134.0);
    QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 77.0);
  }

  void multilineEditUpdatesDocumentState() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);
    editor.beginTextEdit();
    QVERIFY(editor.editingText());
    editor.updateSelectedText(QStringLiteral("Line 1\nLine 2"));
    editor.endTextEdit();

    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Line 1\nLine 2"));
    QVERIFY(!editor.editingText());
  }

  void selectedBoxCanBeDeselectedWithoutDeletion() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.selectBox(-1);

    QCOMPARE(editor.selectedIndex(), -1);
    QCOMPARE(editor.boxes().size(), 1);
  }

  void textPropertiesUpdateSelectedBox() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.setSelectedFontFamily(
        QStringLiteral("TextFX Custom Missing Family"));
    editor.setSelectedFontSize(42);
    editor.setSelectedTextColor(QStringLiteral("#ff00aa"));
    editor.setSelectedBold(true);
    editor.setSelectedItalic(true);
    editor.setSelectedUppercase(true);
    editor.setSelectedAlignment(1);
    editor.setSelectedLineSpacing(12);
    editor.setSelectedLetterSpacing(3);

    const auto box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("TextFX Custom Missing Family"));
    QVERIFY(box.contains(QStringLiteral("resolvedFontFamily")));
    QVERIFY(
        !box.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty());
    QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() !=
            box.value(QStringLiteral("fontFamily")).toString());
    QCOMPARE(box.value(QStringLiteral("fontSize")).toInt(), 42);
    QCOMPARE(box.value(QStringLiteral("color")).toString(),
             QStringLiteral("ff00aaff"));
    QVERIFY(box.value(QStringLiteral("bold")).toBool());
    QVERIFY(box.value(QStringLiteral("italic")).toBool());
    QVERIFY(box.value(QStringLiteral("uppercase")).toBool());
    QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("lineSpacing")).toInt(), 12);
    QCOMPARE(box.value(QStringLiteral("letterSpacing")).toInt(), 3);
    QVERIFY(editor.dirty());
  }
  void effectControlsUpdateSelectedBox() {
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
    QCOMPARE(box.value(QStringLiteral("outlineColor")).toString(),
             QStringLiteral("112233ff"));
    QCOMPARE(box.value(QStringLiteral("outlineSize")).toInt(), 7);
    QVERIFY(box.value(QStringLiteral("blur")).toBool());
    QCOMPARE(box.value(QStringLiteral("blurSize")).toInt(), 8);
    QVERIFY(box.value(QStringLiteral("shadow")).toBool());
    QCOMPARE(box.value(QStringLiteral("shadowColor")).toString(),
             QStringLiteral("445566ff"));
    QCOMPARE(box.value(QStringLiteral("shadowOffsetX")).toInt(), -9);
    QCOMPARE(box.value(QStringLiteral("shadowOffsetY")).toInt(), 10);
    QCOMPARE(box.value(QStringLiteral("shadowBlurSize")).toInt(), 11);
    QVERIFY(box.value(QStringLiteral("gradient")).toBool());
    QCOMPARE(box.value(QStringLiteral("gradientDirection")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("gradientColorA")).toString(),
             QStringLiteral("abcdefff"));
    QCOMPARE(box.value(QStringLiteral("gradientColorB")).toString(),
             QStringLiteral("123456ff"));
    QVERIFY(box.value(QStringLiteral("perspective")).toBool());
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNe")).toList().at(0).toDouble(),
        22.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNe")).toList().at(1).toDouble(),
        6.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNw")).toList().at(1).toDouble(),
        6.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSe")).toList().at(0).toDouble(),
        22.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSe")).toList().at(1).toDouble(),
        9.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSw")).toList().at(0).toDouble(),
        1.0);
    QVERIFY(box.value(QStringLiteral("path")).toBool());
    QCOMPARE(box.value(QStringLiteral("pathMode")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("pathPoints")).toList().size(), 4);
    editor.setPathHandle(0, -0.3, 1.4);
    const auto clampedPath = editor.boxes()
                                 .at(0)
                                 .toMap()
                                 .value(QStringLiteral("pathPoints"))
                                 .toList()
                                 .at(0)
                                 .toList();
    QCOMPARE(clampedPath.at(0).toDouble(), 0.0);
    QCOMPARE(clampedPath.at(1).toDouble(), 1.0);
  }

  void pathPointsInsertIntoLongestSegmentInOrder() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(0, 0, 100, 40);
    editor.setSelectedPathEnabled(true);

    editor.addSelectedPathPoint();
    auto points = editor.boxes()
                      .at(0)
                      .toMap()
                      .value(QStringLiteral("pathPoints"))
                      .toList();
    QCOMPARE(points.size(), 4);
    QCOMPARE(points.at(1).toList().at(0).toDouble(), 0.25);
    QCOMPARE(points.at(1).toList().at(1).toDouble(), 0.5);

    editor.addSelectedPathPoint();
    points = editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("pathPoints"))
                 .toList();
    QCOMPARE(points.size(), 5);
    QCOMPARE(points.at(3).toList().at(0).toDouble(), 0.75);
    QCOMPARE(points.at(3).toList().at(1).toDouble(), 0.5);
  }

  void straightPathHandleMovementMatchesTypeXFreeDrag() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(0, 0, 100, 40);
    editor.setSelectedPathEnabled(true);
    editor.setSelectedPathMode(0);
    editor.setPathHandle(1, 0.25, 0.9);

    auto point = editor.boxes()
                     .at(0)
                     .toMap()
                     .value(QStringLiteral("pathPoints"))
                     .toList()
                     .at(1)
                     .toList();
    QCOMPARE(point.at(0).toDouble(), 0.25);
    QCOMPARE(point.at(1).toDouble(), 0.9);

    editor.setPathHandle(0, 0.8, 0.2);
    point = editor.boxes()
                .at(0)
                .toMap()
                .value(QStringLiteral("pathPoints"))
                .toList()
                .at(0)
                .toList();
    QCOMPARE(point.at(0).toDouble(), 0.8);
    QCOMPARE(point.at(1).toDouble(), 0.2);

    editor.setSelectedPathMode(1);
    editor.setPathHandle(1, 0.25, 0.9);
    point = editor.boxes()
                .at(0)
                .toMap()
                .value(QStringLiteral("pathPoints"))
                .toList()
                .at(1)
                .toList();
    QCOMPARE(point.at(0).toDouble(), 0.25);
    QCOMPARE(point.at(1).toDouble(), 0.9);
  }

  void effectsPersistAsTextFXFields() {
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

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QVERIFY(box.value(QStringLiteral("uppercase")).toBool());
    QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 2);
    QCOMPARE(box.value(QStringLiteral("line_spacing")).toInt(), 9);
    QCOMPARE(box.value(QStringLiteral("letter_spacing")).toInt(), 4);
    QCOMPARE(box.value(QStringLiteral("shadow_blur_size")).toInt(), 12);
    QCOMPARE(box.value(QStringLiteral("gradient_direction")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("gradient_color_a")).toString(),
             QStringLiteral("ff0000ff"));
    QCOMPARE(box.value(QStringLiteral("gradient_color_b")).toString(),
             QStringLiteral("0000ffff"));
    QVERIFY(box.value(QStringLiteral("perspective_offsets"))
                .toObject()
                .contains(QStringLiteral("nw")));
    QCOMPARE(box.value(QStringLiteral("perspective_offsets"))
                 .toObject()
                 .value(QStringLiteral("nw"))
                 .toArray()
                 .at(0)
                 .toDouble(),
             10.0);
    QCOMPARE(box.value(QStringLiteral("perspective_offsets"))
                 .toObject()
                 .value(QStringLiteral("nw"))
                 .toArray()
                 .at(1)
                 .toDouble(),
             20.0);
    QCOMPARE(box.value(QStringLiteral("path_points")).toArray().size(), 3);
    QCOMPARE(box.value(QStringLiteral("path_mode")).toInt(), 1);
  }
  void pageTextsLoadAndApplyToSelectedBox() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page1.png]\nFirst\nSecond\n[page2.png]\nOther\n");
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());
    QCOMPARE(editor.pageTexts(),
             QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
    QVERIFY(!editor.applyNextPageText());
    editor.createTextBox(1, 2, 80, 40);

    QVERIFY(editor.applyNextPageText());
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("First"));
    QVERIFY(editor.applyNextPageText());
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Second"));
    QVERIFY(!editor.applyNextPageText());
    editor.nextPage();
    QCOMPARE(editor.pageTexts(), QStringList({QStringLiteral("Other")}));
  }

  void textPresetsApplyAddUpdateRenameAndDelete() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    QVERIFY(editor.presets().empty());
    QCOMPARE(editor.selectedPresetIndex(), -1);
    editor.createTextBox(1, 2, 80, 40);

    QVERIFY(!editor.applySelectedPreset());

    editor.setSelectedFontFamily(QStringLiteral("Inter"));
    editor.setSelectedFontSize(31);
    editor.setSelectedBold(true);
    QVERIFY(editor.addPreset(QStringLiteral("Narration")));
    QVERIFY(
        QFile::exists(dir.filePath(QStringLiteral(".textfx/presets.json"))));
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("name"))
                 .toString(),
             QStringLiteral("Narration"));

    editor.setSelectedFontSize(44);
    QVERIFY(editor.updateSelectedPreset());
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("fontSize"))
                 .toInt(),
             44);

    QVERIFY(editor.renameSelectedPreset(QStringLiteral("Caption")));
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("name"))
                 .toString(),
             QStringLiteral("Caption"));
    QVERIFY(editor.deleteSelectedPreset());
    QVERIFY(editor.presets().empty());
    QCOMPARE(editor.selectedPresetIndex(), -1);
  }

  void loadedUserPresetCanBeAppliedUpdatedRenamedAndDeleted() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    writeJsonFile(
        dir.filePath(QStringLiteral(".textfx/presets.json")),
        QJsonObject{{QStringLiteral("format"),
                     QStringLiteral("textfx.text-presets.v1")},
                    {QStringLiteral("presets"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("name"), QStringLiteral("Narration")},
                         {QStringLiteral("properties"),
                          QJsonObject{{QStringLiteral("font_family"),
                                       QStringLiteral("Inter")},
                                      {QStringLiteral("font_size"), 24}}}}}}});

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.selectPreset(0);

    QVERIFY(editor.applySelectedPreset());
    QCOMPARE(editor.boxes()
                 .first()
                 .toMap()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Inter"));
    editor.setSelectedFontFamily(QStringLiteral("Custom Bubble"));
    QVERIFY(editor.updateSelectedPreset());

    QCOMPARE(editor.presets()
                 .first()
                 .toMap()
                  .value(QStringLiteral("name"))
                  .toString(),
             QStringLiteral("Narration"));
    QCOMPARE(editor.presets()
                 .first()
                 .toMap()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Custom Bubble"));
    QVERIFY(editor.renameSelectedPreset(QStringLiteral("Caption")));
    const auto saved =
        readJson(dir.filePath(QStringLiteral(".textfx/presets.json")))
            .value(QStringLiteral("presets"))
            .toArray();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Caption"));
    QVERIFY(editor.deleteSelectedPreset());
    QVERIFY(editor.presets().empty());
  }
};

QTEST_MAIN(EditorControllerWorkflowTests)

#include "editor_controller_workflow_tests.moc"
