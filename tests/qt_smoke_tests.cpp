#include "app/EditorController.h"
#include "app/TextBoxSelectionService.h"
#include "core/EffectLimits.h"
#include "fonts/FontResolver.h"
#include "fonts/SfntNames.h"
#include "qt_test_helpers.h"

#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <cmath>

using namespace textfx;
using namespace textfx::test;

class QtSmokeTests final : public QObject {
  Q_OBJECT

private slots:
  void blurKernelRadiusIsCapped() {
    QCOMPARE(cappedBlurKernelRadius(0), 0);
    QCOMPARE(cappedBlurKernelRadius(1), 1);
    QCOMPARE(cappedBlurKernelRadius(MaxBlurKernelRadius), MaxBlurKernelRadius);
    QCOMPARE(cappedBlurKernelRadius(MaxBlurKernelRadius + 100),
             MaxBlurKernelRadius);
  }

  void selectionServiceDuplicatesDeletesAndMovesLayers() {
    std::vector<TextBox> boxes(3);
    boxes[0].text = "bottom";
    boxes[1].text = "middle";
    boxes[1].bounds = {10.0, 20.0, 100.0, 50.0};
    boxes[2].text = "top";

    QCOMPARE(TextBoxSelectionService::normalizedIndex(boxes, 1), 1);
    QCOMPARE(TextBoxSelectionService::normalizedIndex(boxes, 3), -1);

    int selectedIndex = 1;
    QVERIFY(TextBoxSelectionService::duplicateSelected(boxes, selectedIndex));
    QCOMPARE(boxes.size(), std::size_t{4});
    QCOMPARE(selectedIndex, 3);
    QVERIFY(boxes.at(3).text == "middle");
    QCOMPARE(boxes.at(3).bounds.x, 26.0);
    QCOMPARE(boxes.at(3).bounds.y, 36.0);

    QVERIFY(TextBoxSelectionService::moveLayer(boxes, selectedIndex, -12));
    QCOMPARE(selectedIndex, 0);
    QVERIFY(boxes.at(0).text == "middle");

    QVERIFY(TextBoxSelectionService::deleteSelected(boxes, selectedIndex));
    QCOMPARE(boxes.size(), std::size_t{3});
    QCOMPARE(selectedIndex, 0);
    QVERIFY(boxes.at(0).text == "bottom");

    selectedIndex = 99;
    QVERIFY(!TextBoxSelectionService::duplicateSelected(boxes, selectedIndex));
    QVERIFY(!TextBoxSelectionService::deleteSelected(boxes, selectedIndex));
    QVERIFY(!TextBoxSelectionService::moveLayer(boxes, selectedIndex, 0));
    QCOMPARE(selectedIndex, 99);
  }

