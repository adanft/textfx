#include "app/EditorController.h"
#include "qt_test_helpers.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace textfx;
using namespace textfx::test;

namespace {
void scrollRightInspectorItemIntoView(QQuickWindow *window, QQuickItem *item) {
  QObject *scrollObject = findVisualChildByName(
      window->contentItem(), QStringLiteral("rightPanelScroll"));
  if (!scrollObject)
    return;
  auto *scrollItem = qobject_cast<QQuickItem *>(scrollObject);
  QObject *contentItem = scrollObject->property("contentItem").value<QObject *>();
  if (!scrollItem || !contentItem ||
      !contentItem->property("contentY").isValid())
    return;

  const QPointF center =
      item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
  const qreal top = scrollItem->mapToScene(QPointF(0, 0)).y();
  const qreal bottom =
      scrollItem->mapToScene(QPointF(0, scrollItem->height())).y();
  qreal contentY = contentItem->property("contentY").toReal();
  if (center.y() < top)
    contentY = std::max<qreal>(0.0, contentY - (top - center.y()) - 24.0);
  else if (center.y() > bottom)
    contentY += center.y() - bottom + 24.0;
  else
    return;

  contentItem->setProperty("contentY", contentY);
  QCoreApplication::processEvents();
}
} // namespace

class QmlInteractionSmokeTests final : public QObject {
  Q_OBJECT

private slots:
  void qmlCentralCanvasShellForwardsViewportInteractions() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *shellObject = nullptr;
    QTRY_VERIFY(
        shellObject = findVisualChildByName(
            window->contentItem(), QStringLiteral("centralCanvasShell")));
    auto *shell = qobject_cast<QQuickItem *>(shellObject);
    QVERIFY(shell);
    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->width() > shell->width());
    QCOMPARE(canvas->height(), shell->height());

    const auto windowCoordinate = [&](const char *method, qreal value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    const auto expectedCreatedBox = [&](const QPointF &start,
                                        const QPointF &end) {
      const QPointF topLeft(qMin(start.x(), end.x()), qMin(start.y(), end.y()));
      return QVariantMap{
          {QStringLiteral("x"),
           windowCoordinate("viewToDocumentX", topLeft.x())},
          {QStringLiteral("y"),
           windowCoordinate("viewToDocumentY", topLeft.y())},
          {QStringLiteral("w"),
           std::abs(windowCoordinate("viewToDocumentX", end.x()) -
                    windowCoordinate("viewToDocumentX", start.x()))},
          {QStringLiteral("h"),
           std::abs(windowCoordinate("viewToDocumentY", end.y()) -
                    windowCoordinate("viewToDocumentY", start.y()))},
      };
    };
    const auto assertBoxGeometry = [](const QVariantMap &actual,
                                      const QVariantMap &expected) {
      const auto nearlyEqual = [](double a, double b) {
        return std::abs(a - b) <= 1.0;
      };
      QVERIFY(nearlyEqual(actual.value(QStringLiteral("x")).toDouble(),
                          expected.value(QStringLiteral("x")).toDouble()));
      QVERIFY(nearlyEqual(actual.value(QStringLiteral("y")).toDouble(),
                          expected.value(QStringLiteral("y")).toDouble()));
      QVERIFY(nearlyEqual(actual.value(QStringLiteral("w")).toDouble(),
                          expected.value(QStringLiteral("w")).toDouble()));
      QVERIFY(nearlyEqual(actual.value(QStringLiteral("h")).toDouble(),
                          expected.value(QStringLiteral("h")).toDouble()));
    };
    const auto ctrlDrag = [&](const QPointF &shellStart, const QPoint &delta) {
      const QPoint start = shell->mapToScene(shellStart).toPoint();
      const QPoint end = start + delta;
      QTest::mousePress(window, Qt::LeftButton, Qt::ControlModifier, start);
      QTest::mouseMove(window, end);
      QTest::mouseRelease(window, Qt::LeftButton, Qt::ControlModifier, end);
      const QPointF canvasStart(start.x(), shellStart.y());
      return expectedCreatedBox(canvasStart, canvasStart + QPointF(delta));
    };

    const QPoint panStart = shell->mapToScene(QPointF(80, 80)).toPoint();
    const QPoint panEnd = panStart + QPoint(32, 18);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, panStart);
    QTest::mouseMove(window, panEnd);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, panEnd);
    QTRY_VERIFY(window->property("panX").toReal() > 20.0);
    QTRY_VERIFY(window->property("panY").toReal() > 10.0);

    const QVariantMap downRight = ctrlDrag(QPointF(160, 140), QPoint(80, 48));
    QTRY_COMPARE(editor.boxes().size(), 1);
    assertBoxGeometry(editor.boxes().at(0).toMap(), downRight);

    const QVariantMap upLeft = ctrlDrag(QPointF(300, 220), QPoint(-80, -48));
    QTRY_COMPARE(editor.boxes().size(), 2);
    assertBoxGeometry(editor.boxes().at(1).toMap(), upLeft);
    QVERIFY(editor.boxes().at(1).toMap().value(QStringLiteral("x")).toDouble() <
            windowCoordinate("viewToDocumentX",
                             shell->mapToScene(QPointF(300, 220)).x()));
    QVERIFY(editor.boxes().at(1).toMap().value(QStringLiteral("y")).toDouble() <
            windowCoordinate("viewToDocumentY", 220.0));

    ctrlDrag(QPointF(260, 180), QPoint(8, 8));
    QCoreApplication::processEvents();
    QCOMPARE(editor.boxes().size(), 2);

    canvas->forceActiveFocus();
    QTRY_VERIFY(canvas->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_COMPARE(editor.selectedIndex(), -1);

    editor.selectBox(0);
    QTRY_COMPARE(editor.selectedIndex(), 0);
    canvas->forceActiveFocus();
    QTRY_VERIFY(canvas->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Delete);
    QTRY_COMPARE(editor.boxes().size(), 1);

    editor.selectBox(0);
    editor.updateSelectedText(QStringLiteral("Keyboard copied box"));
    const auto copiedBox = editor.boxes().at(0).toMap();
    QGuiApplication::clipboard()->clear();
    canvas->forceActiveFocus();
    QTRY_VERIFY(canvas->hasActiveFocus());

    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    QTRY_VERIFY(QGuiApplication::clipboard()->text().contains(
        QStringLiteral("Keyboard copied box")));

    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(editor.boxes().size(), 2);
    const auto pastedBox = editor.boxes().at(1).toMap();
    QCOMPARE(pastedBox.value(QStringLiteral("text")).toString(),
             QStringLiteral("Keyboard copied box"));
    QCOMPARE(pastedBox.value(QStringLiteral("x")).toDouble(),
             copiedBox.value(QStringLiteral("x")).toDouble() + 16.0);
    QCOMPARE(pastedBox.value(QStringLiteral("y")).toDouble(),
             copiedBox.value(QStringLiteral("y")).toDouble() + 16.0);
    QCOMPARE(editor.selectedIndex(), 1);

    const qreal zoomBeforeWheel = window->property("zoom").toReal();
    const QPointF wheelPosition = shell->mapToScene(QPointF(220, 180));
    QWheelEvent wheelEvent(
        wheelPosition, window->mapToGlobal(wheelPosition.toPoint()), QPoint(),
        QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &wheelEvent);
    QVERIFY(wheelEvent.isAccepted());
    QTRY_VERIFY(window->property("zoom").toReal() > zoomBeforeWheel);
  }

  void paintModeDragAddsPaintWithoutSelectingOrCreatingBoxes() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());
    editor.selectBox(-1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->setProperty("zoom", 1.5));

    QObject *paintModeObject = nullptr;
    QTRY_VERIFY(paintModeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintModeCheck")));
    auto *paintMode = qobject_cast<QQuickItem *>(paintModeObject);
    QVERIFY(paintMode);
    scrollRightInspectorItemIntoView(window, paintMode);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      paintMode->mapToScene(QPointF(8, 8)).toPoint());

    QObject *shellObject = nullptr;
    QTRY_VERIFY(shellObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvasShell")));
    auto *shell = qobject_cast<QQuickItem *>(shellObject);
    QVERIFY(shell);

    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QTRY_VERIFY(paintInput->isVisible());
    QTRY_VERIFY(paintInput->width() > 64);
    QTRY_VERIFY(paintInput->height() > 64);

    QObject *paintLayerObject = nullptr;
    QTRY_VERIFY(paintLayerObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintBehindTextLayer")));
    auto *paintLayer = qobject_cast<QQuickItem *>(paintLayerObject);
    QVERIFY(paintLayer);

    QObject *paintPreviewLayerObject = nullptr;
    QTRY_VERIFY(paintPreviewLayerObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("paintBehindTextPreviewLayer")));
    auto *paintPreviewLayer = qobject_cast<QQuickItem *>(paintPreviewLayerObject);
    QVERIFY(paintPreviewLayer);

    QObject *pageImageObject = nullptr;
    QTRY_VERIFY(pageImageObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("pageImage")));
    auto *pageImage = qobject_cast<QQuickItem *>(pageImageObject);
    QVERIFY(pageImage);
    QTRY_VERIFY(pageImage->isVisible());
    QCOMPARE(paintLayer->isVisible(), false);
    QCOMPARE(paintLayer->width(), 320.0);
    QCOMPARE(paintLayer->height(), 240.0);
    QCOMPARE(paintPreviewLayer->isVisible(), false);
    QCOMPARE(paintPreviewLayer->width(), 320.0);
    QCOMPARE(paintPreviewLayer->height(), 240.0);
    QVERIFY(paintInput->width() > paintLayer->width());
    QCOMPARE(paintLayer->scale(),
             window->property("pageBaseScale").toReal() *
                 window->property("zoom").toReal());

    const int initialBoxCount = editor.boxes().size();
    const QPointF outsidePaintCanvasPoint =
        paintInput->x() > 32.0
            ? QPointF(paintInput->x() - 16.0, paintInput->y() + 32.0)
            : QPointF(paintInput->x() + paintInput->width() + 16.0,
                      paintInput->y() + 32.0);
    QVERIFY(canvas->contains(outsidePaintCanvasPoint));
    QVERIFY(!paintInput->contains(
        paintInput->mapFromItem(canvas, outsidePaintCanvasPoint)));

    const QPoint outsideStart =
        canvas->mapToScene(outsidePaintCanvasPoint).toPoint();
    const QPoint outsideEnd = outsideStart + QPoint(24, 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, outsideStart);
    QTest::mouseMove(window, outsideEnd);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, outsideEnd);
    QCoreApplication::processEvents();

    QCOMPARE(editor.paintBehindText().size(), 0);
    QCOMPARE(paintLayer->property("liveStrokeCount").toInt(), 0);
    QCOMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 0);
    QCOMPARE(editor.boxes().size(), initialBoxCount);

    const QPoint start = paintInput->mapToScene(QPointF(32, 32)).toPoint();
    const QPoint end = start + QPoint(40, 0);
    const QPointF canvasStart(paintInput->x() + 32, paintInput->y() + 32);
    const int persistedPaintRevisionBeforePreview =
        paintLayer->property("paintRevision").toInt();
    const int previewPaintRevisionBefore =
        paintPreviewLayer->property("paintRevision").toInt();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(window, end);

    QTRY_VERIFY(paintPreviewLayer->isVisible());
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 1);
    QTRY_VERIFY(paintPreviewLayer->property("paintRevision").toInt() >
                previewPaintRevisionBefore);
    QCOMPARE(editor.paintBehindText().size(), 0);
    QCOMPARE(paintLayer->property("liveStrokeCount").toInt(), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);

    QTRY_COMPARE(editor.paintBehindText().size(), 1);
    QTRY_COMPARE(paintLayer->property("liveStrokeCount").toInt(), 1);
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 0);
    QTRY_VERIFY(paintLayer->property("paintRevision").toInt() >
                persistedPaintRevisionBeforePreview);
    const QVariantMap stroke = editor.paintBehindText().constFirst().toMap();
    const QVariantList points = stroke.value(QStringLiteral("points")).toList();
    QVERIFY(points.size() >= 2);
    const QVariantList firstPoint = points.constFirst().toList();
    QVERIFY(firstPoint.size() >= 2);
    const auto windowCoordinate = [&](const char *method, qreal value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    QVERIFY(std::abs(firstPoint.at(0).toDouble() -
                    windowCoordinate("viewToDocumentX", canvasStart.x())) <=
            0.5);
    QVERIFY(std::abs(firstPoint.at(1).toDouble() -
                    windowCoordinate("viewToDocumentY", canvasStart.y())) <=
            0.5);
    QCOMPARE(editor.boxes().size(), initialBoxCount);
    QCOMPARE(editor.selectedIndex(), -1);
  }

  void paintColorDialogSelectionUpdatesBrushAndStrokeColor() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());
    editor.selectBox(-1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QCOMPARE(rightPanelObject->property("paintBrushColor").toString(),
             QStringLiteral("ff0000ff"));

    QObject *paintColorButtonObject = nullptr;
    QTRY_VERIFY(paintColorButtonObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintColorButton")));
    auto *paintColorButton = qobject_cast<QQuickItem *>(paintColorButtonObject);
    QVERIFY(paintColorButton);
    scrollRightInspectorItemIntoView(window, paintColorButton);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      paintColorButton
                          ->mapToScene(QPointF(paintColorButton->width() / 2.0,
                                               paintColorButton->height() / 2.0))
                          .toPoint());

    QObject *chromeObject = nullptr;
    QTRY_VERIFY(chromeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("editorChrome")));
    QTRY_COMPARE(chromeObject->property("colorDialogSetter").toString(),
                 QStringLiteral("paint"));

    const QString selectedColor = QStringLiteral("#00ff88");
    QVERIFY(QMetaObject::invokeMethod(chromeObject, "applyColorDialogSelection",
                                      Q_ARG(QVariant, selectedColor)));
    QObject *colorDialogObject = chromeObject->findChild<QObject *>(
        QStringLiteral("colorDialog"));
    QVERIFY(colorDialogObject);
    QVERIFY(QMetaObject::invokeMethod(colorDialogObject, "close"));
    QTRY_COMPARE(rightPanelObject->property("paintBrushColor").toString(),
                 selectedColor);

    QObject *paintModeObject = nullptr;
    QTRY_VERIFY(paintModeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintModeCheck")));
    auto *paintMode = qobject_cast<QQuickItem *>(paintModeObject);
    QVERIFY(paintMode);
    scrollRightInspectorItemIntoView(window, paintMode);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      paintMode->mapToScene(QPointF(8, 8)).toPoint());
    QTRY_VERIFY(rightPanelObject->property("paintMode").toBool());

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QTRY_VERIFY(paintInput->isVisible());
    QTRY_VERIFY(paintInput->width() > 64);
    QTRY_VERIFY(paintInput->height() > 64);

    const QPoint start = paintInput->mapToScene(QPointF(32, 32)).toPoint();
    const QPoint end = start + QPoint(40, 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(window, end);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);

    QTRY_COMPARE(editor.paintBehindText().size(), 1);
    const QVariantMap stroke = editor.paintBehindText().constFirst().toMap();
    QCOMPARE(stroke.value(QStringLiteral("color")).toString(),
             QStringLiteral("00ff88ff"));
  }

  void rapidPaintDragThrottlesPreviewAndPersistsEveryPoint() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());
    editor.selectBox(-1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QVERIFY(rightPanelObject->setProperty("paintMode", true));

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QTRY_VERIFY(paintInput->isVisible());

    QObject *paintLayerObject = nullptr;
    QTRY_VERIFY(paintLayerObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintBehindTextLayer")));
    auto *paintLayer = qobject_cast<QQuickItem *>(paintLayerObject);
    QVERIFY(paintLayer);
    QCOMPARE(paintLayer->isVisible(), false);
    QCOMPARE(paintLayer->property("hasPaintContent").toBool(), false);
    QCOMPARE(paintLayer->property("liveStrokeCount").toInt(), 0);

    QObject *paintPreviewLayerObject = nullptr;
    QTRY_VERIFY(paintPreviewLayerObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("paintBehindTextPreviewLayer")));
    auto *paintPreviewLayer = qobject_cast<QQuickItem *>(paintPreviewLayerObject);
    QVERIFY(paintPreviewLayer);
    QCOMPARE(paintPreviewLayer->isVisible(), false);
    QCOMPARE(paintPreviewLayer->property("hasPaintContent").toBool(), false);
    QCOMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 0);

    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);

    const QPointF start = paintInput->mapToItem(canvas, QPointF(32, 32));
    QVariant beginResult;
    QVERIFY(QMetaObject::invokeMethod(window, "beginPaintDrag",
                                      Q_RETURN_ARG(QVariant, beginResult),
                                      Q_ARG(QVariant, start.x()),
                                      Q_ARG(QVariant, start.y())));
    QVERIFY(beginResult.toBool());

    const int publishRevisionAfterBegin =
        window->property("paintPreviewPublishRevision").toInt();
    QVERIFY(publishRevisionAfterBegin > 0);
    QCOMPARE(editor.paintBehindText().size(), 0);
    QTRY_VERIFY(paintPreviewLayer->isVisible());
    QTRY_COMPARE(paintPreviewLayer->property("hasPaintContent").toBool(), true);
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 1);

    constexpr int firstMoveCount = 20;
    for (int i = 1; i <= firstMoveCount; ++i) {
      QVERIFY(QMetaObject::invokeMethod(window, "updatePaintDrag",
                                        Q_ARG(QVariant, start.x() + i),
                                        Q_ARG(QVariant, start.y())));
    }
    QCOMPARE(window->property("paintPreviewPublishRevision").toInt(),
             publishRevisionAfterBegin);
    QCOMPARE(editor.paintBehindText().size(), 0);
    QCOMPARE(paintLayer->property("liveStrokeCount").toInt(), 0);
    QCOMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 1);

    QTRY_VERIFY(window->property("paintPreviewPublishRevision").toInt() >
                publishRevisionAfterBegin);
    const int publishRevisionAfterTimer =
        window->property("paintPreviewPublishRevision").toInt();
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 1);

    constexpr int secondMoveCount = 15;
    for (int i = 1; i <= secondMoveCount; ++i) {
      QVERIFY(QMetaObject::invokeMethod(
          window, "updatePaintDrag",
          Q_ARG(QVariant, start.x() + firstMoveCount + i),
          Q_ARG(QVariant, start.y())));
    }
    QCOMPARE(window->property("paintPreviewPublishRevision").toInt(),
             publishRevisionAfterTimer);

    QVariant endResult;
    QVERIFY(QMetaObject::invokeMethod(window, "endPaintDrag",
                                      Q_RETURN_ARG(QVariant, endResult)));
    QVERIFY(endResult.toBool());
    QCOMPARE(window->property("paintPreviewPublishRevision").toInt(),
             publishRevisionAfterTimer + 1);

    QTRY_COMPARE(editor.paintBehindText().size(), 1);
    QTRY_VERIFY(paintLayer->isVisible());
    QTRY_COMPARE(paintLayer->property("hasPaintContent").toBool(), true);
    QTRY_COMPARE(paintLayer->property("liveStrokeCount").toInt(), 1);
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 0);
    const QVariantMap stroke = editor.paintBehindText().constFirst().toMap();
    const QVariantList points = stroke.value(QStringLiteral("points")).toList();
    QCOMPARE(points.size(), 1 + firstMoveCount + secondMoveCount);
  }

  void paintDragDoesNotCommitToPageReachedDuringDrag() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page1.png"))));
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page2.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());
    editor.selectBox(-1);
    QCOMPARE(editor.pageCount(), 2);
    QCOMPARE(editor.currentPageIndex(), 0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QVERIFY(rightPanelObject->setProperty("paintMode", true));

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QTRY_VERIFY(paintInput->isVisible());

    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);

    QObject *nextPageButtonObject = nullptr;
    QTRY_VERIFY(nextPageButtonObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorNextPageButton")));
    QCOMPARE(nextPageButtonObject->property("enabled").toBool(), true);

    const QPointF start = paintInput->mapToItem(canvas, QPointF(32, 32));
    QVariant beginResult;
    QVERIFY(QMetaObject::invokeMethod(window, "beginPaintDrag",
                                      Q_RETURN_ARG(QVariant, beginResult),
                                      Q_ARG(QVariant, start.x()),
                                      Q_ARG(QVariant, start.y())));
    QVERIFY(beginResult.toBool());
    QTRY_COMPARE(nextPageButtonObject->property("enabled").toBool(), false);

    QVERIFY(QMetaObject::invokeMethod(window, "updatePaintDrag",
                                      Q_ARG(QVariant, start.x() + 40),
                                      Q_ARG(QVariant, start.y())));

    editor.nextPage();
    QCOMPARE(editor.currentPageIndex(), 1);

    QVariant endResult;
    QVERIFY(QMetaObject::invokeMethod(window, "endPaintDrag",
                                      Q_RETURN_ARG(QVariant, endResult)));
    QVERIFY(endResult.toBool());

    QCOMPARE(editor.paintBehindText().size(), 0);
    editor.previousPage();
    QCOMPARE(editor.currentPageIndex(), 0);
    QCOMPARE(editor.paintBehindText().size(), 0);
    QTRY_COMPARE(nextPageButtonObject->property("enabled").toBool(), true);
  }

  void eraserDragDoesNotErasePageReachedDuringDrag() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page1.png"))));
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page2.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());
    editor.selectBox(-1);
    QCOMPARE(editor.pageCount(), 2);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QVERIFY(rightPanelObject->setProperty("paintMode", true));
    QVERIFY(rightPanelObject->setProperty("paintEraserMode", true));

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QTRY_VERIFY(paintInput->isVisible());

    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);

    const QPointF start = paintInput->mapToItem(canvas, QPointF(32, 32));
    const QPointF eraseAt(start.x() + 40, start.y());
    auto documentCoordinate = [&](const char *method, qreal value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    const double strokeX = documentCoordinate("viewToDocumentX", eraseAt.x());
    const double strokeY = documentCoordinate("viewToDocumentY", eraseAt.y());
    QVERIFY(std::isfinite(strokeX));
    QVERIFY(std::isfinite(strokeY));
    QVariantList point;
    point << strokeX << strokeY;
    QVariantList strokePoints;
    strokePoints << QVariant::fromValue(point);

    editor.nextPage();
    QCOMPARE(editor.currentPageIndex(), 1);
    editor.addPaintStroke(QStringLiteral("behind_text"), QStringLiteral("ff0000ff"),
                          12.0, 1.0, strokePoints);
    QCOMPARE(editor.paintBehindText().size(), 1);
    editor.previousPage();
    QCOMPARE(editor.currentPageIndex(), 0);

    QVariant beginResult;
    QVERIFY(QMetaObject::invokeMethod(window, "beginPaintDrag",
                                      Q_RETURN_ARG(QVariant, beginResult),
                                      Q_ARG(QVariant, start.x()),
                                      Q_ARG(QVariant, start.y())));
    QVERIFY(beginResult.toBool());

    editor.nextPage();
    QCOMPARE(editor.currentPageIndex(), 1);
    QVERIFY(QMetaObject::invokeMethod(window, "updatePaintDrag",
                                      Q_ARG(QVariant, eraseAt.x()),
                                      Q_ARG(QVariant, eraseAt.y())));

    QCOMPARE(editor.paintBehindText().size(), 1);
    QCOMPARE(window->property("activePaintPageIndex").toInt(), -1);
    QCOMPARE(window->property("activePaintTarget").toString(), QString());
  }

  void cancelPaintDragDoesNotCommitAndClearsPreview() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QVERIFY(rightPanelObject->setProperty("paintMode", true));

    QObject *paintLayerObject = nullptr;
    QTRY_VERIFY(paintLayerObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintBehindTextLayer")));
    auto *paintLayer = qobject_cast<QQuickItem *>(paintLayerObject);
    QVERIFY(paintLayer);

    QObject *paintPreviewLayerObject = nullptr;
    QTRY_VERIFY(paintPreviewLayerObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("paintBehindTextPreviewLayer")));
    auto *paintPreviewLayer = qobject_cast<QQuickItem *>(paintPreviewLayerObject);
    QVERIFY(paintPreviewLayer);

    QVariant beginResult;
    QVERIFY(QMetaObject::invokeMethod(window, "beginPaintDrag",
                                      Q_RETURN_ARG(QVariant, beginResult),
                                      Q_ARG(QVariant, 32.0),
                                      Q_ARG(QVariant, 32.0)));
    QVERIFY(beginResult.toBool());
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 1);

    QVERIFY(QMetaObject::invokeMethod(window, "updatePaintDrag",
                                      Q_ARG(QVariant, 48.0),
                                      Q_ARG(QVariant, 32.0)));
    const int revisionBeforeCancel =
        window->property("paintPreviewPublishRevision").toInt();
    QVariant cancelResult;
    QVERIFY(QMetaObject::invokeMethod(window, "cancelPaintDrag",
                                      Q_RETURN_ARG(QVariant, cancelResult)));
    QVERIFY(cancelResult.toBool());

    QCOMPARE(editor.paintBehindText().size(), 0);
    QCOMPARE(window->property("paintPreviewPublishRevision").toInt(),
             revisionBeforeCancel);
    QTRY_COMPARE(window->property("activePaintPoints").toList().size(), 0);
    QTRY_COMPARE(window->property("activePaintPreviewPoints").toList().size(), 0);
    QTRY_COMPARE(window->property("activePaintTarget").toString(), QString());
    QTRY_COMPARE(window->property("activePaintPageIndex").toInt(), -1);
    QTRY_COMPARE(paintLayer->property("liveStrokeCount").toInt(), 0);
    QTRY_COMPARE(paintLayer->property("hasPaintContent").toBool(), false);
    QTRY_COMPARE(paintPreviewLayer->property("liveStrokeCount").toInt(), 0);
    QTRY_COMPARE(paintPreviewLayer->property("hasPaintContent").toBool(), false);
  }

  void escapeCancelsPaintDragWithoutCommit() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QVERIFY(rightPanelObject->setProperty("paintMode", true));

    QVariant beginResult;
    QVERIFY(QMetaObject::invokeMethod(window, "beginPaintDrag",
                                      Q_RETURN_ARG(QVariant, beginResult),
                                      Q_ARG(QVariant, 32.0),
                                      Q_ARG(QVariant, 32.0)));
    QVERIFY(beginResult.toBool());
    QVERIFY(QMetaObject::invokeMethod(window, "updatePaintDrag",
                                      Q_ARG(QVariant, 48.0),
                                      Q_ARG(QVariant, 32.0)));

    QVERIFY(QMetaObject::invokeMethod(window, "handleEscape"));

    QCOMPARE(editor.paintBehindText().size(), 0);
    QTRY_COMPARE(window->property("activePaintPoints").toList().size(), 0);
    QTRY_COMPARE(window->property("activePaintPreviewPoints").toList().size(), 0);
    QTRY_COMPARE(window->property("activePaintTarget").toString(), QString());
    QTRY_COMPARE(window->property("activePaintPageIndex").toInt(), -1);
  }

  void paintToolDefaultsLoadWithTwelvePixelBrushAndEraser() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *rightPanelObject = nullptr;
    QTRY_VERIFY(rightPanelObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("rightInspectorPanel")));
    QCOMPARE(rightPanelObject->property("paintBrushSize").toDouble(), 12.0);
    QCOMPARE(rightPanelObject->property("paintEraserSize").toDouble(), 12.0);

    QObject *brushSizeObject = nullptr;
    QTRY_VERIFY(brushSizeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintBrushSizeSpin")));
    QCOMPARE(brushSizeObject->property("value").toDouble(), 12.0);

    QObject *eraserSizeObject = nullptr;
    QTRY_VERIFY(eraserSizeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintEraserSizeSpin")));
    QCOMPARE(eraserSizeObject->property("value").toDouble(), 12.0);
  }

  void paintModeCursorChangesOnlyOverPaintCanvas() {
    registerQmlTypes();

    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());
    QImage page(320, 240, QImage::Format_RGB32);
    page.fill(Qt::white);
    QVERIFY(page.save(projectDir.filePath(QStringLiteral("page.png"))));

    EditorController editor;
    editor.openProject(projectDir.path());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1200);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *paintInputObject = nullptr;
    QTRY_VERIFY(paintInputObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintInputArea")));
    auto *paintInput = qobject_cast<QQuickItem *>(paintInputObject);
    QVERIFY(paintInput);
    QVERIFY(!paintInput->isVisible());
    QCOMPARE(paintInput->property("cursorShape").toInt(),
             static_cast<int>(Qt::CrossCursor));

    QObject *paintModeObject = nullptr;
    QTRY_VERIFY(paintModeObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintModeCheck")));
    auto *paintMode = qobject_cast<QQuickItem *>(paintModeObject);
    QVERIFY(paintMode);
    scrollRightInspectorItemIntoView(window, paintMode);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      paintMode->mapToScene(QPointF(8, 8)).toPoint());

    QTRY_VERIFY(paintInput->isVisible());
    QTRY_VERIFY(paintInput->width() > 16);
    QTRY_VERIFY(paintInput->height() > 16);
    QCOMPARE(paintInput->property("cursorShape").toInt(),
             static_cast<int>(Qt::CrossCursor));
    QVERIFY(paintInput->contains(QPointF(8, 8)));
    QVERIFY(!paintInput->contains(QPointF(paintInput->width() + 8, 8)));
  }

  void aboveTextPaintLayerStaysAboveTextContentButBelowSelectionUi() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.selectBox(0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *paintObject = nullptr;
    QTRY_VERIFY(paintObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("paintAboveTextLayer")));
    auto *paintLayer = qobject_cast<QQuickItem *>(paintObject);
    QVERIFY(paintLayer);

    QObject *paintPreviewObject = nullptr;
    QTRY_VERIFY(paintPreviewObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("paintAboveTextPreviewLayer")));
    auto *paintPreviewLayer = qobject_cast<QQuickItem *>(paintPreviewObject);
    QVERIFY(paintPreviewLayer);

    QObject *contentDelegateObject = nullptr;
    QTRY_VERIFY(contentDelegateObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxContentDelegate")));
    auto *contentDelegate = qobject_cast<QQuickItem *>(contentDelegateObject);
    QVERIFY(contentDelegate);

    QObject *uiDelegateObject = nullptr;
    QTRY_VERIFY(uiDelegateObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    auto *uiDelegate = qobject_cast<QQuickItem *>(uiDelegateObject);
    QVERIFY(uiDelegate);

    QObject *resizeHandleObject = nullptr;
    QTRY_VERIFY(resizeHandleObject = findVisualChildByName(
                    uiDelegate, QStringLiteral("resizeHandle_nw")));
    auto *resizeHandle = qobject_cast<QQuickItem *>(resizeHandleObject);
    QVERIFY(resizeHandle);

    QVERIFY(paintLayer->z() > contentDelegate->z());
    QVERIFY(paintPreviewLayer->z() >= paintLayer->z());
    QVERIFY(paintPreviewLayer->z() > contentDelegate->z());
    QVERIFY(uiDelegate->z() > paintLayer->z());
    QVERIFY(uiDelegate->z() > paintPreviewLayer->z());
    QVERIFY(resizeHandle->isVisible());
  }

  void qmlViewportMetricsConvertsCoordinatesAndZoomsAroundFocus() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *viewport =
        window->findChild<QObject *>(QStringLiteral("viewportMetrics"));
    QVERIFY(viewport);
    QVERIFY(viewport->setProperty("canvasWidth", 500.0));
    QVERIFY(viewport->setProperty("canvasHeight", 400.0));
    QVERIFY(viewport->setProperty("pageSourceWidth", 200.0));
    QVERIFY(viewport->setProperty("pageSourceHeight", 100.0));
    QVERIFY(window->setProperty("pageBaseScale", 0.5));
    QVERIFY(window->setProperty("zoom", 2.0));
    QVERIFY(window->setProperty("panX", 10.0));
    QVERIFY(window->setProperty("panY", 20.0));

    const auto callWindow = [&](const char *method, double value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };

    QCOMPARE(callWindow("documentToViewLength", 12.0), 12.0);
    QCOMPARE(callWindow("documentToViewX", 10.0), 420.0);
    QCOMPARE(callWindow("documentToViewY", 30.0), 400.0);
    QCOMPARE(callWindow("viewToDocumentX", 420.0), 10.0);
    QCOMPARE(callWindow("viewToDocumentY", 400.0), 30.0);

    QVERIFY(QMetaObject::invokeMethod(window, "zoomAt", Q_ARG(QVariant, 100.0),
                                      Q_ARG(QVariant, 80.0),
                                      Q_ARG(QVariant, 1.5)));
    QCOMPARE(window->property("zoom").toDouble(), 3.0);
    QCOMPARE(window->property("panX").toDouble(), -35.0);
    QCOMPARE(window->property("panY").toDouble(), -10.0);

    QVERIFY(window->setProperty("zoom", 6.0));
    QVERIFY(window->setProperty("panX", 14.0));
    QVERIFY(window->setProperty("panY", 18.0));
    QVERIFY(QMetaObject::invokeMethod(window, "zoomAt", Q_ARG(QVariant, 100.0),
                                      Q_ARG(QVariant, 80.0),
                                      Q_ARG(QVariant, 1.5)));
    QCOMPARE(window->property("zoom").toDouble(), 6.0);
    QCOMPARE(window->property("panX").toDouble(), 14.0);
    QCOMPARE(window->property("panY").toDouble(), 18.0);
  }

  void qmlBoxResizeInteractionStateCommitsVisibleBoundsAndClamps() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.rotateSelected(30.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *boxObject = nullptr;
    QTRY_VERIFY(boxObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginResizeDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)),
        Q_ARG(QVariant, QStringLiteral("e")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateResizeDrag",
                                      Q_ARG(QVariant, 30.0),
                                      Q_ARG(QVariant, 0.0)));
    QTRY_VERIFY(window->property("resizeW").toDouble() > 120.0);
    QTRY_VERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("w")).toDouble() >
                120.0);
    QVERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble() >
            30.0);
    QVERIFY(QMetaObject::invokeMethod(window, "endResizeDrag",
                                      Q_ARG(QVariant, true)));

    QVariantMap grown = editor.boxes().at(0).toMap();
    QVERIFY(grown.value(QStringLiteral("w")).toDouble() > 120.0);
    QVERIFY(grown.value(QStringLiteral("x")).toDouble() > 30.0);
    QVERIFY(grown.value(QStringLiteral("y")).toDouble() > 40.0);

    boxObject = nullptr;
    QTRY_VERIFY(boxObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    QVERIFY(QMetaObject::invokeMethod(
        window, "beginResizeDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)),
        Q_ARG(QVariant, QStringLiteral("e")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateResizeDrag",
                                      Q_ARG(QVariant, -1000.0),
                                      Q_ARG(QVariant, 0.0)));
    QTRY_COMPARE(window->property("resizeW").toDouble(), 12.0);
    QVERIFY(QMetaObject::invokeMethod(window, "endResizeDrag",
                                      Q_ARG(QVariant, true)));

    const QVariantMap clamped = editor.boxes().at(0).toMap();
    QCOMPARE(clamped.value(QStringLiteral("w")).toDouble(), 12.0);
    QCOMPARE(clamped.value(QStringLiteral("h")).toDouble(), 60.0);
  }

  void qmlBoxMoveInteractionStateCommitsVisiblePositionAndCancels() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *boxObject = nullptr;
    QTRY_VERIFY(boxObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    auto *boxItem = qobject_cast<QQuickItem *>(boxObject);
    QVERIFY(boxItem);

    const QVariantMap original = editor.boxes().at(0).toMap();
    const qreal originalVisualX = boxItem->x();
    const qreal originalVisualY = boxItem->y();

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginMoveDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)), Q_ARG(QVariant, 0),
        Q_ARG(QVariant, 0.0), Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateMoveDrag",
                                      Q_ARG(QVariant, 25.0),
                                      Q_ARG(QVariant, -10.0)));
    QTRY_COMPARE(window->property("moveX").toDouble(), 65.0);
    QTRY_COMPARE(window->property("moveY").toDouble(), 40.0);
    QTRY_VERIFY(boxItem->x() > originalVisualX + 20.0);
    QTRY_VERIFY(boxItem->y() < originalVisualY - 5.0);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
             original.value(QStringLiteral("x")).toDouble() + 25.0);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(),
             original.value(QStringLiteral("y")).toDouble() - 10.0);

    QVERIFY(QMetaObject::invokeMethod(window, "endMoveDrag",
                                      Q_ARG(QVariant, false)));
    QCoreApplication::processEvents();
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(),
             original.value(QStringLiteral("x")).toDouble());
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(),
             original.value(QStringLiteral("y")).toDouble());

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginMoveDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)), Q_ARG(QVariant, 0),
        Q_ARG(QVariant, 0.0), Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateMoveDrag",
                                      Q_ARG(QVariant, 25.0),
                                      Q_ARG(QVariant, -10.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "endMoveDrag",
                                      Q_ARG(QVariant, true)));

    const QVariantMap moved = editor.boxes().at(0).toMap();
    QCOMPARE(moved.value(QStringLiteral("x")).toDouble(),
             original.value(QStringLiteral("x")).toDouble() + 25.0);
    QCOMPARE(moved.value(QStringLiteral("y")).toDouble(),
             original.value(QStringLiteral("y")).toDouble() - 10.0);
  }

  void qmlBoxRotateInteractionStatePreviewsCancelsAndCommits() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.setSelectedRotation(10.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *boxObject = nullptr;
    QTRY_VERIFY(boxObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    auto *boxItem = qobject_cast<QQuickItem *>(boxObject);
    QVERIFY(boxItem);

    const auto documentToView = [&](const char *method, double value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    const auto nearlyEqual = [](double a, double b) {
      return std::abs(a - b) <= 0.001;
    };
    const double centerX = 90.0;
    const double centerY = 80.0;
    const double startX = documentToView("documentToViewX", centerX);
    const double startY = documentToView("documentToViewY", 0.0);
    const double endX = documentToView("documentToViewX", 190.0);
    const double endY = documentToView("documentToViewY", centerY);

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginRotateDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)),
        Q_ARG(QVariant, startX), Q_ARG(QVariant, startY)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag",
                                      Q_ARG(QVariant, endX),
                                      Q_ARG(QVariant, endY)));
    QTRY_VERIFY(
        nearlyEqual(window->property("rotateDegrees").toDouble(), 100.0));
    QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 100.0));
    QCOMPARE(editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("rotation"))
                 .toDouble(),
             100.0);

    QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag",
                                      Q_ARG(QVariant, false)));
    QCoreApplication::processEvents();
    QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 10.0));
    QCOMPARE(editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("rotation"))
                 .toDouble(),
             10.0);

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginRotateDrag",
        Q_ARG(QVariant, QVariant::fromValue(boxObject)),
        Q_ARG(QVariant, startX), Q_ARG(QVariant, startY)));
    QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag",
                                      Q_ARG(QVariant, endX),
                                      Q_ARG(QVariant, endY)));
    QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag",
                                      Q_ARG(QVariant, true)));
    QTRY_VERIFY(nearlyEqual(editor.boxes()
                                .at(0)
                                .toMap()
                                .value(QStringLiteral("rotation"))
                                .toDouble(),
                            100.0));
  }

  void qmlBoxRotateInteractionStateWrapsAcrossAngleBoundary() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.setSelectedRotation(10.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *boxObject = nullptr;
    QTRY_VERIFY(boxObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("textBoxDelegate")));
    auto *boxItem = qobject_cast<QQuickItem *>(boxObject);
    QVERIFY(boxItem);

    const auto documentToView = [&](const char *method, double value) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          window, method, Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, value));
      return invoked ? result.toDouble()
                     : std::numeric_limits<double>::quiet_NaN();
    };
    const auto nearlyEqual = [](double a, double b) {
      return std::abs(a - b) <= 0.001;
    };
    const QPointF center(90.0, 80.0);
    constexpr double pi = 3.14159265358979323846;
    const double radius = 100.0;
    const auto viewPointAtAngle = [&](double degrees) {
      const double radians = degrees * pi / 180.0;
      const double documentX = center.x() + std::cos(radians) * radius;
      const double documentY = center.y() + std::sin(radians) * radius;
      return QPointF(documentToView("documentToViewX", documentX),
                     documentToView("documentToViewY", documentY));
    };
    const auto dragPreview = [&](double fromDegrees, double toDegrees) {
      const QPointF start = viewPointAtAngle(fromDegrees);
      const QPointF end = viewPointAtAngle(toDegrees);
      QVERIFY(QMetaObject::invokeMethod(
          window, "beginRotateDrag",
          Q_ARG(QVariant, QVariant::fromValue(boxObject)),
          Q_ARG(QVariant, start.x()), Q_ARG(QVariant, start.y())));
      QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag",
                                        Q_ARG(QVariant, end.x()),
                                        Q_ARG(QVariant, end.y())));
    };

    dragPreview(179.0, -179.0);
    QTRY_VERIFY(
        nearlyEqual(window->property("rotateDegrees").toDouble(), 12.0));
    QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 12.0));
    QCOMPARE(editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("rotation"))
                 .toDouble(),
             12.0);
    QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag",
                                      Q_ARG(QVariant, false)));
    QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 10.0));

    dragPreview(-179.0, 179.0);
    QTRY_VERIFY(nearlyEqual(window->property("rotateDegrees").toDouble(), 8.0));
    QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 8.0));
    QCOMPARE(editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("rotation"))
                 .toDouble(),
             8.0);
    QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag",
                                      Q_ARG(QVariant, true)));
    QTRY_VERIFY(nearlyEqual(editor.boxes()
                                .at(0)
                                .toMap()
                                .value(QStringLiteral("rotation"))
                                .toDouble(),
                            8.0));
  }

  void qmlPerspectiveGeometryCalculatesHandlesBoundsAndHitArea() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *geometry =
        window->findChild<QObject *>(QStringLiteral("perspectiveGeometry"));
    QVERIFY(geometry);
    QVERIFY(geometry->setProperty("documentScale", 2.0));
    QVERIFY(geometry->setProperty("handleSize", 12.0));

    const QVariantMap box{
        {QStringLiteral("index"), 0},
        {QStringLiteral("perspective"), true},
        {QStringLiteral("perspectiveNw"), QVariantList{10.0, -5.0}},
        {QStringLiteral("perspectiveNe"), QVariantList{-20.0, 5.0}},
        {QStringLiteral("perspectiveSe"), QVariantList{15.0, 10.0}},
        {QStringLiteral("perspectiveSw"), QVariantList{-5.0, 20.0}},
    };

    QVariant northHandle;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "visualHandlePosition", Q_RETURN_ARG(QVariant, northHandle),
        Q_ARG(QVariant, box), Q_ARG(QVariant, QStringLiteral("n")),
        Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 100.0)));
    const QVariantMap north = northHandle.toMap();
    QCOMPARE(north.value(QStringLiteral("x")).toDouble(), 90.0);
    QCOMPARE(north.value(QStringLiteral("y")).toDouble(), 0.0);

    QVariant boundsValue;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "perspectiveVisualBounds",
        Q_RETURN_ARG(QVariant, boundsValue), Q_ARG(QVariant, box),
        Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 100.0)));
    const QVariantMap bounds = boundsValue.toMap();
    QVERIFY(bounds.value(QStringLiteral("x")).toDouble() < 0.0);
    QVERIFY(bounds.value(QStringLiteral("y")).toDouble() < 0.0);
    QVERIFY(bounds.value(QStringLiteral("width")).toDouble() > 200.0);
    QVERIFY(bounds.value(QStringLiteral("height")).toDouble() > 100.0);

    const QVariantMap insidePoint{{QStringLiteral("x"), 100.0},
                                  {QStringLiteral("y"), 50.0}};
    QVariant inside;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "pointInPerspectivePolygon", Q_RETURN_ARG(QVariant, inside),
        Q_ARG(QVariant, insidePoint), Q_ARG(QVariant, box),
        Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 100.0)));
    QVERIFY(inside.toBool());

    const QVariantMap outsidePoint{{QStringLiteral("x"), -30.0},
                                   {QStringLiteral("y"), -30.0}};
    QVariant outside;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "pointInPerspectivePolygon", Q_RETURN_ARG(QVariant, outside),
        Q_ARG(QVariant, outsidePoint), Q_ARG(QVariant, box),
        Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 100.0)));
    QVERIFY(!outside.toBool());

    QVERIFY(geometry->setProperty("livePerspectiveActive", true));
    QVERIFY(geometry->setProperty("activePerspectiveBoxIndex", 0));
    QVERIFY(
        geometry->setProperty("activePerspectiveHandle", QStringLiteral("ne")));
    QVERIFY(geometry->setProperty("perspectiveX", 42.0));
    QVERIFY(geometry->setProperty("perspectiveY", -7.0));

    const QVariantMap otherBox{
        {QStringLiteral("index"), 1},
        {QStringLiteral("perspective"), true},
        {QStringLiteral("perspectiveNw"), QVariantList{10.0, -5.0}},
        {QStringLiteral("perspectiveNe"), QVariantList{-20.0, 5.0}},
        {QStringLiteral("perspectiveSe"), QVariantList{15.0, 10.0}},
        {QStringLiteral("perspectiveSw"), QVariantList{-5.0, 20.0}},
    };
    QVariant selectedLiveCorner;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "perspectiveLayoutCorner",
        Q_RETURN_ARG(QVariant, selectedLiveCorner), Q_ARG(QVariant, box),
        Q_ARG(QVariant, QStringLiteral("ne")), Q_ARG(QVariant, 100.0),
        Q_ARG(QVariant, 50.0)));
    QVariant otherCorner;
    QVERIFY(QMetaObject::invokeMethod(
        geometry, "perspectiveLayoutCorner",
        Q_RETURN_ARG(QVariant, otherCorner), Q_ARG(QVariant, otherBox),
        Q_ARG(QVariant, QStringLiteral("ne")), Q_ARG(QVariant, 100.0),
        Q_ARG(QVariant, 50.0)));
    QCOMPARE(selectedLiveCorner.toMap().value(QStringLiteral("x")).toDouble(),
             142.0);
    QCOMPARE(selectedLiveCorner.toMap().value(QStringLiteral("y")).toDouble(),
             -7.0);
    QCOMPARE(otherCorner.toMap().value(QStringLiteral("x")).toDouble(), 80.0);
    QCOMPARE(otherCorner.toMap().value(QStringLiteral("y")).toDouble(), 5.0);
  }

  void
  qmlPerspectiveInteractionStatePreviewsCancelsCommitsAndScopesSelectedBox() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.setSelectedPerspectiveEnabled(true);
    editor.createTextBox(200, 50, 100, 60);
    editor.setSelectedPerspectiveEnabled(true);
    editor.selectBox(0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    const auto nearlyEqual = [](double a, double b) {
      return std::abs(a - b) <= 0.001;
    };
    const auto collectDelegates = [](auto &&self, QQuickItem *item,
                                     QList<QObject *> &results) -> void {
      if (!item)
        return;
      if (item->objectName() == QStringLiteral("textBoxDelegate"))
        results.append(item);
      for (QQuickItem *child : item->childItems())
        self(self, child, results);
    };
    const auto modelIndex = [](QObject *delegate) {
      return delegate->property("boxModel")
          .toMap()
          .value(QStringLiteral("index"))
          .toInt();
    };
    const auto pointValue = [](const QVariantMap &box, const QString &name,
                               int axis) {
      return box
          .value(QStringLiteral("perspective") + name.left(1).toUpper() +
                 name.mid(1))
          .toList()
          .at(axis)
          .toDouble();
    };
    const auto invokeLayoutCorner = [](QObject *geometry,
                                       const QVariantMap &box,
                                       const QString &name) {
      QVariant result;
      const bool invoked = QMetaObject::invokeMethod(
          geometry, "perspectiveLayoutCorner", Q_RETURN_ARG(QVariant, result),
          Q_ARG(QVariant, box), Q_ARG(QVariant, name), Q_ARG(QVariant, 100.0),
          Q_ARG(QVariant, 60.0));
      return invoked ? result.toMap() : QVariantMap{};
    };

    QList<QObject *> delegates;
    collectDelegates(collectDelegates, window->contentItem(), delegates);
    QTRY_COMPARE(delegates.size(), 2);
    QObject *selectedDelegate =
        modelIndex(delegates.at(0)) == 0 ? delegates.at(0) : delegates.at(1);
    QObject *otherDelegate =
        selectedDelegate == delegates.at(0) ? delegates.at(1) : delegates.at(0);
    QCOMPARE(modelIndex(selectedDelegate), 0);
    QCOMPARE(modelIndex(otherDelegate), 1);

    auto *geometry =
        window->findChild<QObject *>(QStringLiteral("perspectiveGeometry"));
    QVERIFY(geometry);
    auto *perspectiveState = window->findChild<QObject *>(
        QStringLiteral("perspectiveInteractionState"));
    QVERIFY(perspectiveState);

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginPerspectiveDrag",
        Q_ARG(QVariant, QVariant::fromValue(selectedDelegate)),
        Q_ARG(QVariant, QStringLiteral("ne")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag",
                                      Q_ARG(QVariant, 20.0),
                                      Q_ARG(QVariant, -10.0)));
    QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 20.0));
    QTRY_VERIFY(
        nearlyEqual(window->property("perspectiveY").toDouble(), -10.0));

    QVariantMap selectedPreview = invokeLayoutCorner(
        geometry, editor.boxes().at(0).toMap(), QStringLiteral("ne"));
    QVariantMap otherPreview = invokeLayoutCorner(
        geometry, editor.boxes().at(1).toMap(), QStringLiteral("ne"));
    QCOMPARE(selectedPreview.value(QStringLiteral("x")).toDouble(), 120.0);
    QCOMPARE(selectedPreview.value(QStringLiteral("y")).toDouble(), -10.0);
    QCOMPARE(otherPreview.value(QStringLiteral("x")).toDouble(), 100.0);
    QCOMPARE(otherPreview.value(QStringLiteral("y")).toDouble(), 0.0);
    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0),
             20.0);
    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 1),
             -10.0);
    QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 0),
             0.0);

    QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag",
                                      Q_ARG(QVariant, false)));
    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0),
             0.0);
    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 1),
             0.0);
    QCOMPARE(perspectiveState->property("activePerspectiveBoxIndex").toInt(),
             -1);

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginPerspectiveDrag",
        Q_ARG(QVariant, QVariant::fromValue(selectedDelegate)),
        Q_ARG(QVariant, QStringLiteral("ne")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag",
                                      Q_ARG(QVariant, 20.0),
                                      Q_ARG(QVariant, -10.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag",
                                      Q_ARG(QVariant, true)));

    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0),
             20.0);
    QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 1),
             -10.0);
    QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 0),
             0.0);
    QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 1),
             0.0);
  }

  void qmlPerspectiveMidpointDragConstrainsAxisAndCommitsAdjacentCorners() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 50, 100, 60);
    editor.setSelectedPerspectiveEnabled(true);
    editor.setPerspectiveHandle(QStringLiteral("nw"), 10.0, 1.0);
    editor.setPerspectiveHandle(QStringLiteral("ne"), 20.0, 2.0);
    editor.setPerspectiveHandle(QStringLiteral("se"), 30.0, 3.0);
    editor.setPerspectiveHandle(QStringLiteral("sw"), 40.0, 4.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    const auto nearlyEqual = [](double a, double b) {
      return std::abs(a - b) <= 0.001;
    };
    const auto pointValue = [](const QVariantMap &box, const QString &name,
                               int axis) {
      return box
          .value(QStringLiteral("perspective") + name.left(1).toUpper() +
                 name.mid(1))
          .toList()
          .at(axis)
          .toDouble();
    };
    const auto collectDelegates = [](auto &&self, QQuickItem *item,
                                     QList<QObject *> &results) -> void {
      if (!item)
        return;
      if (item->objectName() == QStringLiteral("textBoxDelegate"))
        results.append(item);
      for (QQuickItem *child : item->childItems())
        self(self, child, results);
    };

    QList<QObject *> delegates;
    collectDelegates(collectDelegates, window->contentItem(), delegates);
    QTRY_COMPARE(delegates.size(), 1);
    QObject *delegate = delegates.constFirst();

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginPerspectiveDrag",
        Q_ARG(QVariant, QVariant::fromValue(delegate)),
        Q_ARG(QVariant, QStringLiteral("n")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag",
                                      Q_ARG(QVariant, 20.0),
                                      Q_ARG(QVariant, -10.0)));
    QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 15.0));
    QTRY_VERIFY(nearlyEqual(window->property("perspectiveY").toDouble(), -8.5));
    QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag",
                                      Q_ARG(QVariant, true)));

    QVariantMap box = editor.boxes().at(0).toMap();
    QCOMPARE(pointValue(box, QStringLiteral("nw"), 0), 10.0);
    QCOMPARE(pointValue(box, QStringLiteral("nw"), 1), -9.0);
    QCOMPARE(pointValue(box, QStringLiteral("ne"), 0), 20.0);
    QCOMPARE(pointValue(box, QStringLiteral("ne"), 1), -8.0);
    QCOMPARE(pointValue(box, QStringLiteral("se"), 0), 30.0);
    QCOMPARE(pointValue(box, QStringLiteral("se"), 1), 3.0);
    QCOMPARE(pointValue(box, QStringLiteral("sw"), 0), 40.0);
    QCOMPARE(pointValue(box, QStringLiteral("sw"), 1), 4.0);

    delegates.clear();
    collectDelegates(collectDelegates, window->contentItem(), delegates);
    QTRY_COMPARE(delegates.size(), 1);
    delegate = delegates.constFirst();

    QVERIFY(QMetaObject::invokeMethod(
        window, "beginPerspectiveDrag",
        Q_ARG(QVariant, QVariant::fromValue(delegate)),
        Q_ARG(QVariant, QStringLiteral("e")), Q_ARG(QVariant, 0.0),
        Q_ARG(QVariant, 0.0)));
    QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag",
                                      Q_ARG(QVariant, 20.0),
                                      Q_ARG(QVariant, -10.0)));
    QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 45.0));
    QTRY_VERIFY(nearlyEqual(window->property("perspectiveY").toDouble(), -2.5));
    QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag",
                                      Q_ARG(QVariant, true)));

    box = editor.boxes().at(0).toMap();
    QCOMPARE(pointValue(box, QStringLiteral("ne"), 0), 40.0);
    QCOMPARE(pointValue(box, QStringLiteral("ne"), 1), -8.0);
    QCOMPARE(pointValue(box, QStringLiteral("se"), 0), 50.0);
    QCOMPARE(pointValue(box, QStringLiteral("se"), 1), 3.0);
    QCOMPARE(pointValue(box, QStringLiteral("nw"), 0), 10.0);
    QCOMPARE(pointValue(box, QStringLiteral("sw"), 0), 40.0);
  }

  void qmlPathHandleInteractionStatePreviewsAndResetsForPerspectivePlane() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 40, 160, 60);
    editor.setSelectedPathEnabled(true);
    editor.setPathHandle(0, 0.0, 0.0);
    editor.setPerspectiveHandle(QStringLiteral("nw"), 30.0, -10.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/app/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *state = window->findChild<QObject *>(
        QStringLiteral("pathHandleInteractionState"));
    QVERIFY(state);

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(window->contentItem(),
                                               QStringLiteral("pathHandle")));
    auto *handle = qobject_cast<QQuickItem *>(object);
    QVERIFY(handle);
    QTRY_VERIFY(handle->isVisible());

    const QVariantList before = editor.boxes()
                                    .at(0)
                                    .toMap()
                                    .value(QStringLiteral("pathPoints"))
                                    .toList();
    const QPoint start =
        handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2))
            .toPoint();
    const QPoint preview = start + QPoint(18, 0);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_VERIFY(state->property("pathHandleInteractionActive").toBool());
    QCOMPARE(state->property("activePathHandleIndex").toInt(), 0);
    QVERIFY(state->property("activePathHandlePerspective").toBool());

    QTest::mouseMove(window, preview);
    QCoreApplication::processEvents();
    const QVariantList during = editor.boxes()
                                    .at(0)
                                    .toMap()
                                    .value(QStringLiteral("pathPoints"))
                                    .toList();
    QVERIFY(during != before);
    QVERIFY(during.at(0).toList().at(0).toDouble() >
            before.at(0).toList().at(0).toDouble());

    QVERIFY(QMetaObject::invokeMethod(window, "endPathHandleDrag"));
    QTRY_VERIFY(!state->property("pathHandleInteractionActive").toBool());
    QCOMPARE(state->property("activePathHandleIndex").toInt(), -1);

    QTest::mouseMove(window, preview + QPoint(18, 0));
    QCoreApplication::processEvents();
    const QVariantList afterReleaseMove =
        editor.boxes()
            .at(0)
            .toMap()
            .value(QStringLiteral("pathPoints"))
            .toList();
    QCOMPARE(afterReleaseMove, during);
  }
};

QTEST_MAIN(QmlInteractionSmokeTests)

#include "qml_interaction_smoke_tests.moc"
