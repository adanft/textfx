#include "app/EditorController.h"
#include "qt_test_helpers.h"

#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
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
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *textArea = nullptr;
    QTRY_VERIFY(textArea = findVisualChildByName(
                    window->contentItem(), QStringLiteral("boxTextArea")));
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

    typeText(window, u"abc");
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("abc"));
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());

    typeText(window, u"def");
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("abcdef"));
    QVERIFY(editor.editingText());
    QVERIFY(textArea->property("activeFocus").toBool());

    QVERIFY(window->setProperty("zoom", 2.0));
    QTRY_COMPARE(lineSpacing(), 14.0);

    editor.setSelectedLineSpacing(9);
    QTRY_COMPARE(lineSpacing(), 18.0);
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
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
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

    QVERIFY(textArea->setProperty("text", QStringLiteral("After")));
    QTRY_COMPARE(outlinedText->property("text").toString(),
                 QStringLiteral("After"));
    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("After"));

    editor.endTextEdit();
    QCoreApplication::processEvents();

    QVERIFY(!editor.editingText());
    QTRY_VERIFY(!textArea->property("visible").toBool());
    QCOMPARE(editor.selectedIndex(), 0);
    QCOMPARE(outlinedText->property("text").toString(),
             QStringLiteral("After"));
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
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
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
    QVERIFY(outlinedObject->property("editLayoutMetricsValid").toBool());

    const qreal expectedInset =
        window->property("zoom").toReal() *
        outlinedObject->property("editLayoutLeftPadding").toReal();
    QCOMPARE(textAreaObject->property("leftPadding").toReal(), expectedInset);
    QCOMPARE(textAreaObject->property("rightPadding").toReal(), expectedInset);
    QVERIFY(textAreaObject->property("topPadding").toReal() > expectedInset);

    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    QVERIFY(
        delegateSource.contains(QStringLiteral("wrapMode: TextEdit.WordWrap")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("boxOutlinedText.editLayoutTopPadding")));
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
      qreal zoom;
      int alignment;
      bool expectWrapped;
    };

    const QList<Case> cases{
        {QStringLiteral("outline inset"), QStringLiteral("Outlined text"),
         180, 100, 24, 8, 1.0, 0, false},
        {QStringLiteral("multiline word wrap"),
         QStringLiteral("Alpha Beta Gamma Delta"), 105, 140, 20, 4, 1.0, 0,
         true},
        {QStringLiteral("zoom render scale"), QStringLiteral("Zoomed text"),
         190, 110, 22, 6, 1.75, 0, false},
        {QStringLiteral("right alignment"), QStringLiteral("Right aligned"),
         240, 110, 22, 4, 1.0, 2, false},
    };

    const auto nearlyEqual = [](qreal a, qreal b, qreal tolerance = 0.75) {
      return std::abs(a - b) <= tolerance;
    };

    for (const Case &testCase : cases) {
      EditorController editor;
      editor.newDocument();
      editor.createTextBox(10, 20, testCase.width, testCase.height);
      editor.updateSelectedText(testCase.text);
      editor.setSelectedFontSize(testCase.fontSize);
      editor.setSelectedOutlineEnabled(true);
      editor.setSelectedOutlineSize(testCase.outlineSize);
      editor.setSelectedAlignment(testCase.alignment);
      editor.beginTextEdit();

      QQmlApplicationEngine engine;
      engine.rootContext()->setContextProperty(QStringLiteral("Editor"),
                                               &editor);
      engine.load(QUrl::fromLocalFile(
          QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
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

      const qreal viewScale = textAreaItem->width() / testCase.width;
      const qreal expectedLeftPadding =
          outlinedObject->property("editLayoutLeftPadding").toReal() *
          viewScale;
      const qreal expectedRightPadding =
          outlinedObject->property("editLayoutRightPadding").toReal() *
          viewScale;
      const qreal expectedTopPadding =
          outlinedObject->property("editLayoutTopPadding").toReal() *
          viewScale;
      QVERIFY2(expectedLeftPadding > 0.0,
               qPrintable(testCase.name + QStringLiteral(" left padding")));
      QVERIFY2(expectedTopPadding > expectedLeftPadding,
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

      const qreal expectedTextWidth =
          textAreaItem->width() - expectedLeftPadding - expectedRightPadding;
      QVERIFY2(nearlyEqual(document->textWidth(), expectedTextWidth, 1.0),
               qPrintable(QStringLiteral("%1 document width=%2 expected=%3")
                              .arg(testCase.name)
                              .arg(document->textWidth())
                              .arg(expectedTextWidth)));
      QVERIFY2(nearlyEqual(outlinedItem->width() * outlinedItem->scale(),
                           textAreaItem->width()),
               qPrintable(testCase.name));
      QVERIFY2(nearlyEqual(outlinedItem->height() * outlinedItem->scale(),
                           textAreaItem->height()),
               qPrintable(testCase.name));

      QTextLayout *layout = document->firstBlock().layout();
      QVERIFY(layout);
      if (testCase.expectWrapped)
        QVERIFY2(layout->lineCount() >= 2, qPrintable(testCase.name));

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

      if (testCase.alignment == 2) {
        QCOMPARE(textAreaObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignRight));
        QCOMPARE(outlinedObject->property("horizontalAlignment").toInt(),
                 int(Qt::AlignRight));
        QVERIFY2(cursorRect.x() > expectedLeftPadding,
                 qPrintable(testCase.name));
      }
    }
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

    const qsizetype delegateStart = delegateSource.indexOf(
        QStringLiteral("Rectangle {\n    id: boxDelegate"));
    const qsizetype textPerspectiveStart = delegateSource.indexOf(
        QStringLiteral("id: boxTextPerspective"), delegateStart);
    const qsizetype textAreaStart = delegateSource.indexOf(
        QStringLiteral("id: boxTextArea"), textPerspectiveStart);
    const qsizetype mouseAreaStart =
        delegateSource.indexOf(QStringLiteral("MouseArea {"), textAreaStart);
    QVERIFY(delegateStart >= 0);
    QVERIFY(textPerspectiveStart > delegateStart);
    QVERIFY(textAreaStart > textPerspectiveStart);
    QVERIFY(mouseAreaStart > textAreaStart);
    const QString delegateHeader =
        delegateSource.mid(delegateStart, textAreaStart - delegateStart);
    const QString textPerspectiveSource = delegateSource.mid(
        textPerspectiveStart, textAreaStart - textPerspectiveStart);
    const QString textAreaSource =
        delegateSource.mid(textAreaStart, mouseAreaStart - textAreaStart);

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
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
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
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
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
    editor.createTextBox(4, 4, 42, 24);
    editor.updateSelectedText(
        QStringLiteral("Supercalifragilisticexpialidocious"));
    editor.setSelectedFontSize(20);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *delegate = nullptr;
    QTRY_VERIFY(delegate = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    QTRY_VERIFY(delegate->property("textOverflow").toBool());

    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    QVERIFY(delegateSource.contains(
        QStringLiteral("border.color: textOverflow ? Qt.rgba(1, 0, 0, 1)")));
  }

  void qmlNestedInteractionHandlersUseStableEditorReference() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        delegateSource, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype pathStart = delegateSource.indexOf(
        QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"),
        resizeStart);
    const qsizetype pathEnd = delegateSource.size();
    const qsizetype rotateRectStart =
        delegateSource.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
    const qsizetype rotateStart = delegateSource.indexOf(
        QStringLiteral(
            "rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef"),
        resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateRectStart > resizeStart);
    QVERIFY(rotateStart > resizeStart);
    QVERIFY(pathStart > rotateStart);
    QVERIFY(pathEnd > pathStart);

    const QString resizeSource =
        delegateSource.mid(resizeStart, rotateStart - resizeStart);
    const QString rotateSource =
        delegateSource.mid(rotateRectStart, pathStart - rotateRectStart);
    const QString pathSource =
        delegateSource.mid(pathStart, pathEnd - pathStart);

    QVERIFY(resizeSource.contains(
        QStringLiteral("resizeHandle.editorRef.endInteraction()")));
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
    QVERIFY(!delegateSource.contains(
        QStringLiteral("property var boxRef: boxDelegate")));
    QVERIFY(
        resizeSource.contains(QStringLiteral("property var boxRef: parent")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("property var rootWindow: boxRef.rootWindow")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("property var editorRef: boxRef.editorRef")));
    QVERIFY(
        rotateSource.contains(QStringLiteral("property var boxRef: parent")));
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

  void qmlSelectionGeometryAndRotateHandleUseCurrentVisualBounds() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString geometrySource =
        readQmlFile(QStringLiteral("PerspectiveGeometry.qml"));
    const QString viewportSource =
        readQmlFile(QStringLiteral("ViewportMetrics.qml"));

    QVERIFY(delegateSource.contains(
        QStringLiteral("property bool moveActive: rootWindow.dragMode === "
                       "interaction.dragModeMove && rootWindow.activeMoveIndex "
                       "=== modelData.index")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("property bool resizeActive: rootWindow.dragMode === "
                       "interaction.dragModeResize && "
                       "rootWindow.activeResizeDelegate === boxDelegate")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("property real visualDocW: resizeActive ? "
                       "rootWindow.resizeW : modelData.w")));
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
        QStringLiteral("border.width: perspectiveActive ? 0 : selected ? "
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
    QVERIFY(delegateSource.contains(
        QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, "
                       "boxRef.width, boxRef.height)")));
  }

  void qmlHasEightResizeHandlesAndAngleRotation() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    const QString rotateStateSource =
        readQmlFile(QStringLiteral("BoxRotateInteractionState.qml"));

    for (const QString &handle :
         {QStringLiteral("nw"), QStringLiteral("n"), QStringLiteral("ne"),
          QStringLiteral("e"), QStringLiteral("se"), QStringLiteral("s"),
          QStringLiteral("sw"), QStringLiteral("w")}) {
      QVERIFY2(sourceContainsIgnoringWhitespace(
                   delegateSource,
                   QStringLiteral("name: \"") + handle + QStringLiteral("\"")),
               qPrintable(handle));
    }
    QVERIFY(rotateStateSource.contains(QStringLiteral("Math.atan2")));
    QVERIFY(mainSource.contains(QStringLiteral("BoxRotateInteractionState")));
    QVERIFY(
        mainSource.contains(QStringLiteral("window.editor.setSelectedBounds")));
    QVERIFY(delegateSource.contains(
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
        rotateStateSource.contains(QStringLiteral("function cancelPreview()")));
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
    QVERIFY(endRotate.contains(
        QStringLiteral("window.editor.setSelectedRotation(boxRotateInteraction."
                       "rotateDegrees)")));
    QVERIFY(endRotate.contains(
        QStringLiteral("boxRotateInteraction.cancelPreview()")));
    QVERIFY(endRotate.contains(QStringLiteral("boxRotateInteraction.reset()")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("function angleDeltaDegrees(from, to)")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("activeRotateDelegate.rotation = rotateDegrees")));
    QVERIFY(!updateRotate.contains(QStringLiteral("setSelectedRotation")));
    QVERIFY(
        !delegateSource.contains(QStringLiteral("Editor.setSelectedRotation")));
  }

  void qmlPerspectiveAndRotateDragsCommitOnlyOnRelease() {
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
    QVERIFY(!perspectiveUpdate.contains(
        QStringLiteral("Editor.setPerspectiveHandle")));
    QVERIFY(perspectiveEnd.contains(QStringLiteral(
        "if (dragMode === editorInteraction.dragModePerspective && commit)")));
    QVERIFY(perspectiveEnd.contains(
        QStringLiteral("commitPerspectiveCorner(handles[i].name, handles[i].x, "
                       "handles[i].y)")));
    QVERIFY(rotateStateSource.contains(
        QStringLiteral("activeRotateDelegate.rotation = rotateDegrees")));
    QVERIFY(
        !rotateUpdate.contains(QStringLiteral("Editor.setSelectedRotation")));
    QVERIFY(rotateEnd.contains(QStringLiteral(
        "if (dragMode === editorInteraction.dragModeRotate && commit)")));
    QVERIFY(rotateEnd.contains(
        QStringLiteral("window.editor.setSelectedRotation(boxRotateInteraction."
                       "rotateDegrees)")));
  }
};

QTEST_MAIN(QmlTextBoxDelegateSmokeTests)

#include "qml_text_box_delegate_smoke_tests.moc"
