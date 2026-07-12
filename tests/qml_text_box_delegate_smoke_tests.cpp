#include "app/controllers/EditorController.h"
#include "qt_test_helpers.h"

#include <QClipboard>
#include <QColor>
#include <QMetaObject>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QUrl>

#include <cmath>
#include <limits>

using namespace textfx;
using namespace textfx::test;

namespace {

class ResizeHandleFakeRootWindow final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariant palette READ palette CONSTANT)

public:
  QVariant palette() const {
    QVariantMap value;
    value.insert(QStringLiteral("highlight"), QStringLiteral("#448aff"));
    return value;
  }

  Q_INVOKABLE QPointF visualHandlePosition(const QVariant &, const QString &, qreal,
                                           qreal) {
    return {10, 12};
  }
  Q_INVOKABLE qreal handleSize() const { return 8; }
  Q_INVOKABLE void beginResizeDrag(QObject *, const QString &, qreal, qreal) {
    ++resizeBeginCount;
  }
  Q_INVOKABLE void beginPerspectiveDrag(QObject *, const QString &, qreal, qreal) {
    ++perspectiveBeginCount;
  }
  Q_INVOKABLE void updateResizeDrag(qreal, qreal) { ++resizeUpdateCount; }
  Q_INVOKABLE void updatePerspectiveDrag(qreal, qreal) {
    ++perspectiveUpdateCount;
  }
  Q_INVOKABLE void endResizeDrag(bool commit) {
    ++resizeEndCount;
    resizeCommit = commit;
  }
  Q_INVOKABLE void endPerspectiveDrag(bool commit) {
    ++perspectiveEndCount;
    perspectiveCommit = commit;
  }

  int resizeBeginCount = 0;
  int perspectiveBeginCount = 0;
  int resizeUpdateCount = 0;
  int perspectiveUpdateCount = 0;
  int resizeEndCount = 0;
  int perspectiveEndCount = 0;
  bool resizeCommit = false;
  bool perspectiveCommit = false;
};

class ResizeHandleFakeEditor final : public QObject {
  Q_OBJECT

public:
  Q_INVOKABLE void selectBox(int index) { selectedIndex = index; }
  Q_INVOKABLE void beginInteraction() { ++beginCount; }
  Q_INVOKABLE void endInteraction() { ++endCount; }

  int selectedIndex = -1;
  int beginCount = 0;
  int endCount = 0;
};

class ResizeHandleFakeBox final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *rootWindow READ rootWindow CONSTANT)
  Q_PROPERTY(QObject *editorRef READ editorRef CONSTANT)
  Q_PROPERTY(QVariant boxModel READ boxModel CONSTANT)
  Q_PROPERTY(bool selected READ selected CONSTANT)
  Q_PROPERTY(bool perspectiveActive READ perspectiveActive WRITE setPerspectiveActive)
  Q_PROPERTY(qreal width READ width CONSTANT)
  Q_PROPERTY(qreal height READ height CONSTANT)

public:
  ResizeHandleFakeBox(ResizeHandleFakeRootWindow *rootWindow,
                      ResizeHandleFakeEditor *editorRef)
      : rootWindow_(rootWindow), editorRef_(editorRef) {}

  QObject *rootWindow() const { return rootWindow_; }
  QObject *editorRef() const { return editorRef_; }
  QVariant boxModel() const {
    QVariantMap value;
    value.insert(QStringLiteral("index"), 3);
    return value;
  }
  bool selected() const { return true; }
  bool perspectiveActive() const { return perspectiveActive_; }
  void setPerspectiveActive(bool value) { perspectiveActive_ = value; }
  qreal width() const { return 100; }
  qreal height() const { return 80; }

private:
  ResizeHandleFakeRootWindow *rootWindow_ = nullptr;
  ResizeHandleFakeEditor *editorRef_ = nullptr;
  bool perspectiveActive_ = false;
};

int countVisualChildrenByName(QQuickItem *item, const QString &objectName) {
  if (!item)
    return 0;
  int count = item->objectName() == objectName ? 1 : 0;
  for (QQuickItem *child : item->childItems())
    count += countVisualChildrenByName(child, objectName);
  return count;
}

void collectVisualChildrenByName(QQuickItem *item, const QString &objectName,
                                 QList<QQuickItem *> &results) {
  if (!item)
    return;
  if (item->objectName() == objectName)
    results.append(item);
  for (QQuickItem *child : item->childItems())
    collectVisualChildrenByName(child, objectName, results);
}

QQuickItem *findVisibleVisualChildByName(QQuickItem *item,
                                         const QString &objectName) {
  QList<QQuickItem *> matches;
  collectVisualChildrenByName(item, objectName, matches);
  for (QQuickItem *match : matches) {
    if (match->isVisible())
      return match;
  }
  return nullptr;
}

int countVisibleVisualChildrenByName(QQuickItem *item,
                                     const QString &objectName) {
  QList<QQuickItem *> matches;
  collectVisualChildrenByName(item, objectName, matches);
  int count = 0;
  for (QQuickItem *match : matches) {
    if (match->isVisible())
      ++count;
  }
  return count;
}

} // namespace

