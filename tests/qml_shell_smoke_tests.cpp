#include "app/EditorController.h"
#include "qt_test_helpers.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <QWheelEvent>

#include <cmath>
#include <limits>

using namespace textfx;
using namespace textfx::test;

class QmlShellSmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void qmlCentralCanvasShellForwardsViewportInteractions()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* shellObject = nullptr;
        QTRY_VERIFY(shellObject = findVisualChildByName(window->contentItem(), QStringLiteral("centralCanvasShell")));
        auto* shell = qobject_cast<QQuickItem*>(shellObject);
        QVERIFY(shell);
        QObject* canvasObject = nullptr;
        QTRY_VERIFY(canvasObject = findVisualChildByName(window->contentItem(), QStringLiteral("centralCanvas")));
        auto* canvas = qobject_cast<QQuickItem*>(canvasObject);
        QVERIFY(canvas);
        QTRY_VERIFY(canvas->width() > shell->width());
        QCOMPARE(canvas->height(), shell->height());

        const auto windowCoordinate = [&](const char* method, qreal value) {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(window, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, value));
            return invoked ? result.toDouble() : std::numeric_limits<double>::quiet_NaN();
        };
        const auto expectedCreatedBox = [&](const QPointF& start, const QPointF& end) {
            const QPointF topLeft(qMin(start.x(), end.x()), qMin(start.y(), end.y()));
            return QVariantMap{
                {QStringLiteral("x"), windowCoordinate("viewToDocumentX", topLeft.x())},
                {QStringLiteral("y"), windowCoordinate("viewToDocumentY", topLeft.y())},
                {QStringLiteral("w"), std::abs(windowCoordinate("viewToDocumentX", end.x()) - windowCoordinate("viewToDocumentX", start.x()))},
                {QStringLiteral("h"), std::abs(windowCoordinate("viewToDocumentY", end.y()) - windowCoordinate("viewToDocumentY", start.y()))},
            };
        };
        const auto assertBoxGeometry = [](const QVariantMap& actual, const QVariantMap& expected) {
            const auto nearlyEqual = [](double a, double b) { return std::abs(a - b) <= 1.0; };
            QVERIFY(nearlyEqual(actual.value(QStringLiteral("x")).toDouble(), expected.value(QStringLiteral("x")).toDouble()));
            QVERIFY(nearlyEqual(actual.value(QStringLiteral("y")).toDouble(), expected.value(QStringLiteral("y")).toDouble()));
            QVERIFY(nearlyEqual(actual.value(QStringLiteral("w")).toDouble(), expected.value(QStringLiteral("w")).toDouble()));
            QVERIFY(nearlyEqual(actual.value(QStringLiteral("h")).toDouble(), expected.value(QStringLiteral("h")).toDouble()));
        };
        const auto ctrlDrag = [&](const QPointF& shellStart, const QPoint& delta) {
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
        QVERIFY(editor.boxes().at(1).toMap().value(QStringLiteral("x")).toDouble() < windowCoordinate("viewToDocumentX", shell->mapToScene(QPointF(300, 220)).x()));
        QVERIFY(editor.boxes().at(1).toMap().value(QStringLiteral("y")).toDouble() < windowCoordinate("viewToDocumentY", 220.0));

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

        const qreal zoomBeforeWheel = window->property("zoom").toReal();
        const QPointF wheelPosition = shell->mapToScene(QPointF(220, 180));
        QWheelEvent wheelEvent(
            wheelPosition,
            window->mapToGlobal(wheelPosition.toPoint()),
            QPoint(),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QCoreApplication::sendEvent(window, &wheelEvent);
        QVERIFY(wheelEvent.isAccepted());
        QTRY_VERIFY(window->property("zoom").toReal() > zoomBeforeWheel);
    }

    void qmlViewportMetricsConvertsCoordinatesAndZoomsAroundFocus()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto* viewport = window->findChild<QObject*>(QStringLiteral("viewportMetrics"));
        QVERIFY(viewport);
        QVERIFY(viewport->setProperty("canvasWidth", 500.0));
        QVERIFY(viewport->setProperty("canvasHeight", 400.0));
        QVERIFY(viewport->setProperty("pageSourceWidth", 200.0));
        QVERIFY(viewport->setProperty("pageSourceHeight", 100.0));
        QVERIFY(window->setProperty("pageBaseScale", 0.5));
        QVERIFY(window->setProperty("zoom", 2.0));
        QVERIFY(window->setProperty("panX", 10.0));
        QVERIFY(window->setProperty("panY", 20.0));

        const auto callWindow = [&](const char* method, double value) {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(window, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, value));
            return invoked ? result.toDouble() : std::numeric_limits<double>::quiet_NaN();
        };

        QCOMPARE(callWindow("documentToViewLength", 12.0), 12.0);
        QCOMPARE(callWindow("documentToViewX", 10.0), 420.0);
        QCOMPARE(callWindow("documentToViewY", 30.0), 400.0);
        QCOMPARE(callWindow("viewToDocumentX", 420.0), 10.0);
        QCOMPARE(callWindow("viewToDocumentY", 400.0), 30.0);

        QVERIFY(QMetaObject::invokeMethod(window, "zoomAt", Q_ARG(QVariant, 100.0), Q_ARG(QVariant, 80.0), Q_ARG(QVariant, 1.5)));
        QCOMPARE(window->property("zoom").toDouble(), 3.0);
        QCOMPARE(window->property("panX").toDouble(), -35.0);
        QCOMPARE(window->property("panY").toDouble(), -10.0);

        QVERIFY(window->setProperty("zoom", 6.0));
        QVERIFY(window->setProperty("panX", 14.0));
        QVERIFY(window->setProperty("panY", 18.0));
        QVERIFY(QMetaObject::invokeMethod(window, "zoomAt", Q_ARG(QVariant, 100.0), Q_ARG(QVariant, 80.0), Q_ARG(QVariant, 1.5)));
        QCOMPARE(window->property("zoom").toDouble(), 6.0);
        QCOMPARE(window->property("panX").toDouble(), 14.0);
        QCOMPARE(window->property("panY").toDouble(), 18.0);
    }

    void qmlBoxResizeInteractionStateCommitsVisibleBoundsAndClamps()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 50, 100, 60);
        editor.rotateSelected(30.0);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* boxObject = nullptr;
        QTRY_VERIFY(boxObject = findVisualChildByName(window->contentItem(), QStringLiteral("textBoxDelegate")));

        QVERIFY(QMetaObject::invokeMethod(window, "beginResizeDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, QStringLiteral("e")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateResizeDrag", Q_ARG(QVariant, 30.0), Q_ARG(QVariant, 0.0)));
        QTRY_VERIFY(window->property("resizeW").toDouble() > 120.0);
        QVERIFY(QMetaObject::invokeMethod(window, "endResizeDrag", Q_ARG(QVariant, true)));

        QVariantMap grown = editor.boxes().at(0).toMap();
        QVERIFY(grown.value(QStringLiteral("w")).toDouble() > 120.0);
        QVERIFY(grown.value(QStringLiteral("x")).toDouble() > 30.0);
        QVERIFY(grown.value(QStringLiteral("y")).toDouble() > 40.0);

        boxObject = nullptr;
        QTRY_VERIFY(boxObject = findVisualChildByName(window->contentItem(), QStringLiteral("textBoxDelegate")));
        QVERIFY(QMetaObject::invokeMethod(window, "beginResizeDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, QStringLiteral("e")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateResizeDrag", Q_ARG(QVariant, -1000.0), Q_ARG(QVariant, 0.0)));
        QTRY_COMPARE(window->property("resizeW").toDouble(), 12.0);
        QVERIFY(QMetaObject::invokeMethod(window, "endResizeDrag", Q_ARG(QVariant, true)));

        const QVariantMap clamped = editor.boxes().at(0).toMap();
        QCOMPARE(clamped.value(QStringLiteral("w")).toDouble(), 12.0);
        QCOMPARE(clamped.value(QStringLiteral("h")).toDouble(), 60.0);
    }

    void qmlBoxMoveInteractionStateCommitsVisiblePositionAndCancels()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 50, 100, 60);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* boxObject = nullptr;
        QTRY_VERIFY(boxObject = findVisualChildByName(window->contentItem(), QStringLiteral("textBoxDelegate")));
        auto* boxItem = qobject_cast<QQuickItem*>(boxObject);
        QVERIFY(boxItem);

        const QVariantMap original = editor.boxes().at(0).toMap();
        const qreal originalVisualX = boxItem->x();
        const qreal originalVisualY = boxItem->y();

        QVERIFY(QMetaObject::invokeMethod(window, "beginMoveDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, 0),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateMoveDrag", Q_ARG(QVariant, 25.0), Q_ARG(QVariant, -10.0)));
        QTRY_COMPARE(window->property("moveX").toDouble(), 65.0);
        QTRY_COMPARE(window->property("moveY").toDouble(), 40.0);
        QTRY_VERIFY(boxItem->x() > originalVisualX + 20.0);
        QTRY_VERIFY(boxItem->y() < originalVisualY - 5.0);
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(), original.value(QStringLiteral("x")).toDouble());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(), original.value(QStringLiteral("y")).toDouble());

        QVERIFY(QMetaObject::invokeMethod(window, "endMoveDrag", Q_ARG(QVariant, false)));
        QCoreApplication::processEvents();
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("x")).toDouble(), original.value(QStringLiteral("x")).toDouble());
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("y")).toDouble(), original.value(QStringLiteral("y")).toDouble());

        QVERIFY(QMetaObject::invokeMethod(window, "beginMoveDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, 0),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateMoveDrag", Q_ARG(QVariant, 25.0), Q_ARG(QVariant, -10.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "endMoveDrag", Q_ARG(QVariant, true)));

        const QVariantMap moved = editor.boxes().at(0).toMap();
        QCOMPARE(moved.value(QStringLiteral("x")).toDouble(), original.value(QStringLiteral("x")).toDouble() + 25.0);
        QCOMPARE(moved.value(QStringLiteral("y")).toDouble(), original.value(QStringLiteral("y")).toDouble() - 10.0);
    }

    void qmlPerspectiveGeometryCalculatesHandlesBoundsAndHitArea()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto* geometry = window->findChild<QObject*>(QStringLiteral("perspectiveGeometry"));
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
        QVERIFY(QMetaObject::invokeMethod(geometry, "visualHandlePosition",
            Q_RETURN_ARG(QVariant, northHandle),
            Q_ARG(QVariant, box),
            Q_ARG(QVariant, QStringLiteral("n")),
            Q_ARG(QVariant, 200.0),
            Q_ARG(QVariant, 100.0)));
        const QVariantMap north = northHandle.toMap();
        QCOMPARE(north.value(QStringLiteral("x")).toDouble(), 90.0);
        QCOMPARE(north.value(QStringLiteral("y")).toDouble(), 0.0);

        QVariant boundsValue;
        QVERIFY(QMetaObject::invokeMethod(geometry, "perspectiveVisualBounds",
            Q_RETURN_ARG(QVariant, boundsValue),
            Q_ARG(QVariant, box),
            Q_ARG(QVariant, 200.0),
            Q_ARG(QVariant, 100.0)));
        const QVariantMap bounds = boundsValue.toMap();
        QVERIFY(bounds.value(QStringLiteral("x")).toDouble() < 0.0);
        QVERIFY(bounds.value(QStringLiteral("y")).toDouble() < 0.0);
        QVERIFY(bounds.value(QStringLiteral("width")).toDouble() > 200.0);
        QVERIFY(bounds.value(QStringLiteral("height")).toDouble() > 100.0);

        const QVariantMap insidePoint{{QStringLiteral("x"), 100.0}, {QStringLiteral("y"), 50.0}};
        QVariant inside;
        QVERIFY(QMetaObject::invokeMethod(geometry, "pointInPerspectivePolygon",
            Q_RETURN_ARG(QVariant, inside),
            Q_ARG(QVariant, insidePoint),
            Q_ARG(QVariant, box),
            Q_ARG(QVariant, 200.0),
            Q_ARG(QVariant, 100.0)));
        QVERIFY(inside.toBool());

        const QVariantMap outsidePoint{{QStringLiteral("x"), -30.0}, {QStringLiteral("y"), -30.0}};
        QVariant outside;
        QVERIFY(QMetaObject::invokeMethod(geometry, "pointInPerspectivePolygon",
            Q_RETURN_ARG(QVariant, outside),
            Q_ARG(QVariant, outsidePoint),
            Q_ARG(QVariant, box),
            Q_ARG(QVariant, 200.0),
            Q_ARG(QVariant, 100.0)));
        QVERIFY(!outside.toBool());

        QVERIFY(geometry->setProperty("livePerspectiveActive", true));
        QVERIFY(geometry->setProperty("activePerspectiveBoxIndex", 0));
        QVERIFY(geometry->setProperty("activePerspectiveHandle", QStringLiteral("ne")));
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
        QVERIFY(QMetaObject::invokeMethod(geometry, "perspectiveLayoutCorner",
            Q_RETURN_ARG(QVariant, selectedLiveCorner),
            Q_ARG(QVariant, box),
            Q_ARG(QVariant, QStringLiteral("ne")),
            Q_ARG(QVariant, 100.0),
            Q_ARG(QVariant, 50.0)));
        QVariant otherCorner;
        QVERIFY(QMetaObject::invokeMethod(geometry, "perspectiveLayoutCorner",
            Q_RETURN_ARG(QVariant, otherCorner),
            Q_ARG(QVariant, otherBox),
            Q_ARG(QVariant, QStringLiteral("ne")),
            Q_ARG(QVariant, 100.0),
            Q_ARG(QVariant, 50.0)));
        QCOMPARE(selectedLiveCorner.toMap().value(QStringLiteral("x")).toDouble(), 142.0);
        QCOMPARE(selectedLiveCorner.toMap().value(QStringLiteral("y")).toDouble(), -7.0);
        QCOMPARE(otherCorner.toMap().value(QStringLiteral("x")).toDouble(), 80.0);
        QCOMPARE(otherCorner.toMap().value(QStringLiteral("y")).toDouble(), 5.0);
    }
};

QTEST_MAIN(QmlShellSmokeTests)

#include "qml_shell_smoke_tests.moc"