  void gradientAndPathUseLiveRendererWithoutPreviewArtifact() {
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
    QVERIFY(source.contains(
        QStringLiteral("gradientEnabled: boxRef.boxModel.gradient")));
    QVERIFY(source.contains(QStringLiteral(
        "gradientDirection: boxRef.boxModel.gradientDirection")));
    QVERIFY(source.contains(QStringLiteral(
        "gradientColorA: rootWindow.qmlColor(boxRef.boxModel."
        "gradientColorA)")));
    QVERIFY(source.contains(QStringLiteral(
        "gradientColorB: rootWindow.qmlColor(boxRef.boxModel."
        "gradientColorB)")));
    QVERIFY(source.contains(QStringLiteral(
        "pathEnabled: boxRef.boxModel.path && "
        "!boxRef.editingSelected")));
    QVERIFY(source.contains(
        QStringLiteral("pathMode: boxRef.boxModel.pathMode")));
    QVERIFY(source.contains(
        QStringLiteral("pathPoints: boxRef.boxModel.pathPoints")));
    QVERIFY(source.contains(QStringLiteral("id: pathGuide")));
    QVERIFY(source.contains(
        QStringLiteral("visible: boxRef.selected && boxRef.boxModel.path && "
                       "boxRef.boxModel.pathPoints.length > 1")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral("transform: Matrix4x4 { matrix: "
                       "pathGuide.rootWindow.perspectiveMatrix(pathGuide."
                       "boxRef.boxModel, pathGuide.width, pathGuide.height, "
                       "pathGuide.rootWindow.viewDocScale(), "
                       "pathGuide.boxRef.perspectiveActive) }")));
    QVERIFY(
        source.contains(QStringLiteral("function smoothGuidePoints(points)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("pathMode === 1 ? smoothGuidePoints(points) : "
                               "points.map((point)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("return guidePoint(point)")));
    QVERIFY(!source.contains(
        QStringLiteral("const baselineOffset = lineWidth / 2")));
    QVERIFY(source.contains(
        QStringLiteral("ctx.moveTo(guidePoints[0].x, guidePoints[0].y)")));
    QVERIFY(source.contains(QStringLiteral(
        "for (let pass = 0; pass < 3 && result.length >= 3; ++pass)")));
    QVERIFY(source.contains(QStringLiteral("* 0.25")));
    QVERIFY(source.contains(QStringLiteral("* 0.75")));
    QVERIFY(
        source.contains(QStringLiteral("onPathModeChanged: requestPaint()")));
    QVERIFY(
        source.contains(QStringLiteral("onPathPointsChanged: requestPaint()")));
  }

  void qmlPathHandleDragUpdatesPathPoints() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 40, 160, 60);
    editor.updateSelectedText(QStringLiteral("Path"));
    editor.setSelectedPathEnabled(true);

    const QVariantList before = editor.boxes()
                                    .at(0)
                                    .toMap()
                                    .value(QStringLiteral("pathPoints"))
                                    .toList();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(window->contentItem(),
                                               QStringLiteral("pathHandle")));
    auto *handle = qobject_cast<QQuickItem *>(object);
    QVERIFY(handle);
    QTRY_VERIFY(handle->isVisible());
    auto *box = handle->parentItem();
    QVERIFY(box);
    const double boxWidth = box->width();
    const double boxHeight = box->height();

    auto *boxesModel = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(boxesModel);
    QSignalSpy changed(boxesModel, &QAbstractItemModel::dataChanged);
    const QPoint start =
        handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2))
            .toPoint();
    const QPoint end = start + QPoint(48, -24);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(window, start + QPoint(12, -6));
    QTest::mouseMove(window, start + QPoint(24, -12));
    QTest::mouseMove(window, start + QPoint(36, -18));
    QTest::mouseMove(window, end);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);

    QTRY_VERIFY(changed.count() >= 1);
    const QVariantList after = editor.boxes()
                                   .at(0)
                                   .toMap()
                                   .value(QStringLiteral("pathPoints"))
                                   .toList();
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
    const QVariantList afterReleaseMove =
        editor.boxes()
            .at(0)
            .toMap()
            .value(QStringLiteral("pathPoints"))
            .toList();
    QCOMPARE(afterReleaseMove, after);
  }

  void qmlPathHandleFollowsPerspectivePlane() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(40, 40, 160, 60);
    editor.setSelectedPathEnabled(true);
    editor.setPathHandle(0, 0.0, 0.0);
    editor.setPerspectiveHandle(QStringLiteral("nw"), 30.0, -10.0);
    editor.setPerspectiveHandle(QStringLiteral("ne"), -24.0, 18.0);
    editor.setPerspectiveHandle(QStringLiteral("se"), 34.0, 12.0);
    editor.setPerspectiveHandle(QStringLiteral("sw"), -12.0, -22.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(window->contentItem(),
                                               QStringLiteral("pathHandle")));
    auto *handle = qobject_cast<QQuickItem *>(object);
    QVERIFY(handle);
    QTRY_VERIFY(handle->isVisible());
    QObject *canvasObject = nullptr;
    QTRY_VERIFY(canvasObject = findVisualChildByName(
                    window->contentItem(), QStringLiteral("centralCanvas")));
    auto *canvas = qobject_cast<QQuickItem *>(canvasObject);
    QVERIFY(canvas);
    auto *pathPlane = handle->parentItem();
    QVERIFY(pathPlane);
    auto *box = pathPlane->parentItem();
    QVERIFY(box);

    const QPointF raw = box->mapToScene(QPointF(0.0, 0.0));
    const QPointF expected = box->mapToScene(QPointF(30.0, -10.0));
    const QPointF actual =
        handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2));

    QVERIFY(std::hypot(actual.x() - raw.x(), actual.y() - raw.y()) > 8.0);
    QVERIFY(std::hypot(actual.x() - expected.x(), actual.y() - expected.y()) <
            2.0);

    const QVariantList before = editor.boxes()
                                    .at(0)
                                    .toMap()
                                    .value(QStringLiteral("pathPoints"))
                                    .toList();
    const QPoint start = actual.toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_VERIFY(window->property("pathHandleInteractionActive").toBool());
    const QPointF canvasStart(
        window->property("pathHandlePressCanvasX").toDouble(),
        window->property("pathHandlePressCanvasY").toDouble());
    const QPointF canvasEnd = canvasStart + QPointF(42.0, 18.0);
    const QPointF planeStart(
        window->property("pathHandlePressLocalX").toDouble(),
        window->property("pathHandlePressLocalY").toDouble());
    const QPointF planeEnd = pathPlane->mapFromItem(canvas, canvasEnd);
    const double expectedDx =
        (planeEnd.x() - planeStart.x()) / pathPlane->width();
    const double expectedDy =
        (planeEnd.y() - planeStart.y()) / pathPlane->height();
    const double naiveDx =
        (canvasEnd.x() - canvasStart.x()) / pathPlane->width();
    const double naiveDy =
        (canvasEnd.y() - canvasStart.y()) / pathPlane->height();
    QVERIFY(std::hypot(expectedDx - naiveDx, expectedDy - naiveDy) > 0.08);
    QVERIFY(QMetaObject::invokeMethod(window, "updatePathHandleDragFromCanvas",
                                      Q_ARG(QVariant, canvasEnd.x()),
                                      Q_ARG(QVariant, canvasEnd.y())));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start);
    const QVariantList after = editor.boxes()
                                   .at(0)
                                   .toMap()
                                   .value(QStringLiteral("pathPoints"))
                                   .toList();
    QVERIFY(after != before);
    const auto beforePoint = before.at(0).toList();
    const auto afterPoint = after.at(0).toList();
    const double actualDx =
        afterPoint.at(0).toDouble() - beforePoint.at(0).toDouble();
    const double actualDy =
        afterPoint.at(1).toDouble() - beforePoint.at(1).toDouble();
    QVERIFY2(std::abs(actualDx - expectedDx) < 0.03,
             qPrintable(QStringLiteral("actualDx=%1 expectedDx=%2 actualDy=%3 "
                                       "expectedDy=%4 naiveDx=%5 naiveDy=%6")
                            .arg(actualDx)
                            .arg(expectedDx)
                            .arg(actualDy)
                            .arg(expectedDy)
                            .arg(naiveDx)
                            .arg(naiveDy)));
    QVERIFY2(std::abs(actualDy - expectedDy) < 0.03,
             qPrintable(QStringLiteral("actualDx=%1 expectedDx=%2 actualDy=%3 "
                                       "expectedDy=%4 naiveDx=%5 naiveDy=%6")
                            .arg(actualDx)
                            .arg(expectedDx)
                            .arg(actualDy)
                            .arg(expectedDy)
                            .arg(naiveDx)
                            .arg(naiveDy)));
    QVERIFY(std::hypot(actualDx - naiveDx, actualDy - naiveDy) > 0.05);
  }

  void blurUsesLiveRendererWithoutPreviewArtifact() {
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
    QVERIFY(source.contains(QStringLiteral(
        "blurSize: boxRef.boxModel.blur && "
        "boxRef.boxModel.blurSize > 0 ? "
        "boxRef.boxModel.blurSize : 0")));
  }

  void shadowUsesLiveRendererWithoutPreviewArtifact() {
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
    QVERIFY(source.contains(
        QStringLiteral("shadowEnabled: boxRef.boxModel.shadow")));
    QVERIFY(source.contains(QStringLiteral(
        "shadowBlurSize: boxRef.boxModel.shadow && "
        "boxRef.boxModel.shadowBlurSize > 0 ? "
        "boxRef.boxModel.shadowBlurSize : 0")));
  }

  void qmlEditorNamedConstantsKeepLegacyValues() {
    const QString source = readQmlFile(QStringLiteral("Main.qml"));

    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModeIdle: 0")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModePan: 2")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModeCreate: 3")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModeResize: 4")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModePerspective: 5")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModeRotate: 6")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModeMove: 7")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int dragModePathHandle: 8")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property real minimumBoxSize: 12")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int minimumFontSize: 1")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int maximumFontSize: 512")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int minimumTextSpacing: -100")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int maximumTextSpacing: 300")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int minimumEffectSize: 0")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int maximumEffectSize: 128")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int minimumShadowOffset: -512")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int maximumShadowOffset: 512")));
  }

  void qmlNormalResizeUsesPressSnapshotSeparateFromPerspective() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype rotateStart =
        source.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateStart > resizeStart);
    const QString resizeSource =
        source.mid(resizeStart, rotateStart - resizeStart);

    QVERIFY(!resizeSource.contains(QStringLiteral("property real startX")));
    QVERIFY(
        !resizeSource.contains(QStringLiteral("property real startCanvasX")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("const delta = rotatePoint((canvasX - "
                               "resizeStartCanvasX) / scale")));
    QVERIFY(source.contains(QStringLiteral(
        "right = Math.max(resizeStartW + delta.x, left + minimumBoxSize)")));
    QVERIFY(source.contains(QStringLiteral(
        "bottom = Math.max(resizeStartH + delta.y, top + minimumBoxSize)")));
    QVERIFY(source.contains(
        QStringLiteral("left = Math.min(delta.x, right - minimumBoxSize)")));
    QVERIFY(source.contains(
        QStringLiteral("top = Math.min(delta.y, bottom - minimumBoxSize)")));
    const qsizetype perspectiveBranch = resizeSource.indexOf(
        QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
    const qsizetype normalUpdate = resizeSource.indexOf(QStringLiteral(
        "resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)"));
    QVERIFY(perspectiveBranch >= 0);
    QVERIFY(normalUpdate > perspectiveBranch);
    QVERIFY(
        resizeSource.mid(perspectiveBranch, normalUpdate - perspectiveBranch)
            .contains(QStringLiteral("return")));
    QVERIFY(source.contains(
        QStringLiteral("window.editor.setSelectedBounds(bounds.x, bounds.y, "
                       "bounds.width, bounds.height)")));
    QVERIFY(!resizeSource.contains(QStringLiteral("startLocalX")));
    QVERIFY(!resizeSource.contains(QStringLiteral("startLocalY")));
  }

  void qmlResizeHandleKeepsStableDragStateOutsideHandle() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype rotateStart =
        source.indexOf(QStringLiteral("id: rotateHandle"), resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateStart > resizeStart);
    const QString resizeSource =
        source.mid(resizeStart, rotateStart - resizeStart);

    QVERIFY(source.contains(
        QStringLiteral("property var activeResizeDelegate: null")));
    QVERIFY(source.contains(
        QStringLiteral("property string activeResizeHandle: \"\"")));
    QVERIFY(source.contains(QStringLiteral(
        "function beginResizeDrag(delegate, handle, canvasX, canvasY)")));
    QVERIFY(source.contains(
        QStringLiteral("function updateResizeDrag(canvasX, canvasY)")));
    QVERIFY(source.contains(QStringLiteral("function endResizeDrag(commit)")));
    QVERIFY(resizeSource.contains(QStringLiteral("preventStealing: true")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("acceptedButtons: Qt.LeftButton")));
    QVERIFY(resizeSource.contains(QStringLiteral("mouse.accepted = true")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle."
                       "boxRef, modelData.name, point.x, point.y)")));
    QVERIFY(resizeSource.contains(QStringLiteral(
        "resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("resizeHandle.rootWindow.endResizeDrag(true)")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("resizeHandle.rootWindow.endPerspectiveDrag(true)")));
    QVERIFY(!resizeSource.contains(QStringLiteral("drag.target")));
  }

  void qmlBoxMoveUsesTransientWindowDragAndStableEditor() {
    const QString source = readQmlFile(QStringLiteral("Main.qml"));
    const QString moveStateSource =
        readQmlFile(QStringLiteral("BoxMoveInteractionState.qml"));
    const QString delegateSource =
        readQmlFile(QStringLiteral("TextBoxDelegate.qml"));
    QVERIFY(!source.isEmpty());
    QVERIFY(!moveStateSource.isEmpty());
    QVERIFY(!delegateSource.isEmpty());

    const qsizetype moveStart = delegateSource.indexOf(QStringLiteral(
        "z: boxRef.selected && editorRef.editingText ? -1 : 10"));
    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        delegateSource, QStringLiteral("model: [\n            {name: \"nw\"}"),
        moveStart);
    QVERIFY(moveStart >= 0);
    QVERIFY(resizeStart > moveStart);
    const QString moveSource =
        delegateSource.mid(moveStart, resizeStart - moveStart);

    const qsizetype updateMoveStart = source.indexOf(
        QStringLiteral("function updateMoveDrag(canvasX, canvasY)"));
    const qsizetype endMoveStart =
        source.indexOf(QStringLiteral("function endMoveDrag(commit)"));
    const qsizetype escapeStart =
        source.indexOf(QStringLiteral("function handleEscape()"));
    QVERIFY(updateMoveStart >= 0);
    QVERIFY(endMoveStart > updateMoveStart);
    QVERIFY(escapeStart > endMoveStart);
    const QString updateMove =
        source.mid(updateMoveStart, endMoveStart - updateMoveStart);
    const QString endMove =
        source.mid(endMoveStart, escapeStart - endMoveStart);

    QVERIFY(source.contains(
        QStringLiteral("readonly property var editor: Editor")));
    QVERIFY(moveStateSource.contains(
        QStringLiteral("property var activeMoveDelegate: null")));
    QVERIFY(source.contains(
        QStringLiteral("property alias activeMoveDelegate: "
                       "boxMoveInteraction.activeMoveDelegate")));
    QVERIFY(source.contains(QStringLiteral(
        "property alias activeMoveIndex: boxMoveInteraction.activeMoveIndex")));
    QVERIFY(source.contains(
        QStringLiteral("property alias moveX: boxMoveInteraction.moveX")));
    QVERIFY(source.contains(
        QStringLiteral("property alias moveY: boxMoveInteraction.moveY")));
    QVERIFY(source.contains(QStringLiteral("BoxMoveInteractionState")));
    QVERIFY(source.contains(
        QStringLiteral("objectName: \"boxMoveInteractionState\"")));
    QVERIFY(source.contains(QStringLiteral(
        "function beginMoveDrag(delegate, index, canvasX, canvasY)")));
    QVERIFY(source.contains(QStringLiteral(
        "boxMoveInteraction.begin(delegate, index, canvasX, canvasY)")));
    QVERIFY(updateMove.contains(
        QStringLiteral("boxMoveInteraction.update(canvasX, canvasY)")));
    QVERIFY(moveSource.contains(QStringLiteral("preventStealing: true")));
    QVERIFY(moveSource.contains(QStringLiteral("mouse.accepted = true")));
    QVERIFY(source.contains(QStringLiteral(
        "function perspectiveVisualBounds(box, width, height)")));
    QVERIFY(source.contains(QStringLiteral(
        "function pointInPerspectivePolygon(point, box, width, height)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        delegateSource,
        QStringLiteral("rootWindow.perspectiveVisualBounds("
                       "boxRef.boxModel, boxRef.width, boxRef.height)")));
    QVERIFY(moveSource.contains(QStringLiteral("x: visualBounds.x")));
    QVERIFY(moveSource.contains(QStringLiteral("width: visualBounds.width")));
    QVERIFY(moveSource.contains(
        QStringLiteral("if (boxRef.perspectiveActive && "
                       "!rootWindow.pointInPerspectivePolygon(local, "
                       "boxRef.boxModel, boxRef.width, boxRef.height))")));
    QVERIFY(delegateSource.contains(QStringLiteral(
        "readonly property var rootWindow: ApplicationWindow.window")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("readonly property var editorRef: rootWindow ? "
                       "rootWindow.editor : null")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("property real visualDocX: boxModel.x")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("property real visualDocY: boxModel.y")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("property real visualDocW: boxModel.w")));
    QVERIFY(!delegateSource.contains(QStringLiteral("property bool moveActive")));
    QVERIFY(!delegateSource.contains(QStringLiteral("property bool resizeActive")));
    QVERIFY(delegateSource.contains(
        QStringLiteral("x: rootWindow.documentToViewX(visualDocX)")));
    QVERIFY(moveSource.contains(
        QStringLiteral("editorRef.selectBox(boxRef.boxModel.index)")));
    QVERIFY(
        moveSource.contains(QStringLiteral("editorRef.beginInteraction()")));
    QVERIFY(moveSource.contains(
        QStringLiteral("rootWindow.beginMoveDrag(boxRef, "
                       "boxRef.boxModel.index, point.x, point.y)")));
    QVERIFY(moveSource.contains(
        QStringLiteral("rootWindow.updateMoveDrag(point.x, point.y)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        moveSource,
        QStringLiteral(
            "rootWindow.endMoveDrag(true); editorRef.endInteraction()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        moveSource,
        QStringLiteral(
            "rootWindow.endMoveDrag(false); editorRef.endInteraction()")));
    QVERIFY(!moveSource.contains(QStringLiteral("Editor.moveSelected")));
    QVERIFY(!moveSource.contains(QStringLiteral("window.editor")));
    QVERIFY(updateMove.contains(QStringLiteral("setSelectedBounds")));
    QVERIFY(!updateMove.contains(QStringLiteral("activeMoveDelegate.x")));
    QVERIFY(!endMove.contains(
        QStringLiteral("const moveDelta = boxMoveInteraction.delta()")));
    QVERIFY(!endMove.contains(QStringLiteral("moveSelected")));
    QVERIFY(endMove.contains(QStringLiteral(
        "window.editor.setSelectedBounds(boxMoveInteraction.moveStartX")));
    QVERIFY(endMove.contains(QStringLiteral("boxMoveInteraction.reset()")));
  }

  void qmlPerspectiveMoveHitAreaFollowsVisualPolygonBelowHandles() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype geometryStart =
        source.indexOf(QStringLiteral("QtObject {\n    id: geometry"));
    const qsizetype boundsStart = source.indexOf(
        QStringLiteral("function perspectiveVisualBounds(box, width, height)"),
        geometryStart);
    const qsizetype marginStart = source.indexOf(
        QStringLiteral("function perspectiveMargin(box)"), boundsStart);
    QVERIFY(geometryStart >= 0);
    QVERIFY(boundsStart >= 0);
    QVERIFY(marginStart > boundsStart);
    const QString hitHelpers =
        source.mid(boundsStart, marginStart - boundsStart);

    QVERIFY(hitHelpers.contains(
        QStringLiteral("perspectiveCorner(box, \"nw\", width, height)")));
    QVERIFY(hitHelpers.contains(
        QStringLiteral("Math.min(nw.x, ne.x, se.x, sw.x)")));
    QVERIFY(hitHelpers.contains(
        QStringLiteral("Math.max(nw.y, ne.y, se.y, sw.y)")));
    QVERIFY(hitHelpers.contains(
        QStringLiteral("function pointOnSegment(point, a, b)")));
    QVERIFY(hitHelpers.contains(QStringLiteral("pointOnSegment(point, a, b)")));

    const qsizetype moveStart = source.indexOf(QStringLiteral(
        "readonly property var visualBounds: boxRef.perspectiveActive"));
    const qsizetype handlesStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"),
        moveStart);
    QVERIFY(moveStart >= 0);
    QVERIFY(handlesStart > moveStart);
    const QString moveSource = source.mid(moveStart, handlesStart - moveStart);

    QVERIFY(moveSource.contains(QStringLiteral(
        "boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        moveSource,
        QStringLiteral(
            ": ({ x: 0, y: 0, width: boxRef.width, height: boxRef.height })")));
    QVERIFY(moveSource.contains(QStringLiteral("mouse.accepted = false")));
    QVERIFY(moveSource.contains(QStringLiteral("return")));
    QVERIFY(moveSource.contains(
        QStringLiteral("rootWindow.beginMoveDrag(boxRef, "
                       "boxRef.boxModel.index, point.x, point.y)")));
    QVERIFY(!moveSource.contains(QStringLiteral("beginResizeDrag")));
    QVERIFY(!moveSource.contains(QStringLiteral("beginPerspectiveDrag")));

    const qsizetype handleZ =
        source.indexOf(QStringLiteral("z: 20"), handlesStart);
    QVERIFY(handleZ > handlesStart);
  }

  void qmlPerspectiveMidpointsAreDerivedAndAxisConstrained() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype geometryStart =
        source.indexOf(QStringLiteral("QtObject {\n    id: geometry"));
    const qsizetype visualStart = source.indexOf(
        QStringLiteral(
            "function visualHandlePosition(box, name, width, height)"),
        geometryStart);
    const qsizetype topStart = source.indexOf(
        QStringLiteral("function topMiddleVisualPoint"), visualStart);
    QVERIFY(geometryStart >= 0);
    QVERIFY(visualStart >= 0);
    QVERIFY(topStart > visualStart);
    const QString visualSource =
        source.mid(visualStart, topStart - visualStart);

    QVERIFY(visualSource.contains(
        QStringLiteral("name === \"n\" || name === \"e\" || name === \"s\" || "
                       "name === \"w\"")));
    QVERIFY(visualSource.contains(
        QStringLiteral("perspectiveCorner(box, \"nw\", width, height)")));
    QVERIFY(visualSource.contains(
        QStringLiteral("perspectiveCorner(box, \"ne\", width, height)")));
    QVERIFY(visualSource.contains(
        QStringLiteral("perspectiveCorner(box, \"se\", width, height)")));
    QVERIFY(visualSource.contains(
        QStringLiteral("perspectiveCorner(box, \"sw\", width, height)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        visualSource,
        QStringLiteral("return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }")));
  }

  void qmlPerspectiveMidpointDragMovesAdjacentCornersFromSnapshots() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(source.contains(
        QStringLiteral("objectName: \"perspectiveInteractionState\"")));
    QVERIFY(!source.contains(
        QStringLiteral("function commitPerspectiveCorner(name, x, y)\n")));
  }

  void qmlPerspectiveAndRotateHandlesAcceptStableMouseCapture() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype pathStart = source.indexOf(
        QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"),
        resizeStart);
    const qsizetype rotateStart = source.indexOf(
        QStringLiteral(
            "rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef"),
        resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateStart > resizeStart);
    QVERIFY(pathStart > rotateStart);

    const QString resizeSource =
        source.mid(resizeStart, rotateStart - resizeStart);
    const QString rotateSource =
        source.mid(rotateStart - 800, pathStart - rotateStart + 800);

    QVERIFY(resizeSource.contains(QStringLiteral("preventStealing: true")));
    QVERIFY(resizeSource.contains(QStringLiteral("mouse.accepted = true")));
    QVERIFY(rotateSource.contains(QStringLiteral("preventStealing: true")));
    QVERIFY(rotateSource.contains(QStringLiteral("mouse.accepted = true")));
    QVERIFY(rotateSource.contains(
        QStringLiteral("rotateHandle.rootWindow.beginRotateDrag(rotateHandle."
                       "boxRef, point.x, point.y)")));
    QVERIFY(resizeSource.contains(QStringLiteral(
        "resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, "
        "modelData.name, point.x, point.y)")));
    QVERIFY(resizeSource.contains(
        QStringLiteral("resizeHandle.rootWindow.endPerspectiveDrag(true)")));
    QVERIFY(!source.contains(
        QStringLiteral("model: [{name: \"nw\"}, {name: \"n\"}")));
  }

  void fontResolverUsesQtFamiliesAndDeterministicFallback() {
    QFont missing(QStringLiteral("TextFX Definitely Missing Font 9B33F761"));
    missing.setPixelSize(17);
    missing.setBold(true);

    const auto fallback = resolveFont(missing);
    QVERIFY(fallback.fellBack);
    QVERIFY(!fallback.font.family().isEmpty());
    QCOMPARE(fallback.font.pixelSize(), 17);
    QCOMPARE(fallback.font.bold(), true);
    QCOMPARE(resolveFont(missing).font.family(), fallback.font.family());
    QCOMPARE(missing.family(),
             QStringLiteral("TextFX Definitely Missing Font 9B33F761"));

    const QStringList families = QFontDatabase::families();
    QVERIFY(!families.isEmpty());
    QFont known(families.first());
    const auto resolved = resolveFont(known);
    QVERIFY(!resolved.fellBack);
    QVERIFY(resolved.requestedAvailable);
    QVERIFY(!QFontInfo(resolved.font).family().isEmpty());

    const QString beatDown = QStringLiteral("000 BeatDownBB [TeddyBear]");
    const bool beatDownAvailable =
        families.contains(beatDown) ||
        families.contains(QStringLiteral("000 BeatDownBB"));
    if (beatDownAvailable) {
      const auto beatDownResolved = resolveFont(QFont(beatDown));
      QVERIFY(!beatDownResolved.fellBack);
      QVERIFY(beatDownResolved.requestedAvailable);
    }
  }

  void sfntFullAndTypographicNamesMapToUsableFamily() {
    const QStringList names =
        detail::fontNamesFromSfntData(syntheticSfntNameTable());
    QVERIFY(names.contains(QStringLiteral("TextFX Alias Test")));
    QVERIFY(names.contains(QStringLiteral("000 TextFX Alias Test [Display]")));
    QVERIFY(names.contains(QStringLiteral("TextFX Usable Family")));
    QVERIFY(!names.contains(QStringLiteral("Ignored Style Name")));

    const QHash<QString, QString> aliases = detail::aliasesForSfntNames(
        names, QStringLiteral("TextFX Usable Family"));
    QCOMPARE(
        aliases.value(
            QStringLiteral("000 TextFX Alias Test [Display]").toCaseFolded()),
        QStringLiteral("TextFX Usable Family"));
    QCOMPARE(aliases.value(QStringLiteral("textfx alias test")),
             QStringLiteral("TextFX Usable Family"));
    QCOMPARE(
        aliases.value(QStringLiteral("TextFX Usable Family").toCaseFolded()),
        QStringLiteral("TextFX Usable Family"));
  }

  void qmlTextStyleControlsUseSvgIconButtons() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();
    const QString buttonSource =
        readQmlFile(QStringLiteral("TextStyleButton.qml"));
    const qsizetype buttonStart =
        buttonSource.indexOf(QStringLiteral("Button {"));
    const qsizetype buttonEnd = buttonSource.size();
    QVERIFY(buttonStart >= 0);
    QVERIFY(buttonEnd > buttonStart);

    QVERIFY(
        !readQmlFile(QStringLiteral("Main.qml"))
             .contains(QStringLiteral("component TextStyleButton: Button")));
    QVERIFY(buttonSource.contains(QStringLiteral("Button {")));
    QVERIFY(buttonSource.contains(QStringLiteral("checkable: true")));
    QVERIFY(buttonSource.contains(
        QStringLiteral("property int minimumButtonSize: 32")));
    QVERIFY(buttonSource.contains(
        QStringLiteral("Layout.minimumWidth: minimumButtonSize")));
    QVERIFY(buttonSource.contains(
        QStringLiteral("Layout.minimumHeight: minimumButtonSize")));
    QVERIFY(buttonSource.contains(QStringLiteral("property url iconSource")));
    QVERIFY(buttonSource.contains(QStringLiteral("display: AbstractButton.IconOnly")));
    QVERIFY(buttonSource.contains(QStringLiteral("icon.source: iconSource")));
    QVERIFY(buttonSource.contains(QStringLiteral("icon.width: 20")));
    QVERIFY(buttonSource.contains(QStringLiteral("icon.height: 20")));
    QVERIFY(
        !buttonSource.contains(QStringLiteral("Layout.preferredWidth: 32")));
    QVERIFY(
        !buttonSource.contains(QStringLiteral("Layout.preferredWidth: 40")));
    QVERIFY(
        !source.contains(QStringLiteral("font.family: \"Symbols Nerd Font\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/bold.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/italic.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/uppercase.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/lowercase.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-left.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-center.svg\"")));
    QVERIFY(source.contains(QStringLiteral("iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-right.svg\"")));
    for (const QString iconPath : {
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/bold.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/italic.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/uppercase.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/lowercase.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/align-left.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/align-center.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/align-right.svg"),
         }) {
      QVERIFY2(QFile::exists(iconPath), qPrintable(iconPath));
    }
    QVERIFY(!source.contains(QStringLiteral("text: qsTr(\"B\")")));
    QVERIFY(!source.contains(QStringLiteral("text: qsTr(\"I\")")));
    QVERIFY(!source.contains(QStringLiteral("text: qsTr(\"Aa\")")));
    QVERIFY(
        source.contains(QStringLiteral("Accessible.name: accessibleLabel")));
    QVERIFY(source.contains(QStringLiteral("ToolTip.text: accessibleLabel")));
    const qsizetype fontFamilyOptionsStart = source.indexOf(
        QStringLiteral("function fontFamilyOptions(selected)"));
    const qsizetype fontFamilyAddStart =
        source.indexOf(QStringLiteral("function add(value)"),
                       fontFamilyOptionsStart);
    const qsizetype fontFamilyOptionsDeclaration =
        source.indexOf(QStringLiteral("const options = []"),
                       fontFamilyOptionsStart);
    QVERIFY(fontFamilyOptionsStart >= 0);
    QVERIFY(fontFamilyOptionsDeclaration > fontFamilyOptionsStart);
    QVERIFY(fontFamilyAddStart > fontFamilyOptionsDeclaration);
    const qsizetype propertiesStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("Label { text: qsTr(\"Text Properties\"); "
                               "font.bold: true; enabled: "
                               "textPropertiesSection.sectionReady }"));
    const qsizetype presetsStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("Label { text: qsTr(\"Text Presets\"); "
                               "font.bold: true; enabled: "
                               "textPresetsSection.sectionReady }"));
    QVERIFY(propertiesStart >= 0);
    QVERIFY(presetsStart > propertiesStart);
    const QString propertiesSource =
        source.mid(propertiesStart, presetsStart - propertiesStart);

    QVERIFY(propertiesSource.contains(QStringLiteral("GroupBox")));
    QVERIFY(propertiesSource.contains(QStringLiteral("ColumnLayout {")));
    QVERIFY(!propertiesSource.contains(QStringLiteral("GridLayout")));
    QVERIFY(!propertiesSource.contains(QStringLiteral("columns: 2")));
    const qsizetype fontLabel = indexOfIgnoringWhitespace(
        propertiesSource,
        QStringLiteral("Label { text: qsTr(\"Font Family\") }"));
    const qsizetype fontCombo =
        propertiesSource.indexOf(QStringLiteral("ComboBox {"), fontLabel);
    const qsizetype fontSizeLabel = indexOfIgnoringWhitespace(
        propertiesSource, QStringLiteral("Label { text: qsTr(\"Size\") }"));
    const qsizetype fontSizeSpin =
        propertiesSource.indexOf(QStringLiteral("SpinBox {"), fontSizeLabel);
    const qsizetype leadingLabel = indexOfIgnoringWhitespace(
        propertiesSource, QStringLiteral("Label { text: qsTr(\"Leading\") }"));
    const qsizetype leadingSpin =
        propertiesSource.indexOf(QStringLiteral("SpinBox {"), leadingLabel);
    const qsizetype letterSpacingLabel = indexOfIgnoringWhitespace(
        propertiesSource, QStringLiteral("Label { text: qsTr(\"Tracking\") }"));
    const qsizetype letterSpacingSpin = propertiesSource.indexOf(
        QStringLiteral("SpinBox {"), letterSpacingLabel);
    const qsizetype colorLabel = indexOfIgnoringWhitespace(
        propertiesSource,
        QStringLiteral("Label { text: qsTr(\"Text Color\") }"));
    const qsizetype colorButton =
        propertiesSource.indexOf(QStringLiteral("ColorButton {"), colorLabel);
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
    QVERIFY(propertiesSource.contains(
        QStringLiteral("model: "
                       "textPropertiesSection.fontFamilyOptionsProvider("
                       "textPropertiesSection.selectedBoxData.fontFamily)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral("ComboBox { id: fontFamilyCombo "
                       "Layout.fillWidth: true "
                       "Layout.minimumWidth: 0 model: "
                       "textPropertiesSection.fontFamilyOptionsProvider")));
    const qsizetype fontComboEnd = propertiesSource.indexOf(
        QStringLiteral(
            "onActivated: "
            "textPropertiesSection.editor.setSelectedFontFamily(currentText)"),
        fontCombo);
    QVERIFY(fontComboEnd > fontCombo);
    QVERIFY(propertiesSource.contains(QStringLiteral("contentItem: Label {")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("text: fontFamilyCombo.displayText")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("font: fontFamilyCombo.font")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("enabled: fontFamilyCombo.enabled")));
    QVERIFY(
        propertiesSource.contains(QStringLiteral("elide: Text.ElideRight")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("verticalAlignment: Text.AlignVCenter")));
    QVERIFY(propertiesSource.contains(QStringLiteral(
        "onActivated: "
        "textPropertiesSection.editor.setSelectedFontFamily(currentText)")));
    QVERIFY(!propertiesSource.contains(QStringLiteral(
        "onEditingFinished: leftInspectorPanel.editor.setSelectedFontFamily")));
    QVERIFY(!propertiesSource.contains(
        QStringLiteral("placeholderText: qsTr(\"Font\")")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource, QStringLiteral("Label { text: qsTr(\"Style\") }")));
    QVERIFY(propertiesSource.contains(QStringLiteral("Flow {")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("objectName: \"leftInspectorBoldButton\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/bold.svg\"; "
            "accessibleLabel: qsTr(\"Bold\"); checked: "
            "textPropertiesSection.selectedBoxData.bold; onClicked: "
            "textPropertiesSection.editor.setSelectedBold(checked)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/italic.svg\"; "
            "accessibleLabel: qsTr(\"Italic\"); checked: "
            "textPropertiesSection.selectedBoxData.italic; onClicked: "
            "textPropertiesSection.editor.setSelectedItalic(checked)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/uppercase.svg\"; "
            "accessibleLabel: qsTr(\"Uppercase\"); checked: "
            "textPropertiesSection.selectedBoxData.uppercase; onClicked: "
            "textPropertiesSection.editor.setSelectedUppercase(checked)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/lowercase.svg\"; "
            "accessibleLabel: qsTr(\"Lowercase\"); checked: "
            "textPropertiesSection.selectedBoxData.lowercase; onClicked: "
            "textPropertiesSection.editor.setSelectedLowercase(checked)")));
    QVERIFY(!propertiesSource.contains(QStringLiteral("String.fromCodePoint(")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral("Label { text: qsTr(\"Paragraph\") }")));
    QVERIFY(!propertiesSource.contains(QStringLiteral("Layout.columnSpan: 2")));
    const qsizetype styleLabel = indexOfIgnoringWhitespace(
        propertiesSource, QStringLiteral("Label { text: qsTr(\"Style\") }"));
    const qsizetype styleFlow =
        propertiesSource.indexOf(QStringLiteral("Flow {"), styleLabel);
    const qsizetype alignmentLabel = indexOfIgnoringWhitespace(
        propertiesSource,
        QStringLiteral("Label { text: qsTr(\"Paragraph\") }"));
    const qsizetype alignmentFlow =
        propertiesSource.indexOf(QStringLiteral("Flow {"), alignmentLabel);
    QVERIFY(styleLabel >= 0);
    QVERIFY(styleFlow > styleLabel);
    QVERIFY(alignmentLabel > styleFlow);
    QVERIFY(alignmentFlow > alignmentLabel);
    QVERIFY(!propertiesSource.contains(QStringLiteral(
        "model: [qsTr(\"Left\"), qsTr(\"Center\"), qsTr(\"Right\")]")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-left.svg\"; accessibleLabel: qsTr(\"Align "
            "Left\"); checked: "
            "textPropertiesSection.selectedBoxData.alignment === 0; "
            "onClicked: "
            "textPropertiesSection.editor.setSelectedAlignment(0)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-center.svg\"; accessibleLabel: qsTr(\"Align "
            "Center\"); checked: "
            "textPropertiesSection.selectedBoxData.alignment === 1; "
            "onClicked: "
            "textPropertiesSection.editor.setSelectedAlignment(1)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        propertiesSource,
        QStringLiteral(
            "iconSource: \"qrc:/qt/qml/TextFX/assets/icons/flat/align-right.svg\"; accessibleLabel: qsTr(\"Align "
            "Right\"); checked: "
            "textPropertiesSection.selectedBoxData.alignment === 2; "
            "onClicked: "
            "textPropertiesSection.editor.setSelectedAlignment(2)")));
    QVERIFY(!source.contains(
        QStringLiteral("CheckBox { checked: window.selectedBox() ? "
                       "window.selectedBox().bold")));
    QVERIFY(!source.contains(
        QStringLiteral("CheckBox { checked: window.selectedBox() ? "
                       "window.selectedBox().italic")));
    QVERIFY(!source.contains(
        QStringLiteral("CheckBox { checked: window.selectedBox() ? "
                       "window.selectedBox().uppercase")));
    QVERIFY(!buttonSource.contains(QStringLiteral("background:")));
    QVERIFY(!buttonSource.contains(QStringLiteral("contentItem:")));
    QVERIFY(!buttonSource.contains(QStringLiteral("indicator:")));
    QVERIFY(!buttonSource.contains(QStringLiteral("border.")));
    QVERIFY(buttonSource.contains(QStringLiteral("checked ? palette.accent : palette.buttonText")));
    QVERIFY(buttonSource.contains(QStringLiteral("palette.buttonText")));
  }

  void qmlTextPropertiesSectionIsExtractedAndWired() {
    const QString leftPanelSource =
        readQmlFile(QStringLiteral("LeftInspectorPanel.qml"));
    const QString textPropertiesSectionSource =
        readQmlFile(QStringLiteral("TextPropertiesSection.qml"));
    QFile cmake(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../CMakeLists.txt"));
    QVERIFY(cmake.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmake.readAll());

    QVERIFY(!textPropertiesSectionSource.isEmpty());
    QVERIFY(cmakeSource.contains(QStringLiteral("qml/TextPropertiesSection.qml")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("id: textPropertiesSection")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("property var editor: null")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("property var editorLimits")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("property var selectedBoxData: ({})")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("property var fontFamilyOptionsProvider")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("property var qmlColorProvider")));
    QVERIFY(textPropertiesSectionSource.contains(
        QStringLiteral("signal colorDialogRequested(string hex, string setter)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        leftPanelSource,
        QStringLiteral("TextPropertiesSection { editor: leftInspectorPanel.editor; "
                       "editorLimits: leftInspectorPanel.editorLimits; "
                       "selectedBoxData: leftInspectorPanel.selectedBoxData; "
                       "fontFamilyOptionsProvider: "
                       "leftInspectorPanel.fontFamilyOptionsProvider; "
                       "qmlColorProvider: leftInspectorPanel.qmlColorProvider; "
                       "onColorDialogRequested: (hex, setter) => { "
                       "leftInspectorPanel.colorDialogRequested(hex, setter) "
                       "}; Layout.fillWidth: true; Layout.minimumWidth: 0 }")));
    QVERIFY(!leftPanelSource.contains(QStringLiteral("id: textPropertiesSection")));
    QVERIFY(textPropertiesSectionSource.contains(QStringLiteral(
        "model: textPropertiesSection.fontFamilyOptionsProvider("
        "textPropertiesSection.selectedBoxData.fontFamily)")));
    QVERIFY(textPropertiesSectionSource.contains(QStringLiteral(
        "swatchText: textPropertiesSection.qmlColorProvider("
        "textPropertiesSection.selectedBoxData.color)")));
    QVERIFY(textPropertiesSectionSource.contains(QStringLiteral(
        "onClicked: textPropertiesSection.colorDialogRequested(swatchText, "
        "\"text\")")));
  }

  void qmlTextPresetsUseComboBoxSelector() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();
    const QString leftPanelSource =
        readQmlFile(QStringLiteral("LeftInspectorPanel.qml"));
    const QString textPropertiesSectionSource =
        readQmlFile(QStringLiteral("TextPropertiesSection.qml"));
    const QString textPresetsSectionSource =
        readQmlFile(QStringLiteral("TextPresetsSection.qml"));
    const qsizetype propertiesStart = indexOfIgnoringWhitespace(
        textPropertiesSectionSource,
        QStringLiteral("Label { text: qsTr(\"Text Properties\"); "
                       "font.bold: true; enabled: "
                       "textPropertiesSection.sectionReady }"));
    const qsizetype presetsStart =
        leftPanelSource.indexOf(QStringLiteral("TextPresetsSection {"));
    const qsizetype pageTextsStart =
        leftPanelSource.indexOf(QStringLiteral("PageTextsSection {"), presetsStart);
    QVERIFY(propertiesStart >= 0);
    QVERIFY(presetsStart >= 0);
    QVERIFY(pageTextsStart > presetsStart);
    const QString propertiesSource =
        textPropertiesSectionSource.mid(propertiesStart);
    const QString presetsSource = textPresetsSectionSource;

    QVERIFY(propertiesSource.contains(QStringLiteral("GroupBox {")));
    QVERIFY(source.contains(QStringLiteral("id: textPropertiesSection")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property bool sectionReady: "
                       "textPropertiesSection.editor && "
                       "textPropertiesSection.editor.selectedIndex >= 0")));
    QVERIFY(propertiesSource.contains(
        QStringLiteral("enabled: textPropertiesSection.sectionReady")));
    QVERIFY(!textPresetsSectionSource.isEmpty());
    QVERIFY(sourceContainsIgnoringWhitespace(
        leftPanelSource, QStringLiteral("TextPresetsSection { editor: "
                                        "leftInspectorPanel.editor; "
                                        "onTextInputFocusChanged: (active) => "
                                        "{ leftInspectorPanel."
                                        "textInputFocusChanged(active) }; "
                                        "Layout.fillWidth: true; "
                                        "Layout.minimumWidth: 0 }")));
    QVERIFY(!leftPanelSource.contains(QStringLiteral("id: presetSelect")));
    QVERIFY(!leftPanelSource.contains(QStringLiteral("id: presetNameField")));
    QVERIFY(presetsSource.contains(QStringLiteral("GroupBox {")));
    QVERIFY(source.contains(QStringLiteral("id: textPresetsSection")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property bool sectionReady: "
                       "textPresetsSection.editor && "
                       "textPresetsSection.editor.hasProject && "
                       "textPresetsSection.editor.selectedIndex >= 0")));
    QVERIFY(source.contains(QStringLiteral("property var editor: null")));
    QVERIFY(source.contains(
        QStringLiteral("signal textInputFocusChanged(bool active)")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int presetCount: "
                       "editor && editor.presets ? editor.presets.length : 0")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property int selectedPresetIndex: "
                       "editor ? editor.selectedPresetIndex : -1")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property bool hasSelectedPreset: "
                       "selectedPresetIndex >= 0 && "
                       "selectedPresetIndex < presetCount")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property var selectedPreset: "
                       "hasSelectedPreset ? editor.presets[selectedPresetIndex] "
                       ": null")));
    QVERIFY(source.contains(
        QStringLiteral("readonly property bool selectedPresetIsDefault: "
                       "selectedPreset ? selectedPreset.isDefault : false")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("enabled: textPresetsSection.sectionReady")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource, QStringLiteral("TextField { id: presetNameField")));
    QVERIFY(presetsSource.contains(QStringLiteral("ComboBox {")));
    QVERIFY(presetsSource.contains(QStringLiteral("id: presetSelect")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource, QStringLiteral("id: presetSelect Layout.fillWidth: "
                                      "true Layout.minimumWidth: 0")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("model: textPresetsSection.editor ? "
                       "textPresetsSection.editor.presets : []")));
    QVERIFY(presetsSource.contains(QStringLiteral("textRole: \"name\"")));
    QVERIFY(presetsSource.contains(QStringLiteral(
        "currentIndex: textPresetsSection.hasSelectedPreset ? "
        "textPresetsSection.selectedPresetIndex : "
        "(textPresetsSection.presetCount > 0 ? 0 : -1)")));
    const qsizetype presetComboEnd = indexOfIgnoringWhitespace(
        presetsSource,
        QStringLiteral("onActivated: (index) => { return "
                       "textPresetsSection.editor.selectPreset(index) }"));
    QVERIFY(presetComboEnd > 0);
    const QString presetComboSource = presetsSource.mid(
        presetsSource.indexOf(QStringLiteral("id: presetSelect")),
        presetComboEnd);
    QVERIFY(presetComboSource.contains(QStringLiteral("contentItem: Label {")));
    QVERIFY(presetComboSource.contains(
        QStringLiteral("text: presetSelect.displayText")));
    QVERIFY(
        presetComboSource.contains(QStringLiteral("font: presetSelect.font")));
    QVERIFY(presetComboSource.contains(
        QStringLiteral("enabled: presetSelect.enabled")));
    QVERIFY(
        presetComboSource.contains(QStringLiteral("elide: Text.ElideRight")));
    QVERIFY(presetComboSource.contains(
        QStringLiteral("verticalAlignment: Text.AlignVCenter")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource,
        QStringLiteral(
            "Button { text: qsTr(\"Apply\"); enabled: "
            "textPresetsSection.sectionReady && "
            "textPresetsSection.hasSelectedPreset; onClicked: "
            "textPresetsSection.editor.applySelectedPreset() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource,
        QStringLiteral("Button { text: qsTr(\"Rename\"); enabled: "
                       "textPresetsSection.sectionReady && "
                       "textPresetsSection.hasSelectedPreset")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource,
        QStringLiteral("Button { text: qsTr(\"Delete\"); enabled: "
                       "textPresetsSection.sectionReady && "
                       "textPresetsSection.hasSelectedPreset")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource,
        QStringLiteral("onActivated: (index) => { return "
                       "textPresetsSection.editor.selectPreset(index) }")));
    QVERIFY(!presetsSource.contains(QStringLiteral("ListView")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("textPresetsSection.editor.applySelectedPreset()")));
    QVERIFY(presetsSource.contains(QStringLiteral("text: qsTr(\"Apply\")")));
    QVERIFY(presetsSource.contains(QStringLiteral("Flow {")));
    QVERIFY(presetsSource.contains(QStringLiteral(
        "textPresetsSection.editor.addPreset(presetNameField.text)")));
    QVERIFY(presetsSource.contains(QStringLiteral("text: qsTr(\"Create\")")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("textPresetsSection.editor.updateSelectedPreset()")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("textPresetsSection.editor.renameSelectedPreset("
                       "presetNameField.text)")));
    QVERIFY(presetsSource.contains(
        QStringLiteral("textPresetsSection.editor.deleteSelectedPreset()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        presetsSource, QStringLiteral("onActiveFocusChanged: "
                                      "textPresetsSection."
                                      "textInputFocusChanged(activeFocus)")));
    QVERIFY(!presetsSource.contains(QStringLiteral(
        "editor.presets[selectedPresetIndex] ||")));
    const qsizetype crudFlowStart =
        presetsSource.indexOf(QStringLiteral("Flow {"));
    const qsizetype createButton = presetsSource.indexOf(
        QStringLiteral("text: qsTr(\"Create\")"), crudFlowStart);
    const qsizetype updateButton = presetsSource.indexOf(
        QStringLiteral("text: qsTr(\"Update\")"), createButton);
    const qsizetype renameButton = presetsSource.indexOf(
        QStringLiteral("text: qsTr(\"Rename\")"), updateButton);
    const qsizetype deleteButton = presetsSource.indexOf(
        QStringLiteral("text: qsTr(\"Delete\")"), renameButton);
    QVERIFY(crudFlowStart >= 0);
    QVERIFY(createButton > crudFlowStart);
    QVERIFY(updateButton > createButton);
    QVERIFY(renameButton > updateButton);
    QVERIFY(deleteButton > renameButton);

    const QString pageTextsSource = readQmlFile(QStringLiteral("PageTextsSection.qml"));
    QFile cmake(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../CMakeLists.txt"));
    QVERIFY(cmake.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmake.readAll());
    QVERIFY(cmakeSource.contains(QStringLiteral("qml/TextPresetsSection.qml")));
    QVERIFY(!pageTextsSource.isEmpty());
    QVERIFY(cmakeSource.contains(QStringLiteral("qml/PageTextsSection.qml")));
    QVERIFY(source.contains(QStringLiteral("id: pageTextsSection")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("id: pageTextsSection")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("property var editor: null")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("readonly property bool sectionReady: "
                       "editor && editor.hasProject && "
                       "editor.pageTexts.length > 0")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("text: pageTextsSection.sectionReady ? "
                       "pageTextsSection.editor.pageTexts.length : 0")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("model: pageTextsSection.sectionReady ? "
                       "pageTextsSection.editor.pageTexts : []")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        leftPanelSource,
        QStringLiteral("PageTextsSection { editor: leftInspectorPanel.editor; "
                       "Layout.fillWidth: true; Layout.minimumWidth: 0; "
                       "Layout.fillHeight: true }")));
    QVERIFY(!leftPanelSource.contains(QStringLiteral("id: pageTextsFrame")));
    QVERIFY(!leftPanelSource.contains(
        QStringLiteral("objectName: \"leftInspectorPageTextsList\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        pageTextsSource,
        QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; "
                       "enabled: pageTextsSection.sectionReady }")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("GroupBox {")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("id: pageTextsFrame")));
    QVERIFY(!pageTextsSource.contains(
        QStringLiteral("color: pageTextsFrame.palette.base")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        pageTextsSource,
        QStringLiteral("horizontalAlignment: Text.AlignRight; text: "
                       "pageTextsSection.sectionReady ? pageTextsSection.editor.pageTexts.length : 0; enabled: "
                       "pageTextsSection.sectionReady")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("enabled: pageTextsSection.sectionReady")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("ListView {")));
    QVERIFY(!pageTextsSource.contains(QStringLiteral("texts.txt")));
    const qsizetype headerCount = indexOfIgnoringWhitespace(
        pageTextsSource,
        QStringLiteral("horizontalAlignment: Text.AlignRight; text: "
                       "pageTextsSection.sectionReady ? pageTextsSection.editor.pageTexts.length : 0; enabled: "
                       "pageTextsSection.sectionReady"));
    const qsizetype pageTextsBox =
        pageTextsSource.indexOf(QStringLiteral("GroupBox {"), headerCount);
    const qsizetype listView =
        pageTextsSource.indexOf(QStringLiteral("ListView {"), pageTextsBox);
    QVERIFY(pageTextsBox > headerCount);
    QVERIFY(listView > pageTextsBox);
    QVERIFY(listView > headerCount);
    QVERIFY(!pageTextsSource.mid(pageTextsBox, listView - pageTextsBox)
                 .contains(QStringLiteral("Page Texts")));
    QVERIFY(!pageTextsSource.mid(pageTextsBox, listView - pageTextsBox)
                 .contains(QStringLiteral("Editor.pageTexts.length")));
    QVERIFY(
        pageTextsSource.contains(QStringLiteral("Layout.fillHeight: true")));
    QVERIFY(
        !pageTextsSource.contains(QStringLiteral("Layout.minimumHeight: 160")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("readonly property real minimumListHeight: 200")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("Layout.minimumHeight: minimumListHeight + topPadding + "
                       "bottomPadding")));
    QVERIFY(pageTextsSource.contains(QStringLiteral(
        "Layout.preferredHeight: pageTextsFrame.minimumListHeight")));
    QVERIFY(pageTextsSource.contains(
        QStringLiteral("model: pageTextsSection.sectionReady ? "
                       "pageTextsSection.editor.pageTexts : []")));
    QVERIFY(
        pageTextsSource.contains(QStringLiteral("delegate: ItemDelegate {")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("hoverEnabled: true")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        pageTextsSource,
        QStringLiteral("background: Rectangle { color: pageTextDelegate.down "
                       "|| pageTextDelegate.hovered ? "
                       "Qt.alpha(pageTextDelegate.palette.highlight")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        pageTextsSource, QStringLiteral("contentItem: Label {")));
    QVERIFY(pageTextsSource.contains(QStringLiteral("elide: Text.ElideRight")));
    QVERIFY(pageTextsSource.contains(QStringLiteral(
        "onClicked: pageTextsSection.editor.applyPageText(index)")));
  }

  void qmlUsesOneOutlinedTextItemDisplayRenderer() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(!source.contains(QStringLiteral("id: effectsPreviewImage")));
    QVERIFY(!source.contains(QStringLiteral("source: Editor.previewImageUrl")));
    QVERIFY(!source.contains(QStringLiteral("effectsPreviewDisplayable")));
    QVERIFY(source.contains(
        QStringLiteral("visible: source.toString().length > 0")));
    QVERIFY(
        !source.contains(QStringLiteral("function boxHasRenderEffects(box)")));
    QVERIFY(!source.contains(
        QStringLiteral("function selectedBoxHasRenderEffects()")));
    QVERIFY(!source.contains(
        QStringLiteral("function boxNeedsPreviewArtifact(box)")));
    QVERIFY(!source.contains(
        QStringLiteral("function anyBoxNeedsPreviewArtifact()")));
    QVERIFY(source.contains(
        QStringLiteral("visible: boxRef.selected && editorRef.editingText")));
    QVERIFY(source.contains(QStringLiteral("OutlinedTextItem {")));
    QCOMPARE(source.count(QStringLiteral("OutlinedTextItem {")), 1);
    QVERIFY(source.contains(QStringLiteral("id: boxOutlinedText")));
    QVERIFY(source.contains(QStringLiteral(
        "width: boxRef.visualDocW * rootWindow.livePreviewScale()")));
    QVERIFY(source.contains(QStringLiteral(
        "height: boxRef.visualDocH * rootWindow.livePreviewScale()")));
    QVERIFY(source.contains(QStringLiteral(
        "scale: rootWindow.viewDocScale() / rootWindow.livePreviewScale()")));
    QVERIFY(source.contains(
        QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
    QVERIFY(!source.contains(QStringLiteral("liveGpuBlurActive")));
    QVERIFY(!source.contains(QStringLiteral("MultiEffect {")));
    QVERIFY(!source.contains(QStringLiteral("source: boxOutlinedText")));
    QVERIFY(source.contains(QStringLiteral(
        "outlineSize: boxRef.boxModel.outline && "
        "boxRef.boxModel.outlineSize > 0 ? "
        "boxRef.boxModel.outlineSize : 0")));
    QVERIFY(source.contains(QStringLiteral(
        "blurSize: boxRef.boxModel.blur && "
        "boxRef.boxModel.blurSize > 0 ? "
        "boxRef.boxModel.blurSize : 0")));
    QVERIFY(source.contains(
        QStringLiteral("shadowEnabled: boxRef.boxModel.shadow")));
    QVERIFY(source.contains(QStringLiteral(
        "shadowBlurSize: boxRef.boxModel.shadow && "
        "boxRef.boxModel.shadowBlurSize > 0 ? "
        "boxRef.boxModel.shadowBlurSize : 0")));
    QVERIFY(source.contains(
        QStringLiteral("gradientEnabled: boxRef.boxModel.gradient")));
    QVERIFY(source.contains(QStringLiteral(
        "pathEnabled: boxRef.boxModel.path && "
        "!boxRef.editingSelected")));
    QVERIFY(source.contains(
        QStringLiteral("pathPoints: boxRef.boxModel.pathPoints")));
    QVERIFY(source.contains(
        QStringLiteral("lineSpacing: boxRef.boxModel.lineSpacing")));
    QVERIFY(source.contains(QStringLiteral(
        "outlineColor: rootWindow.qmlColor(boxRef.boxModel."
        "outlineColor)")));
    QVERIFY(source.indexOf(QStringLiteral("id: boxOutlinedText")) <
            source.indexOf(QStringLiteral("TextArea {")));
    QVERIFY(source.contains(QStringLiteral("editorRef.beginInteraction()")));
    QVERIFY(source.contains(QStringLiteral("editorRef.endInteraction()")));
  }

  void qmlEditingKeepsOutlinedRendererVisible() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype renderer =
        source.indexOf(QStringLiteral("id: boxOutlinedText"));
    const qsizetype editor = source.indexOf(QStringLiteral("id: boxTextArea"));
    QVERIFY(renderer >= 0);
    QVERIFY(editor > renderer);

    const QString rendererBlock = source.mid(renderer, editor - renderer);
    QVERIFY(
        !rendererBlock.contains(QStringLiteral("effectsPreviewDisplayable")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "text: boxRef.modelPreviewText()")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "outlineSize: boxRef.boxModel.outline && "
        "boxRef.boxModel.outlineSize > 0 ? "
        "boxRef.boxModel.outlineSize : 0")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "blurSize: boxRef.boxModel.blur && "
        "boxRef.boxModel.blurSize > 0 ? "
        "boxRef.boxModel.blurSize : 0")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("shadowEnabled: boxRef.boxModel.shadow")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "shadowBlurSize: boxRef.boxModel.shadow && "
        "boxRef.boxModel.shadowBlurSize > 0 ? "
        "boxRef.boxModel.shadowBlurSize : 0")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("gradientEnabled: boxRef.boxModel.gradient")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "pathEnabled: boxRef.boxModel.path && "
        "!boxRef.editingSelected")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("pathPoints: boxRef.boxModel.pathPoints")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("lineSpacing: boxRef.boxModel.lineSpacing")));

    const QString editorBlock = source.mid(
        editor, source.indexOf(QStringLiteral("MouseArea {"), editor) - editor);
    QVERIFY(editorBlock.contains(
        QStringLiteral("visible: boxRef.selected && editorRef.editingText")));
    QVERIFY(editorBlock.contains(QStringLiteral("color: \"transparent\"")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("selectedTextColor: \"transparent\"")));
    QVERIFY(editorBlock.contains(QStringLiteral(
        "selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)")));
    QVERIFY(editorBlock.contains(
        QStringLiteral(
            "property real editLineSpacing: "
            "boxRef.boxModel.lineSpacing")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("scale: rootWindow.viewDocScale()")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("applyTextLineSpacing(textDocument, editLineSpacing)")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("readonly property bool editLayoutAligned: "
                       "boxOutlinedText.editLayoutMetricsValid")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("topPadding: editLayoutTopPadding")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("leftPadding: editLayoutLeftPadding")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("rightPadding: editLayoutRightPadding")));
    QVERIFY(
        editorBlock.contains(QStringLiteral("wrapMode: TextEdit.WordWrap")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("onEditLineSpacingChanged: applyLineSpacing()")));
    QVERIFY(editorBlock.contains(QStringLiteral("cursorDelegate: Rectangle")));
    QVERIFY(editorBlock.contains(QStringLiteral("text: \"\"")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("property bool userInputSyncPending: false")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("property string livePreviewText: boxRef.modelPreviewText()")));
    QVERIFY(editorBlock.contains(
        QStringLiteral("property string pendingUserInputText: \"\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorBlock,
        QStringLiteral("function setLivePreviewText(nextText) { "
                       "livePreviewText = nextText; }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorBlock,
        QStringLiteral("function syncTextFromModel() { const nextText = "
                       "modelText(); if (activeFocus && "
                       "userInputSyncPending) { setLivePreviewText(text); "
                       "userInputSyncPending = false; pendingUserInputText "
                       "= \"\"; applyLineSpacing(); return")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorBlock,
        QStringLiteral("onTextChanged: { setLivePreviewText(text); "
                       "applyLineSpacing(); if (activeFocus && editorRef && "
                       "!syncingTextFromModel) { userInputSyncPending = true; "
                       "pendingUserInputText = text; "
                       "editorRef.updateSelectedText(text); } }")));
  }

  void qmlPerspectiveWarpsLiveOutlinedRenderer() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(
        source.contains(QStringLiteral("function perspectiveMatrix(box, width, "
                                       "height, renderScale, enabled)")));
    QVERIFY(source.contains(QStringLiteral(
        "function perspectiveLayoutCorner(box, name, width, height)")));
    QVERIFY(source.contains(QStringLiteral(
        "function perspectiveCorner(box, name, width, height)")));
    QVERIFY(source.contains(QStringLiteral(
        "function visualHandlePosition(box, name, width, height)")));
    QVERIFY(source.contains(
        QStringLiteral("const corner = perspectiveLayoutCorner(box, name, "
                       "width / scale, height / scale)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("x: corner.x * scale")));
    QVERIFY(source.contains(QStringLiteral(
        "property bool editingSelected: selected && editorRef.editingText")));
    QVERIFY(source.contains(
        QStringLiteral("property bool perspectiveActive: boxModel.perspective "
                       "&& !editingSelected")));
    QVERIFY(source.contains(QStringLiteral("id: boxTextPerspective")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral("transform: Matrix4x4 { matrix: "
                       "boxTextPerspective.rootWindow.perspectiveMatrix("
                       "boxTextPerspective."
                       "boxRef.boxModel, boxTextPerspective.width, "
                       "boxTextPerspective.height, "
                       "boxTextPerspective.rootWindow.viewDocScale(), "
                       "boxTextPerspective.boxRef.perspectiveActive) }")));
    QVERIFY(!source.contains(QStringLiteral(
        "transform: Matrix4x4 { matrix: "
        "rootWindow.perspectiveMatrix(boxRef.boxModel, "
        "boxTextPerspective.width, boxTextPerspective.height, "
        "rootWindow.viewDocScale(), boxRef.perspectiveActive) }")));
    QVERIFY(source.contains(
        QStringLiteral("rootWindow.visualHandlePosition(boxRef.boxModel, "
                       "modelData.name, boxRef.width, boxRef.height).x")));
    QVERIFY(source.contains(
        QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)")));
    QVERIFY(source.contains(QStringLiteral(
        "resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, "
        "modelData.name, point.x, point.y)")));
    QVERIFY(source.contains(QStringLiteral(
        "const delta = rotatePoint((canvasX - resizeStartCanvasX) / scale")));
    QVERIFY(source.contains(QStringLiteral(
        "right = Math.max(resizeStartW + delta.x, left + minimumBoxSize)")));
    QVERIFY(source.contains(QStringLiteral(
        "bottom = Math.max(resizeStartH + delta.y, top + minimumBoxSize)")));
    QVERIFY(source.contains(
        QStringLiteral("left = Math.min(delta.x, right - minimumBoxSize)")));
    QVERIFY(source.contains(
        QStringLiteral("top = Math.min(delta.y, bottom - minimumBoxSize)")));
    QVERIFY(!source.contains(QStringLiteral("offsetFromCenter")));
    QVERIFY(!source.contains(QStringLiteral("dx / length * 16")));
    QVERIFY(!source.contains(QStringLiteral(
        "drag.target: parent; onPressed: Editor.beginInteraction(); "
        "onReleased: Editor.endInteraction(); onCanceled: "
        "Editor.endInteraction(); onPositionChanged: "
        "Editor.setPerspectiveHandle")));
    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype pathStart = source.indexOf(
        QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints"),
        resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(pathStart > resizeStart);
    const QString manualHandleSource =
        source.mid(resizeStart, pathStart - resizeStart);
    QVERIFY(!manualHandleSource.contains(QStringLiteral("drag.target")));
    QVERIFY(manualHandleSource.contains(
        QStringLiteral("mapToItem(canvasItem, mouse.x, mouse.y)")));
    QVERIFY(source.contains(
        QStringLiteral("window.editor.setSelectedBounds(bounds.x, bounds.y, "
                       "bounds.width, bounds.height)")));
    QVERIFY(
        source.contains(QStringLiteral("window.editor.setPerspectiveHandle")));
    QVERIFY(source.contains(
        QStringLiteral("border.width: perspectiveActive ? 0 : selected ? "
                       "rootWindow.selectionLineWidth() : Math.max(1, "
                       "rootWindow.documentToViewLength(1))")));
    QVERIFY(source.contains(QStringLiteral("id: perspectiveBorder")));
    QVERIFY(source.contains(QStringLiteral(
        "visible: boxRef.selected && boxRef.perspectiveActive")));
    QVERIFY(source.contains(QStringLiteral("return Qt.matrix4x4(1, 0, 0, 0,")));

    const qsizetype wrapper =
        source.indexOf(QStringLiteral("id: boxTextPerspective"));
    const qsizetype renderer =
        source.indexOf(QStringLiteral("id: boxOutlinedText"), wrapper);
    const qsizetype pathGuide =
        source.indexOf(QStringLiteral("TextPathGuide {"), renderer);
    const qsizetype editor =
        source.indexOf(QStringLiteral("TextArea {"), pathGuide);
    QVERIFY(wrapper >= 0);
    QVERIFY(renderer > wrapper);
    QVERIFY(pathGuide > renderer);
    QVERIFY(editor > renderer);
    const qsizetype rendererEnd =
        source.indexOf(QStringLiteral("transform: Matrix4x4"), renderer);
    QVERIFY(rendererEnd > renderer);
    QVERIFY(source.mid(renderer, rendererEnd - renderer)
                .contains(QStringLiteral("Matrix4x4")) == false);
  }

  void qmlPerspectiveResizeDragStartsFromVisualHandle() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype rotateUse =
        source.indexOf(QStringLiteral("rootWindow.rotateHandlePosition(boxRef."
                                      "boxModel, boxRef.width, boxRef.height)"),
                       resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateUse > resizeStart);
    const QString resizeSource =
        source.mid(resizeStart, rotateUse - resizeStart);

    QVERIFY(resizeSource.contains(QStringLiteral(
        "x: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, "
        "boxRef.width, boxRef.height).x - width / 2")));
    QVERIFY(resizeSource.contains(QStringLiteral(
        "const point = mapToItem(canvasItem, mouse.x, mouse.y)")));
    const qsizetype perspectiveBranch = resizeSource.indexOf(
        QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
    const qsizetype perspectiveBegin = resizeSource.indexOf(QStringLiteral(
        "resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, "
        "modelData.name, point.x, point.y)"));
    const qsizetype normalBegin = resizeSource.indexOf(
        QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle."
                       "boxRef, modelData.name, point.x, point.y)"));
    QVERIFY(perspectiveBranch >= 0);
    QVERIFY(perspectiveBegin > perspectiveBranch);
    QVERIFY(normalBegin > perspectiveBegin);
    QVERIFY(
        source.contains(QStringLiteral("perspectiveStartCanvasX = canvasX")));
    QVERIFY(
        source.contains(QStringLiteral("perspectiveStartCanvasY = canvasY")));
    QVERIFY(source.contains(
        QStringLiteral("applyPerspectiveHandles(perspectiveInteraction.commitHandles())")));
  }

  void qmlPerspectiveDragDoesNotApplyVisualOffsetToGeometryDelta() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(
        !source.contains(QStringLiteral("canvasX - perspectiveStartCanvasX")));
    QVERIFY(!source.contains(
        QStringLiteral("visualHandlePosition(box, activePerspectiveHandle")));
    QVERIFY(!source.contains(
        QStringLiteral("offsetFromCenter(activePerspectiveHandle")));
    QVERIFY(!source.contains(
        QStringLiteral("perspectiveHandleOffset(activePerspectiveHandle")));
  }

  void qmlNormalResizePathRemainsSeparateFromPerspectiveDrag() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype rotateUse =
        source.indexOf(QStringLiteral("rootWindow.rotateHandlePosition(boxRef."
                                      "boxModel, boxRef.width, boxRef.height)"),
                       resizeStart);
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateUse > resizeStart);
    const QString resizeSource =
        source.mid(resizeStart, rotateUse - resizeStart);

    const qsizetype perspectiveBranch = resizeSource.indexOf(
        QStringLiteral("if (resizeHandle.boxRef.perspectiveActive)"));
    const qsizetype perspectiveBegin = resizeSource.indexOf(QStringLiteral(
        "resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, "
        "modelData.name, point.x, point.y)"));
    const qsizetype normalBegin = resizeSource.indexOf(
        QStringLiteral("resizeHandle.rootWindow.beginResizeDrag(resizeHandle."
                       "boxRef, modelData.name, point.x, point.y)"));
    QVERIFY(perspectiveBranch >= 0);
    QVERIFY(perspectiveBegin > perspectiveBranch);
    QVERIFY(normalBegin > perspectiveBegin);
    QVERIFY(resizeSource.contains(QStringLiteral(
        "resizeHandle.rootWindow.updatePerspectiveDrag(point.x, point.y)")));
    QVERIFY(resizeSource.contains(QStringLiteral(
        "resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)")));
    QVERIFY(source.contains(QStringLiteral(
        "function beginResizeDrag(delegate, handle, canvasX, canvasY)")));
    QVERIFY(source.contains(QStringLiteral(
        "function beginPerspectiveDrag(delegate, handle, canvasX, canvasY)")));
  }

  void qmlPerspectiveHandlesStayOnGeometry() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(!source.contains(QStringLiteral("perspectiveHandleOffset")));
    QVERIFY(!source.contains(
        QStringLiteral("function offsetFromCenter(point, width, height)")));
    QVERIFY(source.contains(
        QStringLiteral("return perspectiveCorner(box, name, width, height)")));
    QVERIFY(source.contains(
        QStringLiteral("function rotateHandlePosition(box, width, height)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral(
                    "return { x: top.x, y: top.y - rotateHandleDistance }")));

    const qsizetype resizeStart = indexOfIgnoringWhitespace(
        source, QStringLiteral("model: [\n            {name: \"nw\"}"));
    const qsizetype rotateUse = source.indexOf(
        QStringLiteral("rootWindow.rotateHandlePosition(boxRef.boxModel, "
                       "boxRef.width, boxRef.height)"));
    QVERIFY(resizeStart >= 0);
    QVERIFY(rotateUse > resizeStart);
    QVERIFY(source.mid(resizeStart, rotateUse - resizeStart)
                .contains(QStringLiteral(
                    "rootWindow.visualHandlePosition(boxRef.boxModel, "
                    "modelData.name, boxRef.width, boxRef.height).x")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source.mid(resizeStart, rotateUse - resizeStart),
        QStringLiteral("width: rootWindow.handleSize(); height: width; radius: "
                       "width / 2")));

    const qsizetype borderStart =
        source.indexOf(QStringLiteral("id: perspectiveBorder"));
    const qsizetype borderEnd =
        source.indexOf(QStringLiteral("OutlinedTextItem"), borderStart);
    QVERIFY(borderStart >= 0);
    QVERIFY(borderEnd > borderStart);
    const QString borderSource =
        source.mid(borderStart, borderEnd - borderStart);
    QVERIFY(borderSource.contains(
        QStringLiteral("rootWindow.perspectiveCorner(boxRef.boxModel, \"nw\", "
                       "boxRef.width, boxRef.height)")));
    QVERIFY(!borderSource.contains(QStringLiteral("visualHandlePosition")));
    QVERIFY(!borderSource.contains(QStringLiteral("offsetFromCenter")));
    QVERIFY(!borderSource.contains(QStringLiteral("perspectiveHandleOffset")));

    QVERIFY(!source.contains(QStringLiteral("resizeHandleOffset")));
  }

  void qmlZoomUsesSingleDocumentToViewScaleContract() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype renderer =
        source.indexOf(QStringLiteral("id: boxOutlinedText"));
    const qsizetype editor =
        source.indexOf(QStringLiteral("TextArea {"), renderer);
    QVERIFY(renderer >= 0);
    QVERIFY(editor > renderer);
    const QString rendererBlock = source.mid(renderer, editor - renderer);

    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral(
                    "function documentToViewLength(value) { "
                    "return viewportMetrics.documentToViewLength(value) }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        readQmlFile(QStringLiteral("ViewportMetrics.qml")),
        QStringLiteral("function documentToViewLength(value) { "
                       "return value * viewDocScale() }")));
    QVERIFY(source.contains(
        QStringLiteral("width: visualDocW * rootWindow.viewDocScale()")));
    QVERIFY(source.contains(
        QStringLiteral("height: visualDocH * rootWindow.viewDocScale()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("function livePreviewScale() { return "
                               "viewportMetrics.livePreviewScale() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        readQmlFile(QStringLiteral("ViewportMetrics.qml")),
        QStringLiteral("function livePreviewScale() { return Math.min(1, "
                       "viewDocScale()) }")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "width: boxRef.visualDocW * rootWindow.livePreviewScale()")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "height: boxRef.visualDocH * rootWindow.livePreviewScale()")));
    QVERIFY(rendererBlock.contains(QStringLiteral(
        "scale: rootWindow.viewDocScale() / rootWindow.livePreviewScale()")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("renderScale: rootWindow.livePreviewScale()")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral(
            "pixelSize: Math.max(1, boxRef.boxModel.fontSize)")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral(
            "letterSpacing: boxRef.boxModel.letterSpacing")));
    QVERIFY(rendererBlock.contains(
        QStringLiteral("lineSpacing: boxRef.boxModel.lineSpacing")));
    QVERIFY(!rendererBlock.contains(
        QStringLiteral("modelData.fontSize * rootWindow.viewDocScale()")));
    QVERIFY(!rendererBlock.contains(
        QStringLiteral("modelData.letterSpacing * rootWindow.viewDocScale()")));
    QVERIFY(!rendererBlock.contains(
        QStringLiteral("modelData.lineSpacing * rootWindow.viewDocScale()")));
    QVERIFY(!rendererBlock.contains(
        QStringLiteral("modelData.outlineSize * rootWindow.viewDocScale()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral("transform: Matrix4x4 { matrix: "
                       "boxTextPerspective.rootWindow.perspectiveMatrix("
                       "boxTextPerspective."
                       "boxRef.boxModel, boxTextPerspective.width, "
                       "boxTextPerspective.height, "
                       "boxTextPerspective.rootWindow.viewDocScale(), "
                       "boxTextPerspective.boxRef.perspectiveActive) }")));
    QVERIFY(!source.contains(QStringLiteral(
        "transform: Matrix4x4 { matrix: "
        "rootWindow.perspectiveMatrix(boxRef.boxModel, "
        "boxTextPerspective.width, boxTextPerspective.height, "
        "rootWindow.viewDocScale(), boxRef.perspectiveActive) }")));
    QVERIFY(
        source.contains(QStringLiteral("const layoutWidth = width / scale")));
    QVERIFY(
        source.contains(QStringLiteral("const layoutHeight = height / scale")));
    QVERIFY(source.contains(
        QStringLiteral("const p0 = perspectiveLayoutCorner(box, \"nw\", "
                       "layoutWidth, layoutHeight)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("x: perspectiveBase(name, 0) * width + "
                               "perspectiveOffset(box, name, 0)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("x: corner.x * scale")));
    QVERIFY(source.contains(
        QStringLiteral("border.width: perspectiveActive ? 0 : selected ? "
                       "rootWindow.selectionLineWidth() : Math.max(1, "
                       "rootWindow.documentToViewLength(1))")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("width: rootWindow.handleSize(); height: width; "
                               "radius: width / 2")));
    QVERIFY(source.contains(
        QStringLiteral("ctx.lineWidth = rootWindow.selectionLineWidth()")));
    QVERIFY(!source.contains(
        QStringLiteral("viewDocScale() * window.viewDocScale()")));
    QVERIFY(!source.contains(
        QStringLiteral("viewDocScale() * rootWindow.viewDocScale()")));
    QVERIFY(!source.contains(QStringLiteral("window.zoom * window.zoom")));
  }

  void qmlHasEightPerspectiveHandlesAndQPainterUsesFontResolver() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString qmlSource = ::qmlSource();

    for (const QString &handle :
         {QStringLiteral("nw"), QStringLiteral("n"), QStringLiteral("ne"),
          QStringLiteral("e"), QStringLiteral("se"), QStringLiteral("s"),
          QStringLiteral("sw"), QStringLiteral("w")}) {
      QVERIFY2(sourceContainsIgnoringWhitespace(
                   qmlSource,
                   QStringLiteral("name: \"") + handle + QStringLiteral("\"")),
               qPrintable(handle));
    }
    QVERIFY(qmlSource.contains(QStringLiteral("perspectiveOffset")));
    QVERIFY(!qmlSource.contains(QStringLiteral("perspectiveHandleOffset")));
    QVERIFY(!qmlSource.contains(QStringLiteral(
        "visible: !canvas.effectsPreviewDisplayable || Editor.editingText")));

    QFile render(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../src/render/RenderGraph.cpp"));
    QVERIFY(render.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString renderSource = QString::fromUtf8(render.readAll());
    QVERIFY(renderSource.contains(
        QStringLiteral("return resolveFont(font).font;")));
    QVERIFY(
        renderSource.contains(QStringLiteral("font.setBold(box.style.bold)")));
    QVERIFY(!renderSource.contains(
        QStringLiteral("SkFontMgr_New_Custom_Directory")));
  }

  void qmlUsesRawOverlayAndTopLayerFirstList() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(source.contains(QStringLiteral("id: rawOverlayImage")));
    QVERIFY(source.contains(QStringLiteral("opacity: 0.45")));
    QVERIFY(!source.contains(QStringLiteral(
        "Editor.rawVisible && Editor.rawPageUrl.toString().length > 0 ? "
        "Editor.rawPageUrl : Editor.currentPageUrl")));
    QVERIFY(source.contains(
        QStringLiteral("model: layersSection.editor.layers")));
    for (const QString iconPath : {
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/arrow-left.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/arrow-right.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/arrow-up.svg"),
             QStringLiteral(":/qt/qml/TextFX/assets/icons/flat/arrow-down.svg"),
         }) {
      QVERIFY2(QFile::exists(iconPath), qPrintable(iconPath));
    }
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral(
            "text: qsTr(\"Up\"); enabled: "
            "layersSection.editor.selectedIndex "
            ">= 0 && layersSection.editor.selectedIndex < "
            "layersSection.editor.boxCount - 1; display: "
            "AbstractButton.IconOnly; icon.source: "
            "\"qrc:/qt/qml/TextFX/assets/icons/flat/arrow-up.svg\"; "
            "icon.width: 20; icon.height: 20; icon.color: !enabled ? "
            "palette.mid : palette.buttonText; onClicked: "
            "layersSection.editor.moveLayer(layersSection.editor."
            "selectedIndex + 1)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral(
            "text: qsTr(\"Down\"); enabled: "
            "layersSection.editor.selectedIndex > 0; display: "
            "AbstractButton.IconOnly; icon.source: "
            "\"qrc:/qt/qml/TextFX/assets/icons/flat/arrow-down.svg\"; "
            "icon.width: 20; icon.height: 20; icon.color: !enabled ? "
            "palette.mid : palette.buttonText; onClicked: "
            "layersSection.editor.moveLayer(layersSection.editor."
            "selectedIndex - 1)")));
  }
};

QTEST_MAIN(QtSmokeTests)

#include "qt_smoke_tests.moc"