class QmlTextBoxDelegateSmokeTests final : public QObject {
  Q_OBJECT

private slots:
  void qmlTextEditOverlayKeepsFocusAndTextAcrossTyping() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 260, 90);
    editor.setSelectedFontSize(20);
    editor.setSelectedLineSpacing(7);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textArea = nullptr;
    QObject *outlinedText = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(textArea->property("activeFocus").toBool());

    auto *quickTextDocument =
        textArea->property("textDocument").value<QObject *>();
    QVERIFY(quickTextDocument);
    auto *documentWrapper =
        qobject_cast<QQuickTextDocument *>(quickTextDocument);
    QVERIFY(documentWrapper);
    auto *document = documentWrapper->textDocument();
    QVERIFY(document);
    const auto currentDocument = [&]() -> QTextDocument * {
      auto *currentTextArea = findVisualChildByName(
          window->contentItem(), QStringLiteral("boxTextArea"));
      if (!currentTextArea)
        return nullptr;
      auto *currentQuickTextDocument =
          currentTextArea->property("textDocument").value<QObject *>();
      auto *currentDocumentWrapper =
          qobject_cast<QQuickTextDocument *>(currentQuickTextDocument);
      return currentDocumentWrapper ? currentDocumentWrapper->textDocument()
                                    : nullptr;
    };
    const auto lineSpacing = [&]() {
      auto *current = currentDocument();
      return current ? current->firstBlock().blockFormat().lineHeight()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    const auto lineSpacingType = [&]() {
      auto *current = currentDocument();
      return current ? current->firstBlock().blockFormat().lineHeightType()
                     : -1;
    };
    QCOMPARE(lineSpacingType(), int(QTextBlockFormat::LineDistanceHeight));
    QCOMPARE(lineSpacing(), 7.0);

    QTest::keyClick(window, Qt::Key_A);
    QTest::qWait(20);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("a"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("a"));
    QCOMPARE(outlinedText->property("text").toString(), QStringLiteral("a"));
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("a"));
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("a"));

    QTest::keyClick(window, Qt::Key_B);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("ab"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("ab"));
    QCOMPARE(outlinedText->property("text").toString(), QStringLiteral("ab"));

    QTest::keyClick(window, Qt::Key_C);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("abc"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("abc"));
    QCOMPARE(outlinedText->property("text").toString(), QStringLiteral("abc"));
    QTRY_COMPARE(editor.selectedBoxViewModel()
                     .toMap()
                     .value(QStringLiteral("text"))
                     .toString(),
                 QStringLiteral("abc"));
    QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("abc"));
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());

    QTest::keyClick(window, Qt::Key_Backspace);
    QTest::qWait(20);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("ab"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("ab"));
    QCOMPARE(outlinedText->property("text").toString(), QStringLiteral("ab"));
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("ab"));

    textArea->setProperty("cursorPosition", 1);
    QTest::keyClick(window, Qt::Key_Delete);
    QTest::qWait(20);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("a"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("a"));
    QCOMPARE(outlinedText->property("text").toString(), QStringLiteral("a"));
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("a"));
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("a"));

    textArea->setProperty("cursorPosition", 1);
    typeText(window, u"bc");
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("abc"));
    QCOMPARE(outlinedText->property("text").toString(),
             QStringLiteral("abc"));

    typeText(window, u"def");
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("abcdef"));
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());

    QVERIFY(window->setProperty("zoom", 2.0));
    QTRY_COMPARE(lineSpacing(), 7.0);

    editor.setSelectedLineSpacing(9);
    QTRY_COMPARE(lineSpacing(), 9.0);
  }

  void qmlTextAreaCtrlCopyPasteEditsTextWithoutPastingBox() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 260, 90);
    editor.updateSelectedText(QStringLiteral("abc"));
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QObject *textArea = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(textArea->property("activeFocus").toBool());
    QTRY_COMPARE(textArea->property("text").toString(), QStringLiteral("abc"));
    QCOMPARE(editor.boxes().size(), 1);

    QGuiApplication::clipboard()->clear();
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    QTRY_COMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("abc"));

    QVERIFY(QMetaObject::invokeMethod(textArea, "deselect"));
    textArea->setProperty("cursorPosition", 3);
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);

    QTRY_COMPARE(textArea->property("text").toString(),
                 QStringLiteral("abcabc"));
    QTRY_COMPARE(editor.selectedBoxViewModel()
                     .toMap()
                     .value(QStringLiteral("text"))
                     .toString(),
                 QStringLiteral("abcabc"));
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(editor.selectedIndex(), 0);
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());
  }

  void qmlTextEditPreviewUsesUpdatedModelWhenEditEnds() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 260, 90);
    editor.updateSelectedText(QStringLiteral("Before"));
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textArea = nullptr;
    QObject *outlinedText = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(textArea->property("activeFocus").toBool());

    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    typeText(window, u"After");
    QTRY_COMPARE(outlinedText->property("text").toString(),
                 QStringLiteral("After"));
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("After"));

    QObject *canvas = nullptr;
    QTRY_VERIFY(canvas = findVisualChildByName(window->contentItem(),
                                               QStringLiteral("centralCanvas")));
    QString previewTextWhenEditEnded;
    const auto captureConnection = QObject::connect(
        &editor, &EditorController::stateChanged, window, [&]() {
          if (!editor.editingText() && previewTextWhenEditEnded.isEmpty()) {
            if (auto *currentOutlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText"))) {
              previewTextWhenEditEnded =
                  currentOutlinedText->property("text").toString();
            }
          }
        });

    QVERIFY(QMetaObject::invokeMethod(canvas, "forceActiveFocus"));
    QTRY_VERIFY(!editor.editingText());
    QObject::disconnect(captureConnection);

    QVERIFY(!editor.editingText());
    QTRY_VERIFY(!textArea->property("visible").toBool());
    QCOMPARE(editor.selectedIndex(), 0);
    QCOMPARE(previewTextWhenEditEnded, QStringLiteral("After"));
    QCOMPARE(outlinedText->property("text").toString(),
             QStringLiteral("After"));
  }

  void qmlCtrlSpaceWhileEditingRefreshesTextAreaAndPreview() {
    registerQmlTypes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(texts.write("[page1.png]\nFirst page line\n"), 28);
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 260, 90);
    editor.updateSelectedText(QStringLiteral("Before"));
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textArea = nullptr;
    QObject *outlinedText = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(textArea->property("activeFocus").toBool());

    QTest::keyClick(window, Qt::Key_Space, Qt::ControlModifier);

    QTRY_COMPARE(textArea->property("text").toString(),
                 QStringLiteral("First page line"));
    QTRY_COMPARE(textArea->property("livePreviewText").toString(),
                 QStringLiteral("First page line"));
    QTRY_COMPARE(outlinedText->property("text").toString(),
                 QStringLiteral("First page line"));
    QTRY_COMPARE(editor.selectedBoxViewModel()
                     .toMap()
                     .value(QStringLiteral("text"))
                     .toString(),
                 QStringLiteral("First page line"));
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());
  }

  void qmlPresetApplyWhileEditingRefreshesStyleWithoutResettingText() {
    registerQmlTypes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 260, 120);
    editor.updateSelectedText(QStringLiteral("Model text"));
    editor.setSelectedFontSize(34);
    editor.setSelectedBold(true);
    editor.setSelectedLineSpacing(11);
    editor.setSelectedAlignment(2);
    QVERIFY(editor.addPreset(QStringLiteral("Live Style")));

    editor.setSelectedFontSize(18);
    editor.setSelectedBold(false);
    editor.setSelectedLineSpacing(0);
    editor.setSelectedAlignment(0);
    editor.updateSelectedText(QStringLiteral("Draft"));
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textArea = nullptr;
    QObject *outlinedText = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(textArea->property("activeFocus").toBool());

    textArea->setProperty("cursorPosition", 5);
    QTest::keyClick(window, Qt::Key_Exclam);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("Draft!"));
    QCOMPARE(outlinedText->property("text").toString(),
             QStringLiteral("Draft!"));

    QVERIFY(editor.applySelectedPreset());

    QTRY_COMPARE(outlinedText->property("pixelSize").toInt(), 34);
    QCOMPARE(outlinedText->property("bold").toBool(), true);
    QCOMPARE(outlinedText->property("lineSpacing").toInt(), 11);
    QCOMPARE(outlinedText->property("horizontalAlignment").toInt(),
             int(Qt::AlignRight));
    QTRY_COMPARE(textArea->property("text").toString(),
                 QStringLiteral("Draft!"));
    QCOMPARE(textArea->property("livePreviewText").toString(),
             QStringLiteral("Draft!"));
    QCOMPARE(outlinedText->property("text").toString(),
             QStringLiteral("Draft!"));
    QCOMPARE(textArea->property("font").value<QFont>().pixelSize(), 34);
    QCOMPARE(textArea->property("font").value<QFont>().bold(), true);
    QCOMPARE(textArea->property("horizontalAlignment").toInt(),
             int(Qt::AlignRight));

    auto *quickTextDocument =
        textArea->property("textDocument").value<QObject *>();
    auto *documentWrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    QVERIFY(documentWrapper);
    QTRY_COMPARE(documentWrapper->textDocument()
                     ->firstBlock()
                     .blockFormat()
                     .lineHeight(),
                 11.0);
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("fontSize"))
                 .toInt(),
             34);
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("Draft!"));
    QVERIFY(editor.editingText());
  }

  void qmlResizeWhileEditingKeepsEffectiveBoundsAfterInteractionEnds() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 120, 60);
    editor.updateSelectedText(QStringLiteral("Resize me"));
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *delegate = nullptr;
    QObject *textArea = nullptr;
    QTRY_VERIFY(delegate = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(textArea->property("visible").toBool());
    QCOMPARE(delegate->property("width").toReal(), 120.0);
    QCOMPARE(delegate->property("height").toReal(), 60.0);

    editor.beginInteraction();
    editor.setSelectedBounds(10, 20, 180, 95);
    editor.endInteraction();

    QTRY_COMPARE(delegate->property("width").toReal(), 180.0);
    QTRY_COMPARE(delegate->property("height").toReal(), 95.0);
    QCOMPARE(editor.selectedBoxViewModel().toMap().value(QStringLiteral("w"))
                 .toDouble(),
             180.0);
    QCOMPARE(editor.selectedBoxViewModel().toMap().value(QStringLiteral("h"))
                 .toDouble(),
             95.0);
    QVERIFY(editor.editingText());
  }

  void qmlTextEditOverlayUsesOutlinedLayoutMetrics() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 180, 100);
    editor.updateSelectedText(QStringLiteral("Centered text"));
    editor.setSelectedFontSize(24);
    editor.setSelectedOutlineEnabled(true);
    editor.setSelectedOutlineSize(8);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textAreaObject = nullptr;
    QObject *outlinedObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textAreaObject->property("visible").toBool());
    auto *overlayOutlinedObject =
        textAreaObject->property("outlinedTextItem").value<QObject *>();
    QVERIFY(overlayOutlinedObject);
    QVERIFY(overlayOutlinedObject->property("editLayoutMetricsValid").toBool());
    auto *overlayOutlinedItem = qobject_cast<QQuickItem *>(overlayOutlinedObject);
    QVERIFY(overlayOutlinedItem);
    auto *contentItem = overlayOutlinedItem->parentItem();
    QVERIFY(contentItem);
    QCOMPARE(contentItem->property("outlinedTextItem").value<QObject *>(),
             overlayOutlinedObject);
    QCOMPARE(textAreaObject->property("outlinedTextItem").value<QObject *>(),
             overlayOutlinedObject);

    const qreal expectedInset =
        window->property("zoom").toReal() *
        overlayOutlinedObject->property("editLayoutLeftPadding").toReal();
    QCOMPARE(textAreaObject->property("leftPadding").toReal(), expectedInset);
    QCOMPARE(textAreaObject->property("rightPadding").toReal(), expectedInset);
    QVERIFY(textAreaObject->property("topPadding").toReal() > expectedInset);

    const QString delegateSource =
        readQmlFile(QStringLiteral("TextEditOverlay.qml"));
    QVERIFY(
        delegateSource.contains(QStringLiteral("wrapMode: TextEdit.WordWrap")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("outlinedTextItem.editLayoutTopPadding")));
  }

  void qmlTextEditOverlayCursorGeometryMatchesPreviewMetrics() {
    registerQmlTypes();

    struct Case {
      QString name;
      QString text;
      qreal width;
      qreal height;
      int fontSize;
      int outlineSize;
      int lineSpacing;
      qreal zoom;
      int alignment;
      bool italic;
      bool expectWrapped;
    };

    const QList<Case> cases{
        {QStringLiteral("outline inset"), QStringLiteral("Outlined text"),
         180, 100, 24, 8, 0, 1.0, 0, false, false},
        {QStringLiteral("multiline word wrap"),
         QStringLiteral("Alpha Beta Gamma Delta"), 105, 140, 20, 4, 0, 1.0, 0,
         false, true},
        {QStringLiteral("zoom render scale"), QStringLiteral("Zoomed text"),
         190, 110, 22, 6, 0, 1.75, 0, false, false},
        {QStringLiteral("right alignment"), QStringLiteral("Right aligned"),
         240, 110, 22, 4, 0, 1.0, 2, false, false},
        {QStringLiteral("center alignment"), QStringLiteral("Centered"), 240,
         110, 22, 4, 0, 1.0, 1, false, false},
        {QStringLiteral("center wrapped line spacing"),
         QStringLiteral("Alpha Beta Gamma Delta Epsilon Zeta"), 130, 190, 22, 6,
         12, 1.25, 1, false, true},
        {QStringLiteral("paint translation"), QStringLiteral("Italic fj"), 120,
         70, 42, 18, 0, 1.0, 0, true, false},
    };

    const auto nearlyEqual = [](qreal a, qreal b, qreal tolerance = 0.75) {
      return std::abs(a - b) <= tolerance;
    };

    bool sawPaintOffsetCase = false;
    for (const Case &testCase : cases) {
      EditorController editor;
      editor.newDocument();
      editor.createTextBox(10, 20, testCase.width, testCase.height);
      editor.updateSelectedText(testCase.text);
      editor.setSelectedFontSize(testCase.fontSize);
      editor.setSelectedOutlineEnabled(true);
      editor.setSelectedOutlineSize(testCase.outlineSize);
      editor.setSelectedLineSpacing(testCase.lineSpacing);
      editor.setSelectedAlignment(testCase.alignment);
      editor.setSelectedItalic(testCase.italic);
      editor.beginTextEdit();

      QQmlApplicationEngine engine;
      engine.rootContext()->setContextProperty(QStringLiteral("Editor"),
                                               &editor);
      engine.load(QUrl::fromLocalFile(
          QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
      QCOMPARE(engine.rootObjects().size(), 1);

      auto *window =
          qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
      QVERIFY(window);
      QVERIFY(window->setProperty("zoom", testCase.zoom));

      QObject *textAreaObject = nullptr;
      QObject *outlinedObject = nullptr;
      QTRY_VERIFY(textAreaObject = findVisualChildByName(
                      window->contentItem(), QStringLiteral("boxTextArea")));
      QTRY_VERIFY(outlinedObject = findVisualChildByName(
                      window->contentItem(), QStringLiteral("boxOutlinedText")));
      auto *textAreaItem = qobject_cast<QQuickItem *>(textAreaObject);
      auto *outlinedItem = qobject_cast<QQuickItem *>(outlinedObject);
      QVERIFY(textAreaItem);
      QVERIFY(outlinedItem);
      QTRY_VERIFY(textAreaObject->property("visible").toBool());
      QTRY_VERIFY(outlinedObject->property("editLayoutMetricsValid").toBool());

      auto *quickTextDocument =
          textAreaObject->property("textDocument").value<QObject *>();
      auto *documentWrapper =
          qobject_cast<QQuickTextDocument *>(quickTextDocument);
      QVERIFY(documentWrapper);
      auto *document = documentWrapper->textDocument();
      QVERIFY(document);
      QTRY_VERIFY(document->firstBlock().layout() &&
                  document->firstBlock().layout()->lineCount() > 0);

      const qreal expectedLeftPadding =
          outlinedObject->property("editLayoutLeftPadding").toReal();
      const qreal expectedRightPadding =
          outlinedObject->property("editLayoutRightPadding").toReal();
      const qreal expectedTopPadding =
          outlinedObject->property("editLayoutTopPadding").toReal();
      QVERIFY2(expectedLeftPadding > 0.0,
               qPrintable(testCase.name + QStringLiteral(" left padding")));
      QVERIFY2(expectedTopPadding >= expectedLeftPadding,
               qPrintable(testCase.name + QStringLiteral(" top padding")));
      QVERIFY2(nearlyEqual(textAreaObject->property("leftPadding").toReal(),
                           expectedLeftPadding),
               qPrintable(testCase.name));
      QVERIFY2(nearlyEqual(textAreaObject->property("rightPadding").toReal(),
                           expectedRightPadding),
               qPrintable(testCase.name));
      QVERIFY2(nearlyEqual(textAreaObject->property("topPadding").toReal(),
                           expectedTopPadding),
               qPrintable(testCase.name));
      const qreal paintOffsetX =
          outlinedObject->property("editLayoutPaintOffsetX").toReal();
      const qreal paintOffsetY =
          outlinedObject->property("editLayoutPaintOffsetY").toReal();
      sawPaintOffsetCase = sawPaintOffsetCase || std::abs(paintOffsetX) > 0.1 ||
                           std::abs(paintOffsetY) > 0.1;
      QVERIFY2(nearlyEqual(textAreaItem->x(), paintOffsetX * testCase.zoom, 1.0),
               qPrintable(QStringLiteral("%1 text area x=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(textAreaItem->x())
                              .arg(paintOffsetX * testCase.zoom)));
      QVERIFY2(nearlyEqual(textAreaItem->y(), paintOffsetY * testCase.zoom, 1.0),
               qPrintable(QStringLiteral("%1 text area y=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(textAreaItem->y())
                              .arg(paintOffsetY * testCase.zoom)));

      const qreal expectedTextWidth =
          textAreaItem->width() - expectedLeftPadding - expectedRightPadding;
      QVERIFY2(nearlyEqual(document->textWidth(), expectedTextWidth, 1.0),
               qPrintable(QStringLiteral("%1 document width=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(document->textWidth())
                              .arg(expectedTextWidth)));
      QVERIFY2(nearlyEqual(outlinedItem->width() * outlinedItem->scale(),
                           textAreaItem->width() * textAreaItem->scale()),
               qPrintable(testCase.name));
      QVERIFY2(nearlyEqual(outlinedItem->height() * outlinedItem->scale(),
                           textAreaItem->height() * textAreaItem->scale()),
               qPrintable(testCase.name));

      QTextLayout *layout = document->firstBlock().layout();
      QVERIFY(layout);
      if (testCase.expectWrapped)
        QVERIFY2(layout->lineCount() >= 2, qPrintable(testCase.name));

      QList<qreal> documentLineTops;
      for (QTextBlock block = document->begin(); block.isValid();
           block = block.next()) {
        QTextLayout *blockLayout = block.layout();
        QVERIFY(blockLayout);
        const QPointF blockPosition = blockLayout->position();
        for (int i = 0; i < blockLayout->lineCount(); ++i) {
          const QTextLine line = blockLayout->lineAt(i);
          QVERIFY(line.isValid());
          documentLineTops.append(expectedTopPadding + blockPosition.y() +
                                  line.y());
        }
      }
      const QVariantList previewLineTops =
          outlinedObject->property("editLayoutLineTops").toList();
      QCOMPARE(previewLineTops.size(), documentLineTops.size());
      for (qsizetype i = 0; i < previewLineTops.size(); ++i) {
        const qreal expectedLineTop = previewLineTops.at(i).toReal();
        QVERIFY2(nearlyEqual(documentLineTops.at(i), expectedLineTop, 1.0),
                 qPrintable(QStringLiteral("%1 line %2 y=%3 expected=%4")
                                .arg(testCase.name)
                                .arg(i)
                                .arg(documentLineTops.at(i))
                                .arg(expectedLineTop)));
      }

      const QTextLine firstLine = layout->lineAt(0);
      QVERIFY(firstLine.isValid());
      textAreaObject->setProperty("cursorPosition", 0);
      const QRectF cursorRect =
          textAreaObject->property("cursorRectangle").toRectF();
      QVERIFY2(!cursorRect.isEmpty(), qPrintable(testCase.name));

      const qreal expectedCursorX =
          expectedLeftPadding + firstLine.cursorToX(0);
      const qreal expectedCursorY = expectedTopPadding + firstLine.y();
      QVERIFY2(nearlyEqual(cursorRect.x(), expectedCursorX, 1.0),
               qPrintable(QStringLiteral("%1 cursor x=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(cursorRect.x())
                              .arg(expectedCursorX)));
      QVERIFY2(nearlyEqual(cursorRect.y(), expectedCursorY, 1.0),
               qPrintable(QStringLiteral("%1 cursor y=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(cursorRect.y())
                              .arg(expectedCursorY)));
      const qreal visualCursorX =
          textAreaItem->x() + cursorRect.x() * testCase.zoom;
      const qreal visualCursorY =
          textAreaItem->y() + cursorRect.y() * testCase.zoom;
      QVERIFY2(nearlyEqual(visualCursorX,
                           (paintOffsetX + expectedCursorX) * testCase.zoom,
                           1.25),
               qPrintable(QStringLiteral("%1 visual cursor x=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(visualCursorX)
                              .arg((paintOffsetX + expectedCursorX) *
                                   testCase.zoom)));
      QVERIFY2(nearlyEqual(visualCursorY,
                           (paintOffsetY + expectedCursorY) * testCase.zoom,
                           1.25),
               qPrintable(QStringLiteral("%1 visual cursor y=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(visualCursorY)
                              .arg((paintOffsetY + expectedCursorY) *
                                   testCase.zoom)));

      if (testCase.alignment == 2) {
        QCOMPARE(textAreaObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignRight));
        QCOMPARE(outlinedObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignRight));
        QVERIFY2(cursorRect.x() > expectedLeftPadding,
                 qPrintable(testCase.name));
      }
      if (testCase.alignment == 1 && !testCase.expectWrapped) {
        QCOMPARE(textAreaObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignHCenter));
        QCOMPARE(outlinedObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignHCenter));
        QVERIFY2(cursorRect.x() > expectedLeftPadding,
                 qPrintable(testCase.name));
        QVERIFY2(cursorRect.x() + firstLine.naturalTextWidth() <
                     expectedLeftPadding + document->textWidth(),
                 qPrintable(testCase.name));
      }
    }
    QVERIFY(sawPaintOffsetCase);
  }

  void qmlPathTextEditsWithFlatPreviewThenRestoresPathPreview() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 220, 110);
    editor.updateSelectedText(QStringLiteral("Curved edit"));
    editor.setSelectedFontSize(24);
    editor.setSelectedPathEnabled(true);
    editor.setPathHandle(1, 0.5, 0.1);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textAreaObject = nullptr;
    QObject *outlinedObject = nullptr;
    QObject *pathGuideObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(pathGuideObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("pathGuide")));

    QTRY_VERIFY(textAreaObject->property("visible").toBool());
    QCOMPARE(outlinedObject->property("pathEnabled").toBool(), false);
    QCOMPARE(outlinedObject->property("editLayoutMetricsValid").toBool(), true);
    QCOMPARE(pathGuideObject->property("visible").toBool(), true);

    editor.endTextEdit();
    QCoreApplication::processEvents();

    QTRY_VERIFY(!textAreaObject->property("visible").toBool());
    QCOMPARE(outlinedObject->property("pathEnabled").toBool(), true);
    QCOMPARE(outlinedObject->property("editLayoutMetricsValid").toBool(), false);
  }

  void qmlDecorationsLoadAndAlignForActiveDelegate() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 160, 80);
    editor.createTextBox(220, 30, 180, 90);
    editor.setSelectedPathEnabled(true);
    editor.setPathHandle(1, 0.5, 0.1);
    editor.setSelectedPerspectiveEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);

    auto *content = window->contentItem();
    QVERIFY(content);
    QTRY_COMPARE(countVisualChildrenByName(content,
                                           QStringLiteral("textBoxDelegate")),
                 2);
    QTRY_COMPARE(countVisualChildrenByName(content,
                                           QStringLiteral("boxOutlinedText")),
                 2);
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("resizeHandle_nw")),
                 1);
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("resizeHandle_se")),
                 1);
    QTRY_COMPARE(countVisualChildrenByName(content,
                                           QStringLiteral("rotateHandle")),
                 1);
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("perspectiveBorder")),
                 1);
    QTRY_COMPARE(countVisualChildrenByName(content, QStringLiteral("pathGuide")),
                 1);
    QVERIFY(countVisualChildrenByName(content, QStringLiteral("pathHandle")) >
            0);
    QQuickItem *perspectiveBorder = nullptr;
    QTRY_VERIFY(perspectiveBorder = findVisibleVisualChildByName(
                    content, QStringLiteral("perspectiveBorder")));

    QQuickItem *selectedDelegate = nullptr;
    QList<QQuickItem *> delegates;
    collectVisualChildrenByName(content, QStringLiteral("textBoxDelegate"),
                                delegates);
    for (QQuickItem *delegate : delegates) {
      if (delegate->property("boxModel")
              .toMap()
              .value(QStringLiteral("index"))
              .toInt() == editor.selectedIndex()) {
        selectedDelegate = delegate;
        break;
      }
    }
    QVERIFY(selectedDelegate);
    QCOMPARE(perspectiveBorder->parentItem(), selectedDelegate);
    QQuickItem *selectionControls = nullptr;
    QTRY_VERIFY(selectionControls = findVisibleVisualChildByName(
                    selectedDelegate, QStringLiteral("textBoxSelectionControls")));
    QCOMPARE(selectionControls->parentItem(), selectedDelegate);

    QQuickItem *pathGuide = nullptr;
    QTRY_VERIFY(pathGuide = findVisibleVisualChildByName(
                    content, QStringLiteral("pathGuide")));
    QQuickItem *pathHandle = nullptr;
    QTRY_VERIFY(pathHandle = findVisibleVisualChildByName(
                    content, QStringLiteral("pathHandle")));
    const auto stackingChildUnderDelegate = [selectedDelegate](QQuickItem *item) {
      QQuickItem *child = item;
      while (child && child->parentItem() && child->parentItem() != selectedDelegate)
        child = child->parentItem();
      return child;
    };
    QQuickItem *resizeHandle = nullptr;
    QTRY_VERIFY(resizeHandle = findVisibleVisualChildByName(
                    content, QStringLiteral("resizeHandle_nw")));
    QQuickItem *rotateHandle = nullptr;
    QTRY_VERIFY(rotateHandle = findVisibleVisualChildByName(
                    content, QStringLiteral("rotateHandle")));
    QQuickItem *pathGuideStackingItem = stackingChildUnderDelegate(pathGuide);
    QQuickItem *pathHandleStackingItem = stackingChildUnderDelegate(pathHandle);
    QQuickItem *resizeStackingItem = stackingChildUnderDelegate(resizeHandle);
    QQuickItem *rotateStackingItem = stackingChildUnderDelegate(rotateHandle);
    QVERIFY(pathGuideStackingItem);
    QVERIFY(pathHandleStackingItem);
    QVERIFY(resizeStackingItem);
    QVERIFY(rotateStackingItem);
    QCOMPARE(pathGuideStackingItem->parentItem(), selectedDelegate);
    QCOMPARE(pathHandleStackingItem->parentItem(), selectedDelegate);
    QCOMPARE(resizeStackingItem->parentItem(), selectedDelegate);
    QCOMPARE(rotateStackingItem->parentItem(), selectedDelegate);
    QVERIFY(pathGuideStackingItem->z() < perspectiveBorder->z());
    QVERIFY(resizeStackingItem->z() > perspectiveBorder->z());
    QVERIFY(rotateStackingItem->z() > perspectiveBorder->z());
    QVERIFY(pathHandleStackingItem->z() > perspectiveBorder->z());

    QVariant marginValue;
    QVERIFY(QMetaObject::invokeMethod(
        window, "perspectiveMargin", Q_RETURN_ARG(QVariant, marginValue),
        Q_ARG(QVariant, selectedDelegate->property("boxModel"))));
    const qreal margin = marginValue.toReal();
    QCOMPARE(perspectiveBorder->x(), -margin);
    QCOMPARE(perspectiveBorder->y(), -margin);
    QCOMPARE(perspectiveBorder->width(), selectedDelegate->width() + margin * 2);
    QCOMPARE(perspectiveBorder->height(),
             selectedDelegate->height() + margin * 2);

    const auto assertHandleCenter = [&](const QString &name) {
      QQuickItem *handle = findVisibleVisualChildByName(
          content, QStringLiteral("resizeHandle_") + name);
      QVERIFY(handle);
      QVariant expectedValue;
      QVERIFY(QMetaObject::invokeMethod(
          window, "visualHandlePosition", Q_RETURN_ARG(QVariant, expectedValue),
          Q_ARG(QVariant, selectedDelegate->property("boxModel")),
          Q_ARG(QVariant, name), Q_ARG(QVariant, selectedDelegate->width()),
          Q_ARG(QVariant, selectedDelegate->height())));
      const QVariantMap expected = expectedValue.toMap();
      const QPointF center = handle->mapToItem(
          selectedDelegate, QPointF(handle->width() / 2, handle->height() / 2));
      QVERIFY(std::abs(center.x() - expected.value(QStringLiteral("x")).toDouble()) <=
              0.001);
      QVERIFY(std::abs(center.y() - expected.value(QStringLiteral("y")).toDouble()) <=
              0.001);
    };
    assertHandleCenter(QStringLiteral("nw"));
    assertHandleCenter(QStringLiteral("ne"));
    assertHandleCenter(QStringLiteral("se"));
    assertHandleCenter(QStringLiteral("sw"));

    rotateHandle = nullptr;
    QTRY_VERIFY(rotateHandle = qobject_cast<QQuickItem *>(findVisualChildByName(
                    content, QStringLiteral("rotateHandle"))));
    QVERIFY(rotateHandle->width() > 0.0);
    QVERIFY(rotateHandle->height() > 0.0);
    QVERIFY2(rotateHandle->width() <= 64.0,
             "rotate handle must not expand to the selected box width");
    QCOMPARE(rotateHandle->height(), rotateHandle->width());

    editor.selectBox(0);
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("resizeHandle_nw")),
                 1);
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("resizeHandle_se")),
                 1);
    QTRY_COMPARE(countVisualChildrenByName(content,
                                           QStringLiteral("rotateHandle")),
                 1);
    rotateHandle = nullptr;
    QTRY_VERIFY(rotateHandle = qobject_cast<QQuickItem *>(findVisualChildByName(
                    content, QStringLiteral("rotateHandle"))));
    QVERIFY(rotateHandle->width() > 0.0);
    QVERIFY2(rotateHandle->width() <= 64.0,
             "rotate handle must not expand to the selected box width");
    QCOMPARE(rotateHandle->height(), rotateHandle->width());
    QTRY_COMPARE(countVisibleVisualChildrenByName(
                     content, QStringLiteral("perspectiveBorder")),
                 0);
    QTRY_COMPARE(countVisualChildrenByName(content, QStringLiteral("pathGuide")),
                 0);
    QCOMPARE(countVisualChildrenByName(content, QStringLiteral("pathHandle")),
             0);
    QCOMPARE(countVisualChildrenByName(content, QStringLiteral("boxOutlinedText")),
             2);
  }

  void qmlTextEditTabStopMatchesOutlinedLayoutMetrics() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 260, 120);
    editor.updateSelectedText(QStringLiteral("A\tB\tC"));
    editor.setSelectedFontSize(22);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(window->setProperty("zoom", 1.5));
    QObject *textAreaObject = nullptr;
    QObject *outlinedObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textAreaObject->property("visible").toBool());

    const qreal expectedTabStop =
        outlinedObject->property("editLayoutTabStopDistance").toReal();
    QVERIFY2(std::abs(textAreaObject->property("tabStopDistance").toReal() -
                      expectedTabStop) <= 0.5,
             qPrintable(QStringLiteral("tab=%1 expected=%2")
                            .arg(textAreaObject->property("tabStopDistance")
                                     .toReal())
                            .arg(expectedTabStop)));

    auto *quickTextDocument =
        textAreaObject->property("textDocument").value<QObject *>();
    auto *documentWrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    QVERIFY(documentWrapper);
    QVERIFY(std::abs(documentWrapper->textDocument()
                         ->defaultTextOption()
                         .tabStopDistance() -
                     expectedTabStop) <= 0.5);
  }

  void qmlWhitespaceAndAlignmentKeepEditPreviewMetricsInSync() {
    registerQmlTypes();

    struct Case {
      QString name;
      QString text;
      int alignment;
      int cursorPosition;
    };
    const QList<Case> cases{
        {QStringLiteral("multiple trailing spaces"),
         QStringLiteral("Alpha   Beta   "), 0, 16},
        {QStringLiteral("explicit empty line"), QStringLiteral("Top\n\nBottom"),
         0, 4},
        {QStringLiteral("center whitespace"), QStringLiteral("  Centered  "), 1,
         2},
        {QStringLiteral("right whitespace"), QStringLiteral("  Right  "), 2, 2},
        {QStringLiteral("long unbroken word"),
         QStringLiteral("Supercalifragilisticexpialidocious"), 0, 34},
    };

    const auto nearlyEqual = [](qreal a, qreal b, qreal tolerance = 1.0) {
      return std::abs(a - b) <= tolerance;
    };

    for (const Case &testCase : cases) {
      EditorController editor;
      editor.newDocument();
      editor.createTextBox(10, 20, 210, 130);
      editor.updateSelectedText(testCase.text);
      editor.setSelectedFontSize(22);
      editor.setSelectedOutlineEnabled(true);
      editor.setSelectedOutlineSize(6);
      editor.setSelectedAlignment(testCase.alignment);
      editor.beginTextEdit();

      QQmlApplicationEngine engine;
      engine.rootContext()->setContextProperty(QStringLiteral("Editor"),
                                               &editor);
      engine.load(QUrl::fromLocalFile(
          QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
      QCOMPARE(engine.rootObjects().size(), 1);

      auto *window =
          qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
      QVERIFY(window);
      QObject *textAreaObject = nullptr;
      QObject *outlinedObject = nullptr;
      QTRY_VERIFY(textAreaObject = findVisualChildByName(
                      window->contentItem(), QStringLiteral("boxTextArea")));
      QTRY_VERIFY(outlinedObject = findVisualChildByName(
                      window->contentItem(), QStringLiteral("boxOutlinedText")));
      auto *textAreaItem = qobject_cast<QQuickItem *>(textAreaObject);
      QVERIFY(textAreaItem);
      QTRY_VERIFY(textAreaObject->property("visible").toBool());

      auto *quickTextDocument =
          textAreaObject->property("textDocument").value<QObject *>();
      auto *documentWrapper =
          qobject_cast<QQuickTextDocument *>(quickTextDocument);
      QVERIFY(documentWrapper);
      auto *document = documentWrapper->textDocument();
      QVERIFY(document);
      QTRY_VERIFY(document->firstBlock().layout() &&
                  document->firstBlock().layout()->lineCount() > 0);

      const qreal expectedLeftPadding =
          outlinedObject->property("editLayoutLeftPadding").toReal();
      const qreal expectedRightPadding =
          outlinedObject->property("editLayoutRightPadding").toReal();
      QVERIFY(nearlyEqual(textAreaObject->property("leftPadding").toReal(),
                          expectedLeftPadding));
      QVERIFY(nearlyEqual(textAreaObject->property("rightPadding").toReal(),
                          expectedRightPadding));
      QVERIFY(nearlyEqual(document->textWidth(),
                          textAreaItem->width() - expectedLeftPadding -
                              expectedRightPadding));
      QCOMPARE(textAreaObject->property("horizontalAlignment").toInt(),
               testCase.alignment == 1   ? int(Qt::AlignHCenter)
               : testCase.alignment == 2 ? int(Qt::AlignRight)
                                          : int(Qt::AlignLeft));

      textAreaObject->setProperty("cursorPosition", testCase.cursorPosition);
      const QRectF cursorRect =
          textAreaObject->property("cursorRectangle").toRectF();
      QVERIFY2(!cursorRect.isEmpty(), qPrintable(testCase.name));
      QVERIFY2(std::isfinite(cursorRect.x()) && std::isfinite(cursorRect.y()),
               qPrintable(testCase.name));
    }
  }

  void qmlUppercaseEditPreviewAndModelStayInSync() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 220, 90);
    editor.updateSelectedText(QStringLiteral("mixed"));
    editor.setSelectedUppercase(true);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textAreaObject = nullptr;
    QObject *outlinedObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textAreaObject->property("visible").toBool());
    QTRY_VERIFY(textAreaObject->property("activeFocus").toBool());
    QCOMPARE(textAreaObject->property("text").toString(),
             QStringLiteral("MIXED"));
    QCOMPARE(outlinedObject->property("text").toString(),
             QStringLiteral("MIXED"));

    textAreaObject->setProperty("cursorPosition", 5);
    typeText(window, u"X");
    QTRY_COMPARE(outlinedObject->property("text").toString(),
                 QStringLiteral("MIXEDX"));
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("MIXEDX"));
  }

  void qmlLowercaseEditPreviewAndModelStayInSync() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 220, 90);
    editor.updateSelectedText(QStringLiteral("MIXED"));
    editor.setSelectedLowercase(true);
    editor.beginTextEdit();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textAreaObject = nullptr;
    QObject *outlinedObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    QTRY_VERIFY(outlinedObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(textAreaObject->property("visible").toBool());
    QTRY_VERIFY(textAreaObject->property("activeFocus").toBool());
    QCOMPARE(textAreaObject->property("text").toString(),
             QStringLiteral("mixed"));
    QCOMPARE(outlinedObject->property("text").toString(),
             QStringLiteral("mixed"));

    textAreaObject->setProperty("cursorPosition", 5);
    typeText(window, u"x");
    QTRY_COMPARE(outlinedObject->property("text").toString(),
                 QStringLiteral("mixedx"));
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("mixedx"));
  }

  void qmlPerspectiveUsesLiveClippedRendererWithoutPreviewArtifact() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")), {160, 80});

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(4, 4, 120, 40);
    editor.updateSelectedText(QStringLiteral("Perspective"));
    editor.setSelectedPerspectiveEnabled(true);

    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString contentSource =
        readQmlFile(QStringLiteral("TextBoxContent.qml"));
    const QString editControlsSource =
        readQmlFile(QStringLiteral("TextBoxEditControls.qml"));
    const QString editOverlaySource =
        readQmlFile(QStringLiteral("TextEditOverlay.qml"));

    const qsizetype delegateStart = delegateSource.indexOf(
        QStringLiteral("Rectangle {\n    id: boxDelegate"));
    const qsizetype textPerspectiveStart = contentSource.indexOf(
        QStringLiteral("id: boxTextPerspective"));
    const qsizetype textAreaStart = editOverlaySource.indexOf(
        QStringLiteral("id: boxTextArea"));
    const qsizetype mouseAreaStart =
        editOverlaySource.indexOf(QStringLiteral("Connections {"), textAreaStart);
    const qsizetype textOverlayStart = delegateSource.indexOf(
        QStringLiteral("TextBoxEditControls {"), delegateStart);
    QVERIFY(delegateStart >= 0);
    QVERIFY(textPerspectiveStart >= 0);
    QVERIFY(textAreaStart >= 0);
    QVERIFY(mouseAreaStart > textAreaStart);
    QVERIFY(textOverlayStart > delegateStart);
    QVERIFY(editControlsSource.contains(QStringLiteral("TextEditOverlay {")));
    const QString delegateHeader =
        delegateSource.mid(delegateStart, textOverlayStart - delegateStart);
    const QString textPerspectiveSource = contentSource.mid(textPerspectiveStart);
    const QString textAreaSource =
        editOverlaySource.mid(textAreaStart, mouseAreaStart - textAreaStart);

    QVERIFY(delegateHeader.contains(
        QStringLiteral("property bool perspectiveActive: boxModel.perspective "
                       "&& !editingSelected")));
    QVERIFY(textPerspectiveSource.contains(QStringLiteral("clip: true")));
    QVERIFY(textAreaSource.contains(QStringLiteral("clip: true")));
  }

  void qmlBlurEnabledBoxKeepsOutlinedRendererVisibleUntilEditing() {
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
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *outlinedText = nullptr;
    QTRY_VERIFY(outlinedText = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    QTRY_VERIFY(outlinedText->property("visible").toBool());
    QCOMPARE(outlinedText->property("blurSize").toInt(), 6);

    QObject *textArea = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    editor.beginTextEdit();
    QTRY_VERIFY(textArea->property("visible").toBool());
    QTRY_VERIFY(outlinedText->property("visible").toBool());
  }

  void qmlOutlinedRendererVisualSizeTracksZoom() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(4, 4, 120, 40);
    editor.updateSelectedText(QStringLiteral("Scale"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxOutlinedText")));
    auto *outlinedText = qobject_cast<QQuickItem *>(object);
    QVERIFY(outlinedText);
    auto *boxTextPerspective = outlinedText->parentItem();
    QVERIFY(boxTextPerspective);

    const auto nearlyEqual = [](qreal a, qreal b) {
      return std::abs(a - b) <= 0.5;
    };
    const auto verifyGeometry = [&]() {
      QVERIFY(nearlyEqual(outlinedText->width() * outlinedText->scale(),
                          boxTextPerspective->width()));
      QVERIFY(nearlyEqual(outlinedText->height() * outlinedText->scale(),
                          boxTextPerspective->height()));
      QVERIFY(outlinedText->property("renderScale").toReal() <= 1.0 + 0.001);
    };

    QTRY_VERIFY(boxTextPerspective->width() > 0);
    verifyGeometry();

    QVERIFY(window->setProperty("zoom", 2.0));
    QTRY_VERIFY(
        nearlyEqual(outlinedText->property("renderScale").toReal(), 1.0));
    verifyGeometry();

    QVERIFY(window->setProperty("zoom", 0.5));
    QTRY_VERIFY(outlinedText->property("renderScale").toReal() < 1.0);
    verifyGeometry();
  }

  void qmlTextOverflowMarksDelegate() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(4, 4, 300, 100);
    editor.updateSelectedText(QStringLiteral("Fits"));
    editor.setSelectedFontSize(20);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *delegate = nullptr;
    QObject *textAreaObject = nullptr;
    QTRY_VERIFY(delegate = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
    auto *outlinedObject =
        textAreaObject->property("outlinedTextItem").value<QObject *>();
    QVERIFY(outlinedObject);
    auto *outlinedItem = qobject_cast<QQuickItem *>(outlinedObject);
    QVERIFY(outlinedItem);
    auto *contentItem = outlinedItem->parentItem();
    QVERIFY(contentItem);
    QCOMPARE(contentItem->property("outlinedTextItem").value<QObject *>(),
             outlinedObject);

    QQmlProperty borderColorProperty(delegate, QStringLiteral("border.color"));
    QVERIFY(borderColorProperty.isValid());
    const QColor overflowColor(255, 0, 0, 255);
    QTRY_VERIFY(!outlinedObject->property("overflow").toBool());
    QCOMPARE(contentItem->property("overflow").toBool(), false);
    QCOMPARE(delegate->property("textOverflow").toBool(), false);
    QVERIFY(borderColorProperty.read().value<QColor>() != overflowColor);

    editor.updateSelectedText(
        QStringLiteral("Supercalifragilisticexpialidocious"));
    editor.setSelectedBounds(4, 4, 42, 24);

    QTRY_VERIFY(outlinedObject->property("overflow").toBool());
    QCOMPARE(contentItem->property("overflow").toBool(), true);
    QCOMPARE(delegate->property("textOverflow").toBool(), true);
    QTRY_COMPARE(borderColorProperty.read().value<QColor>(), overflowColor);
  }

  void qmlEditControlsExtractionPreservesRuntimeBindingsAndLayering() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 240, 100);
    editor.updateSelectedText(QStringLiteral("Runtime"));
    editor.setSelectedPathEnabled(true);
    editor.setPathHandle(1, 0.5, 0.1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);

    QObject *delegateObject = nullptr;
    QObject *contentDelegateObject = nullptr;
    QObject *canvasObject = nullptr;
    QTRY_VERIFY(delegateObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    QTRY_VERIFY(contentDelegateObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxContentDelegate")));
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *delegate = qobject_cast<QQuickItem *>(delegateObject);
    auto *contentDelegate = qobject_cast<QQuickItem *>(contentDelegateObject);
    QVERIFY(delegate);
    QVERIFY(contentDelegate);

    QList<QQuickItem *> editControlItems;
    collectVisualChildrenByName(window->contentItem(),
                                QStringLiteral("textBoxEditControls"),
                                editControlItems);
    QQuickItem *selectedEditControls = nullptr;
    QQuickItem *contentEditControls = nullptr;
    for (QQuickItem *item : editControlItems) {
      if (item->parentItem() == delegate)
        selectedEditControls = item;
      if (item->parentItem() == contentDelegate)
        contentEditControls = item;
    }
    QVERIFY(selectedEditControls);
    QVERIFY(contentEditControls);
    QCOMPARE(selectedEditControls->property("boxRef").value<QObject *>(),
             delegateObject);
    QCOMPARE(selectedEditControls->property("canvasItem").value<QObject *>(),
             canvasObject);
    QCOMPARE(selectedEditControls->z(),
             delegateObject->property("zTextContent").toReal());

    QObject *textAreaObject = nullptr;
    QObject *metricsOutlinedObject = nullptr;
    QObject *moveAreaObject = nullptr;
    QTRY_VERIFY(textAreaObject = findVisualChildByName(
                    selectedEditControls, QStringLiteral("boxTextArea")));
    QTRY_VERIFY(metricsOutlinedObject = findVisualChildByName(
                    delegate, QStringLiteral("boxOutlinedTextMetrics")));
    QTRY_VERIFY(moveAreaObject = findVisualChildByName(
                    selectedEditControls, QStringLiteral("textBoxMoveArea")));
    QCOMPARE(textAreaObject->property("outlinedTextItem").value<QObject *>(),
             metricsOutlinedObject);
    QCOMPARE(moveAreaObject->property("boxRef").value<QObject *>(), delegateObject);
    QCOMPARE(moveAreaObject->property("canvasItem").value<QObject *>(),
             canvasObject);
    QCOMPARE(moveAreaObject->property("editOverlay").value<QObject *>(),
             textAreaObject);
    QTRY_VERIFY(moveAreaObject->property("visible").toBool());
    QVERIFY(!textAreaObject->property("visible").toBool());

    editor.beginTextEdit();
    QTRY_VERIFY(textAreaObject->property("visible").toBool());
    QTRY_VERIFY(moveAreaObject->property("visible").toBool());
    QTRY_VERIFY(!moveAreaObject->property("enabled").toBool());

    QObject *inactiveTextAreaObject = nullptr;
    QObject *inactiveMoveAreaObject = nullptr;
    QTRY_VERIFY(inactiveTextAreaObject = findVisualChildByName(
                    contentEditControls, QStringLiteral("boxTextAreaInactive")));
    QTRY_VERIFY(inactiveMoveAreaObject = findVisualChildByName(
                    contentEditControls, QStringLiteral("textBoxMoveArea")));
    QVERIFY(!inactiveTextAreaObject->property("visible").toBool());
    QVERIFY(!inactiveMoveAreaObject->property("visible").toBool());

    QObject *resizeHandleObject = nullptr;
    QObject *rotateHandleObject = nullptr;
    QObject *pathHandleObject = nullptr;
    QTRY_VERIFY(resizeHandleObject = findVisualChildByName(
                    delegate, QStringLiteral("resizeHandle_nw")));
    QTRY_VERIFY(rotateHandleObject = findVisualChildByName(
                    delegate, QStringLiteral("rotateHandle")));
    QTRY_VERIFY(pathHandleObject = findVisualChildByName(
                    delegate, QStringLiteral("pathHandle")));
    auto *resizeHandle = qobject_cast<QQuickItem *>(resizeHandleObject);
    auto *rotateHandle = qobject_cast<QQuickItem *>(rotateHandleObject);
    auto *pathHandle = qobject_cast<QQuickItem *>(pathHandleObject);
    QVERIFY(resizeHandle);
    QVERIFY(rotateHandle);
    QVERIFY(pathHandle);
    QVERIFY(resizeHandle->parentItem());
    QVERIFY(rotateHandle->parentItem());
    QVERIFY(pathHandle->parentItem());
    QVERIFY(selectedEditControls->z() < resizeHandle->parentItem()->z());
    QVERIFY(selectedEditControls->z() < rotateHandle->parentItem()->z());
    QVERIFY(selectedEditControls->z() < pathHandle->parentItem()->z());
  }

  void qmlNestedInteractionHandlersUseStableEditorReference() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString editControlsSource =
        readQmlFile(QStringLiteral("TextBoxEditControls.qml"));
    const QString moveAreaSource =
        readQmlFile(QStringLiteral("TextBoxMoveArea.qml"));
    const QString resizeHandlesSource =
        readQmlFile(QStringLiteral("TextResizeHandles.qml"));
    const QString rotateHandleSource =
        readQmlFile(QStringLiteral("TextRotateHandle.qml"));
    const QString pathControlsSource =
        readQmlFile(QStringLiteral("TextBoxPathControls.qml"));
    const QString selectionControlsSource =
        readQmlFile(QStringLiteral("TextBoxSelectionControls.qml"));
    const QString pathHandlesSource =
        readQmlFile(QStringLiteral("TextPathHandles.qml"));

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        resizeHandlesSource, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype pathStart = pathHandlesSource.indexOf(
        QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"),
        0);
    const qsizetype pathEnd = pathHandlesSource.size();
    const qsizetype rotateRectStart =
        rotateHandleSource.indexOf(QStringLiteral("id: rotateHandle"));
    const qsizetype rotateStart = rotateHandleSource.indexOf(
        QStringLiteral(
            "rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef"));
    const qsizetype pathComponentStart =
        delegateSource.indexOf(QStringLiteral("TextBoxPathControls {"),
                               0);
    QVERIFY(resizeStart >= 0);
    QVERIFY(!moveAreaSource.isEmpty());
    QVERIFY(rotateRectStart >= 0);
    QVERIFY(rotateStart > rotateRectStart);
    QVERIFY(pathComponentStart >= 0);
    QVERIFY(pathStart >= 0);
    QVERIFY(pathEnd > pathStart);

    const QString resizeSource =
        resizeHandlesSource.mid(resizeStart);
    const QString rotateSource =
        rotateHandleSource.mid(rotateRectStart);
    const QString pathSource =
        pathHandlesSource.mid(pathStart, pathEnd - pathStart);

    QVERIFY(resizeSource.contains(
        QStringLiteral("dragEditorRef.endInteraction()")));
    QVERIFY(rotateSource.contains(
        QStringLiteral("rotateHandle.editorRef.endInteraction()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("readonly property var editor: Editor")));
    QVERIFY(delegateSource.contains(QStringLiteral(
        "readonly property var rootWindow: ApplicationWindow.window")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("readonly property var rootWindow: window")));
    QVERIFY(!delegateSource.contains(QStringLiteral("Editor.")));
    QVERIFY(!delegateSource.contains(QStringLiteral("window.")));
    QVERIFY(delegateSource.contains(QStringLiteral("TextBoxSelectionControls {")));
    QVERIFY(!delegateSource.contains(QStringLiteral("TextResizeHandles {")));
    QVERIFY(!delegateSource.contains(QStringLiteral("TextRotateHandle {")));
    QVERIFY(selectionControlsSource.contains(QStringLiteral("TextResizeHandles {")));
    QVERIFY(selectionControlsSource.contains(QStringLiteral("TextRotateHandle {")));
    QVERIFY(selectionControlsSource.contains(QStringLiteral("z: boxRef ? boxRef.zSelectionControls : 20")));
    QVERIFY(!selectionControlsSource.contains(QStringLiteral("parent: selectionControls.boxRef")));
    QVERIFY(selectionControlsSource.contains(QStringLiteral("boxRef.rotateDecorationsLoaded")));
    QVERIFY(delegateSource.contains(QStringLiteral("TextBoxPathControls {")));
    QVERIFY(pathControlsSource.contains(QStringLiteral("TextPathGuide {")));
    QVERIFY(pathControlsSource.contains(QStringLiteral("TextPathHandles {")));
    QVERIFY(delegateSource.contains(QStringLiteral("TextBoxEditControls {")));
    QVERIFY(delegateSource.contains(QStringLiteral("boxRef: boxDelegate")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("canvasItem: boxDelegate.canvasItem")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("outlinedTextItem: textContent.outlinedTextItem")));
    QVERIFY(!delegateSource.contains(QStringLiteral("TextBoxMoveArea {")));
    QVERIFY(editControlsSource.contains(QStringLiteral("TextBoxMoveArea {")));
    QVERIFY(editControlsSource.contains(QStringLiteral("TextEditOverlay {")));
    QVERIFY(editControlsSource.contains(QStringLiteral("editOverlay: boxTextOverlay")));
    QVERIFY(moveAreaSource.contains(
        QStringLiteral("property var rootWindow: boxRef.rootWindow")));
    QVERIFY(moveAreaSource.contains(
        QStringLiteral("property var editorRef: boxRef.editorRef")));
    QVERIFY(moveAreaSource.contains(
        QStringLiteral("editOverlay.forceEditFocus()")));
    QVERIFY(
        resizeSource.contains(QStringLiteral("property var boxRef: resizeHandles.boxRef")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("property var rootWindow: boxRef ? boxRef.rootWindow : null")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("property var editorRef: boxRef ? boxRef.editorRef : null")));
    QVERIFY(resizeSource.contains(QStringLiteral("readonly property bool handleReady")));
    QVERIFY(resizeSource.contains(QStringLiteral("property var dragRootWindow: null")));
    QVERIFY(resizeSource.contains(QStringLiteral("property var dragEditorRef: null")));
    QVERIFY(resizeSource.contains(QStringLiteral("visible: handleReady && boxRef.selected")));
    QVERIFY(
        rotateSource.contains(QStringLiteral("property var boxRef")));
    QVERIFY(pathSource.contains(
        QStringLiteral("property var boxRef: pathPlane.boxRef")));
    QVERIFY(pathSource.contains(
        QStringLiteral("property var editorRef: boxRef.editorRef")));
    QVERIFY(!resizeSource.contains(
        QStringLiteral("if (boxDelegate.perspectiveActive)")));
    QVERIFY(!resizeSource.contains(
        QStringLiteral("boxDelegate.rootWindow.endResizeDrag")));
    QVERIFY(!resizeSource.contains(
        QStringLiteral("boxDelegate.rootWindow.endPerspectiveDrag")));
    QVERIFY(!rotateSource.contains(
        QStringLiteral("boxDelegate.rootWindow.endRotateDrag")));
    QVERIFY(!resizeSource.contains(QStringLiteral("boxDelegate.")));
    QVERIFY(!rotateSource.contains(QStringLiteral("boxDelegate.")));
    QVERIFY(!pathSource.contains(QStringLiteral("boxDelegate.")));
    QVERIFY(!pathSource.contains(
        QStringLiteral("onReleased: boxDelegate.editorRef")));
    QVERIFY(!pathSource.contains(
        QStringLiteral("onCanceled: boxDelegate.editorRef")));
    QVERIFY(!resizeSource.contains(QStringLiteral("Editor.endInteraction()")));
    QVERIFY(!rotateSource.contains(QStringLiteral("Editor.endInteraction()")));
    QVERIFY(!pathSource.contains(QStringLiteral("Editor.endInteraction()")));
    QVERIFY(!resizeSource.contains(QStringLiteral("window.")));
    QVERIFY(!rotateSource.contains(QStringLiteral("window.")));
    QVERIFY(!pathSource.contains(QStringLiteral("window.")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("model: [{name: \"nw\"}, {name: \"n\"}")));
  }

  void qmlResizeHandleFinishesDragAfterBoxRefClears() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qmlPath = dir.filePath(QStringLiteral("ResizeHarness.qml"));
    QFile qml(qmlPath);
    QVERIFY(qml.open(QIODevice::WriteOnly | QIODevice::Text));
    qml.write(QStringLiteral(R"QML(
import QtQuick
import "%1"

Item {
    id: root
    width: 120
    height: 120
    property var currentBox: fakeBox

    TextResizeHandles {
        anchors.fill: parent
        boxRef: root.currentBox
        canvasItem: root
    }
}
)QML")
                  .arg(QUrl::fromLocalFile(QStringLiteral(
                           TEXTFX_FIXTURE_DIR
                           "/../../qml/features/canvas/text"))
                           .toString())
                  .toUtf8());
    qml.close();

    ResizeHandleFakeRootWindow rootWindow;
    ResizeHandleFakeEditor editor;
    ResizeHandleFakeBox box(&rootWindow, &editor);
    ResizeHandleFakeRootWindow replacementRootWindow;
    ResizeHandleFakeEditor replacementEditor;
    ResizeHandleFakeBox replacementBox(&replacementRootWindow, &replacementEditor);

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("fakeBox"), &box);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(120, 120);
    view.setSource(QUrl::fromLocalFile(qmlPath));
    QVERIFY2(view.status() == QQuickView::Ready,
             qPrintable(view.errors().isEmpty()
                            ? QStringLiteral("QML failed to load")
                            : view.errors().constFirst().toString()));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    auto *root = qobject_cast<QQuickItem *>(view.rootObject());
    QVERIFY(root);
    QQuickItem *handle = nullptr;
    QTRY_VERIFY(handle = findVisibleVisualChildByName(
                    root, QStringLiteral("resizeHandle_nw")));
    const QPoint pressPoint =
        handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2))
            .toPoint();

    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, pressPoint);
    QCOMPARE(editor.selectedIndex, 3);
    QCOMPARE(editor.beginCount, 1);
    QCOMPARE(rootWindow.resizeBeginCount, 1);

    QVERIFY(root->setProperty("currentBox", QVariant::fromValue<QObject *>(&replacementBox)));
    QCoreApplication::processEvents();
    QVERIFY(handle->property("handleReady").toBool());
    QTest::mouseMove(&view, pressPoint + QPoint(8, 8));
    QCOMPARE(rootWindow.resizeUpdateCount, 1);
    QCOMPARE(replacementRootWindow.resizeUpdateCount, 0);
    QVERIFY(root->setProperty("currentBox", QVariant::fromValue<QObject *>(nullptr)));
    QCoreApplication::processEvents();
    QVERIFY(!handle->property("handleReady").toBool());
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier,
                        pressPoint + QPoint(8, 8));
    QCOMPARE(rootWindow.resizeEndCount, 1);
    QVERIFY(!rootWindow.resizeCommit);
    QCOMPARE(rootWindow.perspectiveEndCount, 0);
    QCOMPARE(editor.endCount, 1);

    box.setPerspectiveActive(true);
    QVERIFY(root->setProperty("currentBox", QVariant::fromValue<QObject *>(&box)));
    QCoreApplication::processEvents();
    QTRY_VERIFY(handle->property("handleReady").toBool());
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, pressPoint);
    QCOMPARE(editor.beginCount, 2);
    QCOMPARE(rootWindow.perspectiveBeginCount, 1);

    replacementBox.setPerspectiveActive(false);
    QVERIFY(root->setProperty("currentBox", QVariant::fromValue<QObject *>(&replacementBox)));
    QCoreApplication::processEvents();
    QVERIFY(handle->property("handleReady").toBool());
    QTest::mouseMove(&view, pressPoint + QPoint(4, 4));
    QCOMPARE(rootWindow.perspectiveUpdateCount, 1);
    QCOMPARE(replacementRootWindow.perspectiveUpdateCount, 0);
    QVERIFY(root->setProperty("currentBox", QVariant::fromValue<QObject *>(nullptr)));
    QCoreApplication::processEvents();
    QVERIFY(!handle->property("handleReady").toBool());
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier,
                        pressPoint + QPoint(4, 4));
    QCOMPARE(rootWindow.perspectiveEndCount, 1);
    QVERIFY(!rootWindow.perspectiveCommit);
    QCOMPARE(rootWindow.resizeEndCount, 1);
    QCOMPARE(editor.endCount, 2);
  }

  void qmlPreviewRepeaterUsesRoleSafeBoxesModelDelegate() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString canvasSource = readQmlFile(QStringLiteral("CanvasView.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));

    QVERIFY(mainSource.contains(QStringLiteral("CanvasView {")));
    QVERIFY(canvasSource.contains(QStringLiteral("model: canvasView.editor.boxesModel")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property int boxIndex")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property real boxX")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property real boxY")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property real boxWidth")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property real boxHeight")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("required property real boxRotation")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("readonly property var boxModel: ({")));
    QVERIFY(delegateSource.contains(QStringLiteral("index: boxIndex")));
    QVERIFY(delegateSource.contains(QStringLiteral("x: boxX")));
    QVERIFY(delegateSource.contains(QStringLiteral("w: boxWidth")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("required property var modelData")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("readonly property var effectiveBoxModel")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("editorRef ? editorRef.selectedBox : boxModel")));
  }

  void qmlInspectorsUseSelectedIndexAndModelRolesWithoutSelectedBoxSnapshot() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString leftSource = readQmlFile(QStringLiteral("LeftInspectorPanel.qml"));
    const QString rightSource = readQmlFile(QStringLiteral("RightInspectorPanel.qml"));
    const QString selectedBoxStateSource =
        readQmlFile(QStringLiteral("SelectedBoxState.qml"));
    const QString layersSource = readQmlFile(QStringLiteral("LayersSection.qml"));

    QVERIFY(!mainSource.contains(QStringLiteral("selectedBox: Editor.selectedBox")));
    QVERIFY(!leftSource.contains(QStringLiteral("property var selectedBox:")));
    QVERIFY(!rightSource.contains(QStringLiteral("property var selectedBox:")));
    QVERIFY(leftSource.contains(QStringLiteral("selectedBoxData")));
    QVERIFY(rightSource.contains(QStringLiteral("selectedBoxData")));
    QVERIFY(leftSource.contains(QStringLiteral("selectedBoxState.value")));
    QVERIFY(rightSource.contains(QStringLiteral("selectedBoxState.value")));
    QVERIFY(!leftSource.contains(QStringLiteral("editor.boxRole(editor.selectedIndex")));
    QVERIFY(!rightSource.contains(QStringLiteral("editor.boxRole(editor.selectedIndex")));
    QVERIFY(!leftSource.contains(QStringLiteral("selectedBoxRevision")));
    QVERIFY(!rightSource.contains(QStringLiteral("selectedBoxRevision")));
    QVERIFY(!leftSource.contains(QStringLiteral("leftInspectorPanel.editor.boxesModel")));
    QVERIFY(!rightSource.contains(QStringLiteral("rightInspectorPanel.editor.boxesModel")));
    QVERIFY(selectedBoxStateSource.contains(
        QStringLiteral("editor.boxRole(editor.selectedIndex")));
    QVERIFY(selectedBoxStateSource.contains(QStringLiteral("revision")));
    QVERIFY(selectedBoxStateSource.contains(
        QStringLiteral("selectedBoxState.editor.boxesModel")));
    QVERIFY(layersSource.contains(QStringLiteral("layersSection.editor.boxCount")));
    QVERIFY(!rightSource.contains(QStringLiteral("rightInspectorPanel.editor.boxes.length")));
    QVERIFY(!layersSource.contains(QStringLiteral("layersSection.editor.boxes.length")));
  }

  void qmlSelectionGeometryAndRotateHandleUseCurrentVisualBounds() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString geometrySource =
        readQmlFile(QStringLiteral("PerspectiveGeometry.qml"));
    const QString viewportSource =
        readQmlFile(QStringLiteral("ViewportMetrics.qml"));

    QVERIFY(delegateSource.contains(QStringLiteral("property real visualDocX: boxModel.x")));
    QVERIFY(delegateSource.contains(QStringLiteral("property real visualDocY: boxModel.y")));
    QVERIFY(delegateSource.contains(QStringLiteral("property real visualDocW: boxModel.w")));
    QVERIFY(!delegateSource.contains(QStringLiteral("property bool moveActive")));
    QVERIFY(!delegateSource.contains(QStringLiteral("property bool resizeActive")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("width: visualDocW * rootWindow.viewDocScale()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource,
        QStringLiteral("function documentToViewLength(value) { return "
                       "viewportMetrics.documentToViewLength(value) }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        viewportSource,
        QStringLiteral("function documentToViewLength(value) { return "
                       "value * viewDocScale() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource,
        QStringLiteral(
            "function handleSize() { return Math.max(1, "
            "documentToViewLength(editorLimits.minimumBoxSize)) }")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("border.width: !renderSelectionUi || perspectiveActive ? 0 : selectionMember ? "
                       "rootWindow.selectionLineWidth() : Math.max(1, "
                       "rootWindow.documentToViewLength(1))")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("activeResizeDelegate.x = documentToViewX(resizeX)")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("activeMoveDelegate.x = documentToViewX(moveX)")));
    QVERIFY(geometrySource.contains(
        QStringLiteral("function topMiddleVisualPoint(box, width, height)")));
    QVERIFY(geometrySource.contains(QStringLiteral(
        "const nw = perspectiveCorner(box, \"nw\", width, height)")));
    QVERIFY(geometrySource.contains(QStringLiteral(
        "const ne = perspectiveCorner(box, \"ne\", width, height)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        geometrySource,
        QStringLiteral(
            "return { x: (nw.x + ne.x) / 2, y: (nw.y + ne.y) / 2 }")));
    const QString rotateHandleSource =
        readQmlFile(QStringLiteral("TextRotateHandle.qml"));

    QVERIFY(rotateHandleSource.contains(
        QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, "
                       "boxRef.width, boxRef.height)")));
  }

  void qmlHasEightResizeHandlesAndAngleRotation() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString resizeHandlesSource =
        readQmlFile(QStringLiteral("TextResizeHandles.qml"));
    const QString rotateHandleSource =
        readQmlFile(QStringLiteral("TextRotateHandle.qml"));
    const QString rotateStateSource =
        readQmlFile(QStringLiteral("BoxRotateInteractionState.qml"));

    for (const QString &handle :
         {QStringLiteral("nw"), QStringLiteral("n"), QStringLiteral("ne"),
          QStringLiteral("e"), QStringLiteral("se"), QStringLiteral("s"),
          QStringLiteral("sw"), QStringLiteral("w")}) {
      QVERIFY2(sourceContainsIgnoringWhitespace(
                   resizeHandlesSource,
                   QStringLiteral("name: \"") + handle + QStringLiteral("\"")),
                qPrintable(handle));
    }
    QVERIFY(rotateStateSource.contains(QStringLiteral("Math.atan2")));
    QVERIFY(mainSource.contains(QStringLiteral("BoxRotateInteractionState")));
    QVERIFY(
        mainSource.contains(QStringLiteral("window.editor.setSelectedBounds")));
    QVERIFY(rotateHandleSource.contains(
        QStringLiteral("rotateHandle.rootWindow.endRotateDrag(true)")));
    QVERIFY(!delegateSource.contains(
        QStringLiteral("Editor.rotateSelected(degrees)")));
  }

  void qmlBoxRotateUsesTransientWindowDragAndStableEditor() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString rotateStateSource =
        readQmlFile(QStringLiteral("BoxRotateInteractionState.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!rotateStateSource.isEmpty());
    QVERIFY(!delegateSource.isEmpty());

    const qsizetype updateRotateStart = mainSource.indexOf(
        QStringLiteral("function updateRotateDrag(canvasX, canvasY)"));
    const qsizetype endRotateStart =
        mainSource.indexOf(QStringLiteral("function endRotateDrag(commit)"));
    const qsizetype beginMoveStart = mainSource.indexOf(
        QStringLiteral("function beginMoveDrag"), endRotateStart);
    QVERIFY(updateRotateStart >= 0);
    QVERIFY(endRotateStart > updateRotateStart);
    QVERIFY(beginMoveStart > endRotateStart);
    const QString updateRotate =
        mainSource.mid(updateRotateStart, endRotateStart - updateRotateStart);
    const QString endRotate =
        mainSource.mid(endRotateStart, beginMoveStart - endRotateStart);

    QVERIFY(rotateStateSource.contains(
        QStringLiteral("function begin(delegate, documentX, documentY)")));
    QVERIFY(rotateStateSource.contains(
        QStringLiteral("function update(documentX, documentY)")));
    QVERIFY(
        !rotateStateSource.contains(QStringLiteral("function cancelPreview()")));
    QVERIFY(rotateStateSource.contains(QStringLiteral("function reset()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("property alias activeRotateDelegate: "
                       "boxRotateInteraction.activeRotateDelegate")));
    QVERIFY(mainSource.contains(QStringLiteral(
        "property alias rotateDegrees: boxRotateInteraction.rotateDegrees")));
    QVERIFY(mainSource.contains(QStringLiteral("BoxRotateInteractionState")));
    QVERIFY(mainSource.contains(
        QStringLiteral("objectName: \"boxRotateInteractionState\"")));
    QVERIFY(mainSource.contains(
        QStringLiteral("boxRotateInteraction.begin(delegate, "
                       "viewToDocumentX(canvasX), viewToDocumentY(canvasY))")));
    QVERIFY(updateRotate.contains(
        QStringLiteral("boxRotateInteraction.update(viewToDocumentX(canvasX), "
                       "viewToDocumentY(canvasY))")));
    QVERIFY(updateRotate.contains(
        QStringLiteral("window.editor.setSelectedRotation(boxRotateInteraction."
                       "rotateDegrees)")));
    QVERIFY(endRotate.contains(
        QStringLiteral("window.editor.setSelectedRotation(boxRotateInteraction."
                       "rotateStartRotation)")));
    QVERIFY(endRotate.contains(QStringLiteral("boxRotateInteraction.reset()")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("function angleDeltaDegrees(from, to)")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("activeRotateDelegate.rotation = rotateDegrees")));
    QVERIFY(updateRotate.contains(QStringLiteral("setSelectedRotation")));
    QVERIFY(
        !delegateSource.contains(QStringLiteral("Editor.setSelectedRotation")));
  }

  void qmlPerspectiveAndRotateDragsUpdateDocumentLiveAndCancelToStart() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString rotateStateSource =
        readQmlFile(QStringLiteral("BoxRotateInteractionState.qml"));

    const qsizetype perspectiveUpdateStart = mainSource.indexOf(
        QStringLiteral("function updatePerspectiveDrag(canvasX, canvasY)"));
    const qsizetype perspectiveEndStart = mainSource.indexOf(
        QStringLiteral("function endPerspectiveDrag(commit)"));
    const qsizetype rotateStart = mainSource.indexOf(
        QStringLiteral("function beginRotateDrag(delegate, canvasX, canvasY)"));
    const qsizetype rotateUpdateStart = mainSource.indexOf(
        QStringLiteral("function updateRotateDrag(canvasX, canvasY)"));
    const qsizetype rotateEndStart =
        mainSource.indexOf(QStringLiteral("function endRotateDrag(commit)"));
    const qsizetype escapeStart =
        mainSource.indexOf(QStringLiteral("function handleEscape()"));
    QVERIFY(perspectiveUpdateStart >= 0);
    QVERIFY(perspectiveEndStart > perspectiveUpdateStart);
    QVERIFY(rotateStart > perspectiveEndStart);
    QVERIFY(rotateUpdateStart > rotateStart);
    QVERIFY(rotateEndStart > rotateUpdateStart);
    QVERIFY(escapeStart > rotateEndStart);

    const QString perspectiveUpdate = mainSource.mid(
        perspectiveUpdateStart, perspectiveEndStart - perspectiveUpdateStart);
    const QString perspectiveEnd =
        mainSource.mid(perspectiveEndStart, rotateStart - perspectiveEndStart);
    const QString rotateUpdate =
        mainSource.mid(rotateUpdateStart, rotateEndStart - rotateUpdateStart);
    const QString rotateEnd =
        mainSource.mid(rotateEndStart, escapeStart - rotateEndStart);

    QVERIFY(mainSource.contains(
        QStringLiteral("property alias activePerspectiveDelegate: "
                       "perspectiveInteraction.activePerspectiveDelegate")));
    QVERIFY(mainSource.contains(
        QStringLiteral("objectName: \"perspectiveInteractionState\"")));
    QVERIFY(mainSource.contains(
        QStringLiteral("property alias activeRotateDelegate: "
                       "boxRotateInteraction.activeRotateDelegate")));
    QVERIFY(perspectiveUpdate.contains(
        QStringLiteral("applyPerspectiveHandles(perspectiveInteraction.commitHandles())")));
    QVERIFY(perspectiveEnd.contains(QStringLiteral(
        "if (dragMode === editorInteraction.dragModePerspective && !commit)")));
    QVERIFY(perspectiveEnd.contains(QStringLiteral("restorePerspectiveStartHandles()")));
    QVERIFY(!rotateStateSource.contains(
        QStringLiteral("activeRotateDelegate.rotation = rotateDegrees")));
    QVERIFY(
        rotateUpdate.contains(QStringLiteral("setSelectedRotation")));
    QVERIFY(rotateEnd.contains(QStringLiteral(
        "if (dragMode === editorInteraction.dragModeRotate && !commit)")));
    QVERIFY(rotateEnd.contains(
        QStringLiteral("window.editor.setSelectedRotation(boxRotateInteraction."
                       "rotateStartRotation)")));
  }
      void qmlMultiSelectionVisualsAndEscapeUseMembership() {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 120, 60);
        editor.createTextBox(180, 20, 120, 60);
        editor.selectBox(0);
        editor.toggleBoxSelection(1);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(
            QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window =
            qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QList<QQuickItem *> delegates;
        QTRY_VERIFY((delegates.clear(), collectVisualChildrenByName(window->contentItem(),
                                                  QStringLiteral("textBoxDelegate"),
                                                  delegates), delegates.size() == 2));
        QQuickItem *primary = nullptr;
        QQuickItem *secondary = nullptr;
        for (QQuickItem *delegate : delegates) {
          if (delegate->property("boxModel").toMap().value("index").toInt() == 1)
            primary = delegate;
          else
            secondary = delegate;
        }
        QVERIFY(primary);
        QVERIFY(secondary);
        QTRY_VERIFY(QQmlProperty(primary, QStringLiteral("border.width")).read()
                        .toReal() > 0.0);
        QTRY_VERIFY(QQmlProperty(secondary, QStringLiteral("border.width")).read()
                        .toReal() > 0.0);
        QCOMPARE(countVisibleVisualChildrenByName(
                     window->contentItem(), QStringLiteral("resizeHandle_nw")),
                 1);

        const auto sceneCenter = [](QQuickItem *item) {
          return item->mapToScene(QPointF(item->width() / 2, item->height() / 2))
              .toPoint();
        };
        editor.selectBox(0);
        const QPoint primaryCenter = sceneCenter(primary);
        const QPointF primaryPosition = primary->position();
        QTest::mousePress(window, Qt::LeftButton, Qt::ControlModifier, primaryCenter);
        QCOMPARE(window->property("dragMode").toInt(), 0);
        QCOMPARE(editor.selectedIndices().size(), 1);
        QCOMPARE(editor.selectedIndices().at(0).toInt(), 0);
        QTest::mouseMove(window, primaryCenter + QPoint(30, 20));
        QTest::mouseMove(window, primaryCenter + QPoint(1, 1));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::ControlModifier,
                            primaryCenter + QPoint(1, 1));
        QCOMPARE(window->property("dragMode").toInt(), 0);
        QCOMPARE(editor.selectedIndices().size(), 1);
        QCOMPARE(editor.selectedIndices().at(0).toInt(), 0);
        QCOMPARE(primary->position(), primaryPosition);
        QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier, primaryCenter);
        QTRY_COMPARE(editor.selectedIndex(), 1);
        QCOMPARE(editor.selectedIndices().size(), 2);

        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
                            sceneCenter(primary));
        QTRY_COMPARE(editor.selectedIndex(), 1);
        QCOMPARE(editor.selectedIndices().size(), 1);
        QTRY_VERIFY(editor.editingText());
        const QVariantMap plainStartBounds = editor.boxes().at(0).toMap();
        const QPoint plainStart = sceneCenter(secondary);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, plainStart);
        QTRY_VERIFY(!editor.editingText());
        QTRY_COMPARE(editor.selectedIndex(), 0);
        QCOMPARE(window->property("dragMode").toInt(), 7);
        QTest::mouseMove(window, plainStart + QPoint(15, 10));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier,
                            plainStart + QPoint(15, 10));
        QTRY_COMPARE(window->property("dragMode").toInt(), 0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
                 plainStartBounds.value(QStringLiteral("x")).toDouble() + 15.0);

        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
                            sceneCenter(primary));
        QTRY_COMPARE(editor.selectedIndex(), 1);
        QTRY_VERIFY(editor.editingText());
        const QVariantMap shiftStartBounds = editor.boxes().at(0).toMap();
        const QPoint shiftStart = sceneCenter(secondary);
        QTest::mousePress(window, Qt::LeftButton, Qt::ShiftModifier, shiftStart);
        QTRY_VERIFY(!editor.editingText());
        QTRY_COMPARE(editor.selectedIndex(), 0);
        QCOMPARE(window->property("dragMode").toInt(), 7);
        QTest::mouseMove(window, shiftStart + QPoint(20, 10));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::ShiftModifier,
                            shiftStart + QPoint(20, 10));
        QTRY_COMPARE(window->property("dragMode").toInt(), 0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
                 shiftStartBounds.value(QStringLiteral("x")).toDouble() + 20.0);

        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
                            sceneCenter(primary));
        QTRY_COMPARE(editor.selectedIndex(), 1);
        QCOMPARE(editor.selectedIndices().size(), 1);
        QTRY_VERIFY(editor.editingText());

        QObject *canvas = nullptr;
        QTRY_VERIFY(canvas = findVisualChildByName(window->contentItem(),
                                                   QStringLiteral("centralCanvas")));
        QVERIFY(QMetaObject::invokeMethod(canvas, "forceActiveFocus"));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!editor.editingText());
        QTRY_COMPARE(editor.selectedIndex(), -1);
        QCOMPARE(editor.selectedIndices().size(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QCOMPARE(editor.selectedIndex(), -1);
        QCOMPARE(editor.selectedIndices().size(), 0);

        auto *canvasItem = qobject_cast<QQuickItem *>(canvas);
        QVERIFY(canvasItem);
        const QPoint createStart = canvasItem->mapToScene(QPointF(420, 260)).toPoint();
        const QPoint createEnd = createStart + QPoint(80, 60);
        QTest::mousePress(window, Qt::LeftButton, Qt::ControlModifier, createStart);
        QTest::mouseMove(window, createEnd);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::ControlModifier, createEnd);
        QTRY_COMPARE(editor.boxes().size(), 3);
      }

      void qmlPlainMoveReleaseCommitsAndEscapeDoesNotRestoreBounds() {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 120, 60);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(
            QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window =
            qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QQuickItem *delegate = nullptr;
        QTRY_VERIFY(delegate = qobject_cast<QQuickItem *>(findVisualChildByName(
                        window->contentItem(), QStringLiteral("textBoxDelegate"))));
        const QPoint start = delegate->mapToScene(
            QPointF(delegate->width() / 2, delegate->height() / 2)).toPoint();
        const QPoint end = start + QPoint(40, 25);
        const QVariantMap startBounds = editor.boxes().at(0).toMap();

        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, end);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);

        QTRY_COMPARE(window->property("dragMode").toInt(), 0);
        QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
                     startBounds.value(QStringLiteral("x")).toDouble() + 40.0);
        QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(),
                     startBounds.value(QStringLiteral("y")).toDouble() + 25.0);
        const QVariantMap committedBounds = editor.boxes().at(0).toMap();

        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_COMPARE(editor.selectedIndex(), -1);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
                 committedBounds.value(QStringLiteral("x")).toDouble());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(),
                 committedBounds.value(QStringLiteral("y")).toDouble());

        const QPoint cancelStart = delegate->mapToScene(
            QPointF(delegate->width() / 2, delegate->height() / 2)).toPoint();
        QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, cancelStart);
        QTest::mouseMove(window, cancelStart + QPoint(30, 15));
        QVERIFY(QMetaObject::invokeMethod(window, "handleEscape"));
        QTRY_COMPARE(window->property("dragMode").toInt(), 0);
        QTRY_COMPARE(editor.selectedIndex(), -1);
        QCOMPARE(editor.selectedIndices().size(), 0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
                 committedBounds.value(QStringLiteral("x")).toDouble());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(),
                 committedBounds.value(QStringLiteral("y")).toDouble());
        QTRY_COMPARE(documentChanged.count(), 1);

        editor.selectBox(0);
        editor.setSelectedFontSize(31);
        QTRY_COMPARE(documentChanged.count(), 2);
        const QVariantMap finalizedBounds = editor.boxes().at(0).toMap();
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier,
                            cancelStart + QPoint(30, 15));
        QTRY_COMPARE(window->property("dragMode").toInt(), 0);
        QCOMPARE(editor.selectedIndex(), 0);
        QCOMPARE(editor.selectedIndices().size(), 1);
        QCOMPARE(editor.boxes().at(0).toMap(), finalizedBounds);
        QCOMPARE(documentChanged.count(), 2);
      }

      void qmlMultiSelectionRoutesModifiersAndKeepsPrimaryControls() {
        const QString delegateSource =
            readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
        const QString moveSource =
            readQmlFile(QStringLiteral("TextBoxMoveArea.qml"));
        const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
        const QString canvasStateSource =
            readQmlFile(QStringLiteral("CanvasInteractionState.qml"));

        QVERIFY(delegateSource.contains(QStringLiteral(
            "readonly property var selectedIndices: editorRef ? editorRef.selectedIndices : []")));
        QVERIFY(delegateSource.contains(QStringLiteral(
            "readonly property bool selectionMember: selectedIndices.indexOf(boxIndex) >= 0")));
        QVERIFY(delegateSource.contains(QStringLiteral(
            "border.width: !renderSelectionUi || perspectiveActive ? 0 : selectionMember")));
        QVERIFY(moveSource.contains(
            QStringLiteral("controlClickCandidate = mouse.modifiers & Qt.ControlModifier")));
        QVERIFY(moveSource.contains(
            QStringLiteral("controlClickDragged = controlClickDragged || deltaX * deltaX + deltaY * deltaY >=")));
        QVERIFY(!moveSource.contains(QStringLiteral("Qt.ShiftModifier")));
        QVERIFY(moveSource.contains(QStringLiteral("editorRef.toggleBoxSelection(boxRef.boxModel.index)")));
        QVERIFY(delegateSource.contains(
            QStringLiteral("if (eventPoint.modifiers & Qt.ControlModifier)")));
        QVERIFY(delegateSource.contains(
            QStringLiteral("boxDelegate.editorRef.selectBox(boxDelegate.boxModel.index)")));
        QVERIFY(moveSource.contains(QStringLiteral("editorRef.selectBox(boxRef.boxModel.index)")));
        QVERIFY(moveSource.contains(QStringLiteral("onDoubleClicked: (mouse) => {")));
        QVERIFY(moveSource.contains(QStringLiteral("editorRef.beginTextEdit()")));
        QVERIFY(canvasStateSource.contains(QStringLiteral(
            "button === Qt.LeftButton && (modifiers & Qt.ControlModifier)")));
        QVERIFY(mainSource.contains(QStringLiteral("Editor.clearSelection()")));
      }

};

QTEST_MAIN(QmlTextBoxDelegateSmokeTests)

#include "qml_text_box_delegate_smoke_tests.moc"
