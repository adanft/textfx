#include "app/EditorController.h"
#include "qt_test_helpers.h"

#include <QCoreApplication>
#include <QFile>
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

    void qmlBoxRotateInteractionStatePreviewsCancelsAndCommits()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 50, 100, 60);
        editor.setSelectedRotation(10.0);

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

        const auto documentToView = [&](const char* method, double value) {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(window, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, value));
            return invoked ? result.toDouble() : std::numeric_limits<double>::quiet_NaN();
        };
        const auto nearlyEqual = [](double a, double b) { return std::abs(a - b) <= 0.001; };
        const double centerX = 90.0;
        const double centerY = 80.0;
        const double startX = documentToView("documentToViewX", centerX);
        const double startY = documentToView("documentToViewY", 0.0);
        const double endX = documentToView("documentToViewX", 190.0);
        const double endY = documentToView("documentToViewY", centerY);

        QVERIFY(QMetaObject::invokeMethod(window, "beginRotateDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, startX),
            Q_ARG(QVariant, startY)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag", Q_ARG(QVariant, endX), Q_ARG(QVariant, endY)));
        QTRY_VERIFY(nearlyEqual(window->property("rotateDegrees").toDouble(), 100.0));
        QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 100.0));
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 10.0);

        QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag", Q_ARG(QVariant, false)));
        QCoreApplication::processEvents();
        QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 10.0));
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 10.0);

        QVERIFY(QMetaObject::invokeMethod(window, "beginRotateDrag",
            Q_ARG(QVariant, QVariant::fromValue(boxObject)),
            Q_ARG(QVariant, startX),
            Q_ARG(QVariant, startY)));
        QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag", Q_ARG(QVariant, endX), Q_ARG(QVariant, endY)));
        QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag", Q_ARG(QVariant, true)));
        QTRY_VERIFY(nearlyEqual(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 100.0));
    }

    void qmlBoxRotateInteractionStateWrapsAcrossAngleBoundary()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(40, 50, 100, 60);
        editor.setSelectedRotation(10.0);

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

        const auto documentToView = [&](const char* method, double value) {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(window, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, value));
            return invoked ? result.toDouble() : std::numeric_limits<double>::quiet_NaN();
        };
        const auto nearlyEqual = [](double a, double b) { return std::abs(a - b) <= 0.001; };
        const QPointF center(90.0, 80.0);
        constexpr double pi = 3.14159265358979323846;
        const double radius = 100.0;
        const auto viewPointAtAngle = [&](double degrees) {
            const double radians = degrees * pi / 180.0;
            const double documentX = center.x() + std::cos(radians) * radius;
            const double documentY = center.y() + std::sin(radians) * radius;
            return QPointF(documentToView("documentToViewX", documentX), documentToView("documentToViewY", documentY));
        };
        const auto dragPreview = [&](double fromDegrees, double toDegrees) {
            const QPointF start = viewPointAtAngle(fromDegrees);
            const QPointF end = viewPointAtAngle(toDegrees);
            QVERIFY(QMetaObject::invokeMethod(window, "beginRotateDrag",
                Q_ARG(QVariant, QVariant::fromValue(boxObject)),
                Q_ARG(QVariant, start.x()),
                Q_ARG(QVariant, start.y())));
            QVERIFY(QMetaObject::invokeMethod(window, "updateRotateDrag", Q_ARG(QVariant, end.x()), Q_ARG(QVariant, end.y())));
        };

        dragPreview(179.0, -179.0);
        QTRY_VERIFY(nearlyEqual(window->property("rotateDegrees").toDouble(), 12.0));
        QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 12.0));
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 10.0);
        QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag", Q_ARG(QVariant, false)));
        QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 10.0));

        dragPreview(-179.0, 179.0);
        QTRY_VERIFY(nearlyEqual(window->property("rotateDegrees").toDouble(), 8.0));
        QTRY_VERIFY(nearlyEqual(boxItem->rotation(), 8.0));
        QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 10.0);
        QVERIFY(QMetaObject::invokeMethod(window, "endRotateDrag", Q_ARG(QVariant, true)));
        QTRY_VERIFY(nearlyEqual(editor.boxes().at(0).toMap().value(QStringLiteral("rotation")).toDouble(), 8.0));
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

    void qmlPerspectiveInteractionStatePreviewsCancelsCommitsAndScopesSelectedBox()
    {
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
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        const auto nearlyEqual = [](double a, double b) { return std::abs(a - b) <= 0.001; };
        const auto collectDelegates = [](auto&& self, QQuickItem* item, QList<QObject*>& results) -> void {
            if (!item) return;
            if (item->objectName() == QStringLiteral("textBoxDelegate")) results.append(item);
            for (QQuickItem* child : item->childItems()) self(self, child, results);
        };
        const auto modelIndex = [](QObject* delegate) {
            return delegate->property("boxModel").toMap().value(QStringLiteral("index")).toInt();
        };
        const auto pointValue = [](const QVariantMap& box, const QString& name, int axis) {
            return box.value(QStringLiteral("perspective") + name.left(1).toUpper() + name.mid(1)).toList().at(axis).toDouble();
        };
        const auto invokeLayoutCorner = [](QObject* geometry, const QVariantMap& box, const QString& name) {
            QVariant result;
            const bool invoked = QMetaObject::invokeMethod(geometry, "perspectiveLayoutCorner",
                Q_RETURN_ARG(QVariant, result),
                Q_ARG(QVariant, box),
                Q_ARG(QVariant, name),
                Q_ARG(QVariant, 100.0),
                Q_ARG(QVariant, 60.0));
            return invoked ? result.toMap() : QVariantMap{};
        };

        QList<QObject*> delegates;
        collectDelegates(collectDelegates, window->contentItem(), delegates);
        QTRY_COMPARE(delegates.size(), 2);
        QObject* selectedDelegate = modelIndex(delegates.at(0)) == 0 ? delegates.at(0) : delegates.at(1);
        QObject* otherDelegate = selectedDelegate == delegates.at(0) ? delegates.at(1) : delegates.at(0);
        QCOMPARE(modelIndex(selectedDelegate), 0);
        QCOMPARE(modelIndex(otherDelegate), 1);

        auto* geometry = window->findChild<QObject*>(QStringLiteral("perspectiveGeometry"));
        QVERIFY(geometry);
        auto* perspectiveState = window->findChild<QObject*>(QStringLiteral("perspectiveInteractionState"));
        QVERIFY(perspectiveState);

        QVERIFY(QMetaObject::invokeMethod(window, "beginPerspectiveDrag",
            Q_ARG(QVariant, QVariant::fromValue(selectedDelegate)),
            Q_ARG(QVariant, QStringLiteral("ne")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag", Q_ARG(QVariant, 20.0), Q_ARG(QVariant, -10.0)));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 20.0));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveY").toDouble(), -10.0));

        QVariantMap selectedPreview = invokeLayoutCorner(geometry, editor.boxes().at(0).toMap(), QStringLiteral("ne"));
        QVariantMap otherPreview = invokeLayoutCorner(geometry, editor.boxes().at(1).toMap(), QStringLiteral("ne"));
        QCOMPARE(selectedPreview.value(QStringLiteral("x")).toDouble(), 120.0);
        QCOMPARE(selectedPreview.value(QStringLiteral("y")).toDouble(), -10.0);
        QCOMPARE(otherPreview.value(QStringLiteral("x")).toDouble(), 100.0);
        QCOMPARE(otherPreview.value(QStringLiteral("y")).toDouble(), 0.0);
        QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0), 0.0);
        QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 0), 0.0);

        QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag", Q_ARG(QVariant, false)));
        QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0), 0.0);
        QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 1), 0.0);
        QCOMPARE(perspectiveState->property("activePerspectiveBoxIndex").toInt(), -1);

        QVERIFY(QMetaObject::invokeMethod(window, "beginPerspectiveDrag",
            Q_ARG(QVariant, QVariant::fromValue(selectedDelegate)),
            Q_ARG(QVariant, QStringLiteral("ne")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag", Q_ARG(QVariant, 20.0), Q_ARG(QVariant, -10.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag", Q_ARG(QVariant, true)));

        QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 0), 20.0);
        QCOMPARE(pointValue(editor.boxes().at(0).toMap(), QStringLiteral("ne"), 1), -10.0);
        QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 0), 0.0);
        QCOMPARE(pointValue(editor.boxes().at(1).toMap(), QStringLiteral("ne"), 1), 0.0);
    }

    void qmlPerspectiveMidpointDragConstrainsAxisAndCommitsAdjacentCorners()
    {
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
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        const auto nearlyEqual = [](double a, double b) { return std::abs(a - b) <= 0.001; };
        const auto pointValue = [](const QVariantMap& box, const QString& name, int axis) {
            return box.value(QStringLiteral("perspective") + name.left(1).toUpper() + name.mid(1)).toList().at(axis).toDouble();
        };
        const auto collectDelegates = [](auto&& self, QQuickItem* item, QList<QObject*>& results) -> void {
            if (!item) return;
            if (item->objectName() == QStringLiteral("textBoxDelegate")) results.append(item);
            for (QQuickItem* child : item->childItems()) self(self, child, results);
        };

        QList<QObject*> delegates;
        collectDelegates(collectDelegates, window->contentItem(), delegates);
        QTRY_COMPARE(delegates.size(), 1);
        QObject* delegate = delegates.constFirst();

        QVERIFY(QMetaObject::invokeMethod(window, "beginPerspectiveDrag",
            Q_ARG(QVariant, QVariant::fromValue(delegate)),
            Q_ARG(QVariant, QStringLiteral("n")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag", Q_ARG(QVariant, 20.0), Q_ARG(QVariant, -10.0)));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 15.0));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveY").toDouble(), -8.5));
        QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag", Q_ARG(QVariant, true)));

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

        QVERIFY(QMetaObject::invokeMethod(window, "beginPerspectiveDrag",
            Q_ARG(QVariant, QVariant::fromValue(delegate)),
            Q_ARG(QVariant, QStringLiteral("e")),
            Q_ARG(QVariant, 0.0),
            Q_ARG(QVariant, 0.0)));
        QVERIFY(QMetaObject::invokeMethod(window, "updatePerspectiveDrag", Q_ARG(QVariant, 20.0), Q_ARG(QVariant, -10.0)));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveX").toDouble(), 45.0));
        QTRY_VERIFY(nearlyEqual(window->property("perspectiveY").toDouble(), -2.5));
        QVERIFY(QMetaObject::invokeMethod(window, "endPerspectiveDrag", Q_ARG(QVariant, true)));

        box = editor.boxes().at(0).toMap();
        QCOMPARE(pointValue(box, QStringLiteral("ne"), 0), 40.0);
        QCOMPARE(pointValue(box, QStringLiteral("ne"), 1), -8.0);
        QCOMPARE(pointValue(box, QStringLiteral("se"), 0), 50.0);
        QCOMPARE(pointValue(box, QStringLiteral("se"), 1), 3.0);
        QCOMPARE(pointValue(box, QStringLiteral("nw"), 0), 10.0);
        QCOMPARE(pointValue(box, QStringLiteral("sw"), 0), 40.0);
    }

    void qmlPathHandleInteractionStatePreviewsAndResetsForPerspectivePlane()
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

        auto* state = window->findChild<QObject*>(QStringLiteral("pathHandleInteractionState"));
        QVERIFY(state);

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("pathHandle")));
        auto* handle = qobject_cast<QQuickItem*>(object);
        QVERIFY(handle);
        QTRY_VERIFY(handle->isVisible());

        const QVariantList before = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        const QPoint start = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
        const QPoint preview = start + QPoint(18, 0);

        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTRY_VERIFY(state->property("pathHandleInteractionActive").toBool());
        QCOMPARE(state->property("activePathHandleIndex").toInt(), 0);
        QVERIFY(state->property("activePathHandlePerspective").toBool());

        QTest::mouseMove(window, preview);
        QCoreApplication::processEvents();
        const QVariantList during = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QVERIFY(during != before);
        QVERIFY(during.at(0).toList().at(0).toDouble() > before.at(0).toList().at(0).toDouble());

        QVERIFY(QMetaObject::invokeMethod(window, "endPathHandleDrag"));
        QTRY_VERIFY(!state->property("pathHandleInteractionActive").toBool());
        QCOMPARE(state->property("activePathHandleIndex").toInt(), -1);

        QTest::mouseMove(window, preview + QPoint(18, 0));
        QCoreApplication::processEvents();
        const QVariantList afterReleaseMove = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList();
        QCOMPARE(afterReleaseMove, during);
    }

    void qmlCtrlSpaceIgnoresFocusedSidePanelTextInputs()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("sidePanelFocusedTextInputs")));
        QVERIFY(source.contains(QStringLiteral("sidePanelTextInputFocused: window.sidePanelFocusedTextInputs > 0")));
        QVERIFY(source.contains(QStringLiteral("enabled: editorChrome.editor && !editorChrome.editor.editingText && !editorChrome.sidePanelTextInputFocused")));
        QVERIFY(source.contains(QStringLiteral("onActiveFocusChanged: leftInspectorPanel.textInputFocusChanged(activeFocus)")));
    }

    void qmlEscapeCancelsCurrentContextInOrder()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        const qsizetype handlerStart = source.indexOf(QStringLiteral("function handleEscape()"));
        const qsizetype handlerEnd = source.indexOf(QStringLiteral("menuBar: chrome.menuBar"), handlerStart);
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
        QVERIFY(source.contains(QStringLiteral("Shortcut { sequence: \"Esc\"; context: Qt.ApplicationShortcut; onActivated: editorChrome.escapeRequested() }")));
        QVERIFY(source.contains(QStringLiteral("onEscapeRequested: window.handleEscape()")));
        QVERIFY(source.contains(QStringLiteral("if (event.key === Qt.Key_Escape) { rootWindow.handleEscape(); event.accepted = true }")));
        QVERIFY(!handler.contains(QStringLiteral("Editor.deleteSelected")));
    }

    void qmlColorControlsUseNativeColorDialog()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
        const QString colorButtonSource = readQmlFile(QStringLiteral("ColorButton.qml"));
        const QString editorChromeSource = readQmlFile(QStringLiteral("EditorChrome.qml"));
        const QString source = qmlSource();

        QVERIFY(editorChromeSource.contains(QStringLiteral("import QtQuick.Dialogs")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ColorDialog")));
        QVERIFY(colorButtonSource.contains(QStringLiteral("color: colorButton.enabled ? colorButton.swatchColor : colorButton.palette.mid")));
        QVERIFY(colorButtonSource.contains(QStringLiteral("Label { text: colorButton.swatchText; enabled: colorButton.enabled; Layout.fillWidth: true }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("selectedColor")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("dialogColorHex(selectedColor)")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("editor.setSelectedTextColor(hex)")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("editor.setSelectedOutlineColor(hex)")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("editor.setSelectedShadowColor(hex)")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("editor.setSelectedGradientColorA(hex)")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("editor.setSelectedGradientColorB(hex)")));
        QVERIFY(!mainSource.contains(QStringLiteral("ColorDialog {")));
        QVERIFY(!mainSource.contains(QStringLiteral("dialogColorHex(selectedColor)")));
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
        const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
        const QString canvasShellSource = readQmlFile(QStringLiteral("CentralCanvasShell.qml"));
        const QString leftPanelSource = readQmlFile(QStringLiteral("LeftInspectorPanel.qml"));
        const QString rightPanelSource = readQmlFile(QStringLiteral("RightInspectorPanel.qml"));
        const QString source = qmlSource();

        for (const QString& removedChromeColor : {
                 QStringLiteral("#202124"), QStringLiteral("#24282f"), QStringLiteral("#111318"),
                 QStringLiteral("#c9d1d9"), QStringLiteral("#8b949e"), QStringLiteral("#30363d"),
                 QStringLiteral("#58a6ff"), QStringLiteral("#58a6ff22")}) {
            QVERIFY2(!source.contains(removedChromeColor), qPrintable(removedChromeColor));
        }

        QVERIFY(source.contains(QStringLiteral("color: window.palette.window")));
        QVERIFY(source.contains(QStringLiteral("color: canvasShell.hostPalette.base")));
        QVERIFY(source.contains(QStringLiteral("window.palette.highlight")));
        QVERIFY(source.contains(QStringLiteral("hostPalette: window.palette")));
        QVERIFY(source.contains(QStringLiteral("editorChrome.hostPalette.highlight")));
        QVERIFY(source.contains(QStringLiteral("editorChrome.hostPalette.highlightedText")));
        QVERIFY(source.contains(QStringLiteral("SplitView {")));
        QVERIFY(source.contains(QStringLiteral("orientation: Qt.Horizontal")));
        QVERIFY(source.contains(QStringLiteral("SplitView.minimumWidth: 240")));
        QVERIFY(source.contains(QStringLiteral("SplitView.preferredWidth: 280")));
        QVERIFY(source.contains(QStringLiteral("SplitView.fillWidth: true")));
        QVERIFY(mainSource.contains(QStringLiteral("LeftInspectorPanel {")));
        QVERIFY(mainSource.contains(QStringLiteral("objectName: \"leftInspectorPanel\"")));
        QVERIFY(source.contains(QStringLiteral("id: sidePanel")));
        QVERIFY(mainSource.contains(QStringLiteral("CentralCanvasShell {")));
        QVERIFY(mainSource.contains(QStringLiteral("objectName: \"centralCanvasShell\"")));
        QVERIFY(source.contains(QStringLiteral("id: canvas")));
        QVERIFY(canvasShellSource.contains(QStringLiteral("objectName: \"centralCanvas\"")));
        QVERIFY(source.contains(QStringLiteral("id: rightPanel")));
        QVERIFY(mainSource.contains(QStringLiteral("RightInspectorPanel {")));
        QVERIFY(mainSource.contains(QStringLiteral("objectName: \"rightInspectorPanel\"")));
        QVERIFY(canvasShellSource.contains(QStringLiteral("x: -canvasShell.x")));
        QVERIFY(canvasShellSource.contains(QStringLiteral("width: canvasShell.SplitView.view ? canvasShell.SplitView.view.width : canvasShell.width")));

        const qsizetype sidePanelStart = mainSource.indexOf(QStringLiteral("id: sidePanel"));
        const qsizetype canvasShellStart = mainSource.indexOf(QStringLiteral("id: canvasShell"), sidePanelStart);
        const qsizetype canvasStart = canvasShellSource.indexOf(QStringLiteral("id: canvas\n"));
        QVERIFY(sidePanelStart >= 0);
        QVERIFY(canvasShellStart > sidePanelStart);
        QVERIFY(canvasStart >= 0);
        const qsizetype rightPanelStart = mainSource.indexOf(QStringLiteral("id: rightPanel"), canvasShellStart);
        QVERIFY(rightPanelStart > canvasShellStart);
        QVERIFY(leftPanelSource.contains(QStringLiteral("z: 1")));
        QVERIFY(mainSource.mid(canvasShellStart, rightPanelStart - canvasShellStart).contains(QStringLiteral("id: canvasShell")));
        QVERIFY(canvasShellSource.mid(0, canvasStart).contains(QStringLiteral("z: 0")));
        QVERIFY(canvasShellSource.mid(canvasStart, canvasShellSource.indexOf(QStringLiteral("x: -canvasShell.x"), canvasStart) - canvasStart).contains(QStringLiteral("z: 0")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("z: 1")));
        const QString sidePanelSource = leftPanelSource;
        QVERIFY(leftPanelSource.contains(QStringLiteral("Pane {")));
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
        QVERIFY(sidePanelSource.contains(QStringLiteral("RowLayout {\n                            Layout.fillWidth: true\n                            Layout.minimumWidth: 0\n                            spacing: 8")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("SpinBox { objectName: \"leftInspectorFontSizeSpinBox\"; Layout.fillWidth: true; Layout.minimumWidth: 0; from: leftInspectorPanel.editorLimits.minimumFontSize; to: leftInspectorPanel.editorLimits.maximumFontSize")));
        QVERIFY(sidePanelSource.contains(QStringLiteral("SpinBox { objectName: \"leftInspectorLineSpacingSpinBox\"; Layout.fillWidth: true; Layout.minimumWidth: 0; from: leftInspectorPanel.editorLimits.minimumTextSpacing; to: leftInspectorPanel.editorLimits.maximumTextSpacing")));
        QVERIFY(sidePanelSource.count(QStringLiteral("Flow {\n                            Layout.fillWidth: true\n                            Layout.minimumWidth: 0")) >= 3);
        QVERIFY(rightPanelSource.contains(QStringLiteral("Pane {")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("z: 1")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("SplitView.minimumWidth: 180")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("id: rightPanelScroll")));
        QVERIFY(rightPanelSource.contains(QStringLiteral("width: rightPanelScroll.availableWidth")));
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
        QVERIFY(source.contains(QStringLiteral("ViewportMetrics {")));
        QVERIFY(source.contains(QStringLiteral("objectName: \"viewportMetrics\"")));
        QVERIFY(source.contains(QStringLiteral("function fitPageScale() { return viewportMetrics.fitPageScale() }")));
        QVERIFY(source.contains(QStringLiteral("function pageScale() { return viewportMetrics.pageScale() }")));
        QVERIFY(source.contains(QStringLiteral("onStatusChanged: if (status === Image.Ready) window.pageBaseScale = window.fitPageScale()")));

        const qsizetype pageScaleStart = source.indexOf(QStringLiteral("function pageScale() { return pageBaseScale }"));
        const qsizetype pageScaleEnd = source.indexOf(QStringLiteral("function pageDisplayWidth()"), pageScaleStart);
        QVERIFY(pageScaleStart >= 0);
        QVERIFY(pageScaleEnd > pageScaleStart);
        const QString pageScaleSource = source.mid(pageScaleStart, pageScaleEnd - pageScaleStart);
        QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.width")));
        QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.height")));
        QVERIFY(source.contains(QStringLiteral("function pageDisplayWidth() { return pageSourceWidth * pageScale() }")));
        QVERIFY(source.contains(QStringLiteral("function pageDisplayHeight() { return pageSourceHeight * pageScale() }")));
    }

    void qmlRightPanelUsesSectionTabsAndNoHorizontalOverflow()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
        const QString source = qmlSource();
        const QString rightPanelSource = readQmlFile(QStringLiteral("RightInspectorPanel.qml"));
        const qsizetype rightPanelStart = source.indexOf(QStringLiteral("id: rightPanel"));
        QVERIFY(rightPanelStart >= 0);
        QVERIFY(mainSource.contains(QStringLiteral("RightInspectorPanel {")));
        QVERIFY(mainSource.contains(QStringLiteral("selectedBoxProvider: () => window.selectedBox()")));
        QVERIFY(mainSource.contains(QStringLiteral("qmlColorProvider: hex => window.qmlColor(hex)")));
        QVERIFY(mainSource.contains(QStringLiteral("onColorDialogRequested: (hex, setter) => window.openColorDialog(hex, setter)")));
        QVERIFY(!mainSource.contains(QStringLiteral("Label { text: qsTr(\"Navigation\")")));
        QVERIFY(!mainSource.contains(QStringLiteral("Label { text: qsTr(\"Text Effects\")")));

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
        QVERIFY(boxEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedRotation(value)")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedPerspectiveEnabled(checked)")));
        QVERIFY(boxEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.resetSelectedPerspective()")));

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
        QVERIFY(textEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedOutlineEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedBlurEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedShadowEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedGradientEnabled(checked)")));
        QVERIFY(textEffectsSource.contains(QStringLiteral("rightInspectorPanel.editor.setSelectedPathEnabled(checked)")));
    }

    void qmlMenusOwnPrimaryControlsAndToolbarIsRemoved()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
        const QString editorChromeSource = readQmlFile(QStringLiteral("EditorChrome.qml"));
        const QString source = qmlSource();
        const QString menuItemSource = readQmlFile(QStringLiteral("ShortcutMenuItem.qml"));
        const qsizetype menuItemStart = menuItemSource.indexOf(QStringLiteral("MenuItem {"));
        const qsizetype menuItemEnd = menuItemSource.size();
        QVERIFY(menuItemStart >= 0);
        QVERIFY(menuItemEnd > menuItemStart);
        const QString menuItemDefinition = menuItemSource.mid(menuItemStart, menuItemEnd - menuItemStart);

        QVERIFY(!mainSource.contains(QStringLiteral("component ShortcutMenuItem: MenuItem")));
        QVERIFY(menuItemDefinition.contains(QStringLiteral("MenuItem {")));
        QVERIFY(menuItemDefinition.contains(QStringLiteral("contentItem: Item")));
        QVERIFY(menuItemDefinition.contains(QStringLiteral("RowLayout {")));
        QVERIFY(menuItemDefinition.contains(QStringLiteral("Layout.fillWidth: true")));
        QVERIFY(menuItemDefinition.contains(QStringLiteral("shortcutMenuItem.shortcutLabel")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("indicator:")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("shortcutMenuItem.checked")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("✓")));
        QVERIFY(!menuItemSource.contains(QStringLiteral("Layout.preferredWidth: 16")));
        QVERIFY(!source.contains(QStringLiteral("Layout.preferredWidth: shortcutMenuItem.checkable ? 16 : 0")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: openAction; text: qsTr(\"Open\"); shortcut: StandardKey.Open")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: saveAction; text: qsTr(\"Save\"); shortcut: StandardKey.Save")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: saveAllAction; text: qsTr(\"Save All\"); shortcut: \"Ctrl+Shift+S\"")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: quitAction; text: qsTr(\"Quit\"); onTriggered: Qt.quit() }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: copyAction; text: qsTr(\"Copy\"); shortcut: StandardKey.Copy")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: pasteAction; text: qsTr(\"Paste\"); shortcut: StandardKey.Paste")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: deleteAction; text: qsTr(\"Delete\"); shortcut: StandardKey.Delete")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: duplicateAction; text: qsTr(\"Duplicate\"); enabled:")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: zoomInAction; text: qsTr(\"Zoom In\"); shortcut: \"Ctrl++\"")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: zoomOutAction; text: qsTr(\"Zoom Out\"); shortcut: \"Ctrl+-\"")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: resetZoomAction; text: qsTr(\"Reset Zoom\"); shortcut: \"Ctrl+0\"")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("Action { id: rawOverlayAction; text: qsTr(\"Show Raw Overlay\"); checkable: true; checked: editorChrome.editor ? editorChrome.editor.rawVisible : false; shortcut: \"Ctrl+H\"")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: openAction; shortcutLabel: \"Ctrl+O\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: saveAction; shortcutLabel: \"Ctrl+S\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: saveAllAction; shortcutLabel: \"Ctrl+Shift+S\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: quitAction }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: copyAction; shortcutLabel: \"Ctrl+C\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: pasteAction; shortcutLabel: \"Ctrl+V\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: deleteAction; shortcutLabel: \"Del\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: zoomInAction; shortcutLabel: \"Ctrl++\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: zoomOutAction; shortcutLabel: \"Ctrl+-\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: resetZoomAction; shortcutLabel: \"Ctrl+0\" }")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("ShortcutMenuItem { action: rawOverlayAction; shortcutLabel: \"Ctrl+H\" }")));

        QVERIFY(mainSource.contains(QStringLiteral("menuBar: chrome.menuBar")));
        QVERIFY(editorChromeSource.contains(QStringLiteral("property alias menuBar: chromeMenuBar")));
        QVERIFY(mainSource.contains(QStringLiteral("EditorChrome {")));
        QVERIFY(!mainSource.contains(QStringLiteral("Action { id: openAction")));
        QVERIFY(!mainSource.contains(QStringLiteral("FolderDialog")));
        QVERIFY(!mainSource.contains(QStringLiteral("ColorDialog {")));
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
        QVERIFY(editorChromeSource.contains(QStringLiteral("editorChrome.editor.openProjectUrl(selectedFolder)")));
    }

    void qmlMainWiresEditorChromeSignalsAndBindings()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);

        auto* chrome = window->findChild<QObject*>(QStringLiteral("editorChrome"));
        QVERIFY(chrome);
        QCOMPARE(chrome->property("editor").value<QObject*>(), &editor);
        QCOMPARE(chrome->property("hostWidth").toReal(), window->width());
        QCOMPARE(chrome->property("hostHeight").toReal(), window->height());
        QVERIFY(chrome->property("zoomCenterX").toReal() > 0.0);
        QVERIFY(chrome->property("zoomCenterY").toReal() >= 0.0);

        QVERIFY(window->setProperty("zoom", 2.0));
        QVERIFY(QMetaObject::invokeMethod(chrome, "resetZoomRequested"));
        QTRY_COMPARE(window->property("zoom").toReal(), 1.0);

        QVERIFY(QMetaObject::invokeMethod(
            chrome, "zoomAtRequested", Q_ARG(double, 320.0), Q_ARG(double, 240.0), Q_ARG(double, 1.1)));
        QTRY_COMPARE(window->property("zoom").toReal(), 1.1);

        QCOMPARE(editor.selectedIndex(), 0);
        QVERIFY(QMetaObject::invokeMethod(chrome, "escapeRequested"));
        QTRY_COMPARE(editor.selectedIndex(), -1);
    }

    void qmlMainWiresLeftInspectorPanelApi()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedFontFamily(QStringLiteral("TextFX Inspector Test"));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);

        auto* leftInspector = window->findChild<QObject*>(QStringLiteral("leftInspectorPanel"));
        QVERIFY(leftInspector);
        QCOMPARE(leftInspector->property("editor").value<QObject*>(), &editor);
        QVERIFY(leftInspector->property("editorLimits").value<QObject*>());

        QVariant selected;
        QVERIFY(QMetaObject::invokeMethod(leftInspector, "selectedBox", Q_RETURN_ARG(QVariant, selected)));
        QCOMPARE(selected.toMap().value(QStringLiteral("fontFamily")).toString(), QStringLiteral("TextFX Inspector Test"));

        QCOMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(leftInspector, "textInputFocusChanged", Q_ARG(bool, true)));
        QTRY_COMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(leftInspector, "textInputFocusChanged", Q_ARG(bool, false)));
        QTRY_COMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 0);
    }

    void qmlLeftInspectorTextPropertyControlUpdatesSelectedBox()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedBold(false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("leftInspectorBoldButton")));
        auto* button = qobject_cast<QQuickItem*>(object);
        QVERIFY(button);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(button->isEnabled());

        const QPoint center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

        QTRY_VERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("bold")).toBool());
    }

    void qmlLeftInspectorPageTextDelegateAppliesThroughEditor()
    {
        registerQmlTypes();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        QFile texts(dir.filePath(QStringLiteral("texts.txt")));
        QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
        texts.write("[page1.png]\nPanel page text\n");
        texts.close();

        EditorController editor;
        editor.openProject(dir.path());
        QCOMPARE(editor.pageTexts(), QStringList({QStringLiteral("Panel page text")}));
        editor.createTextBox(10, 20, 100, 50);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("leftInspectorPageTextDelegate")));
        auto* delegate = qobject_cast<QQuickItem*>(object);
        QVERIFY(delegate);
        QTRY_VERIFY(delegate->isVisible());
        QVERIFY(delegate->isEnabled());

        const QPoint center = delegate->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

        QTRY_COMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Panel page text"));
    }

    void qmlLeftInspectorColorButtonRoutesThroughEditorChrome()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedTextColor(QStringLiteral("#112233"));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto* chrome = window->findChild<QObject*>(QStringLiteral("editorChrome"));
        QVERIFY(chrome);
        QCOMPARE(chrome->property("colorDialogSetter").toString(), QString());

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("leftInspectorTextColorButton")));
        auto* button = qobject_cast<QQuickItem*>(object);
        QVERIFY(button);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(button->isEnabled());

        const QPoint center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

        QTRY_COMPARE(chrome->property("colorDialogSetter").toString(), QStringLiteral("text"));
    }

    void qmlMainWiresRightInspectorPanelApi()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedOutlineColor(QStringLiteral("#112233"));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);

        auto* rightInspector = window->findChild<QObject*>(QStringLiteral("rightInspectorPanel"));
        QVERIFY(rightInspector);
        QCOMPARE(rightInspector->property("editor").value<QObject*>(), &editor);
        QVERIFY(rightInspector->property("editorLimits").value<QObject*>());

        QVariant selected;
        QVERIFY(QMetaObject::invokeMethod(rightInspector, "selectedBox", Q_RETURN_ARG(QVariant, selected)));
        QCOMPARE(selected.toMap().value(QStringLiteral("outlineColor")).toString(), QStringLiteral("112233ff"));
    }

    void qmlRightInspectorNavigationAndEffectsRouteThroughEditor()
    {
        registerQmlTypes();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("page1.png")));
        touch(dir.filePath(QStringLiteral("page2.png")));

        EditorController editor;
        editor.openProject(dir.path());
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedOutlineEnabled(false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorNextPageButton")));
        auto* nextButton = qobject_cast<QQuickItem*>(object);
        QVERIFY(nextButton);
        QTRY_VERIFY(nextButton->isVisible());
        QVERIFY(nextButton->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, nextButton->mapToScene(QPointF(nextButton->width() / 2, nextButton->height() / 2)).toPoint());
        QTRY_COMPARE(editor.currentPageIndex(), 1);

        editor.previousPage();
        QTRY_COMPARE(editor.currentPageIndex(), 0);
        editor.selectBox(0);
        QTRY_COMPARE(editor.selectedIndex(), 0);

        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorRawVisibleCheckBox")));
        auto* rawCheckBox = qobject_cast<QQuickItem*>(object);
        QVERIFY(rawCheckBox);
        QTRY_VERIFY(rawCheckBox->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, rawCheckBox->mapToScene(QPointF(rawCheckBox->width() / 2, rawCheckBox->height() / 2)).toPoint());
        QTRY_VERIFY(editor.rawVisible());

        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorOutlineEnabledCheckBox")));
        auto* outlineCheckBox = qobject_cast<QQuickItem*>(object);
        QVERIFY(outlineCheckBox);
        QTRY_VERIFY(outlineCheckBox->isVisible());
        QVERIFY(outlineCheckBox->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, outlineCheckBox->mapToScene(QPointF(outlineCheckBox->width() / 2, outlineCheckBox->height() / 2)).toPoint());
        QTRY_VERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("outline")).toBool());
    }

    void qmlRightInspectorColorButtonRoutesThroughEditorChrome()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setSelectedOutlineColor(QStringLiteral("#112233"));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto* chrome = window->findChild<QObject*>(QStringLiteral("editorChrome"));
        QVERIFY(chrome);
        QCOMPARE(chrome->property("colorDialogSetter").toString(), QString());

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorOutlineColorButton")));
        auto* button = qobject_cast<QQuickItem*>(object);
        QVERIFY(button);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(button->isEnabled());

        const QPoint center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

        QTRY_COMPARE(chrome->property("colorDialogSetter").toString(), QStringLiteral("outline"));
    }

    void qmlRightInspectorPathAndPerspectiveControlsRouteThroughEditor()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.setPerspectiveHandle(QStringLiteral("nw"), 12.0, 8.0);
        editor.setSelectedPathEnabled(false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->resize(1400, 1000);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        const auto click = [&](const QString& objectName) {
            QObject* object = nullptr;
            QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), objectName));
            auto* item = qobject_cast<QQuickItem*>(object);
            QVERIFY(item);
            QTRY_VERIFY(item->isVisible());
            QVERIFY(item->isEnabled());
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        };

        QObject* tabsObject = nullptr;
        QTRY_VERIFY(tabsObject = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorTextEffectsTabs")));
        QVERIFY(tabsObject->setProperty("currentIndex", 4));
        QCoreApplication::processEvents();

        click(QStringLiteral("rightInspectorPathEnabledCheckBox"));
        QTRY_VERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("path")).toBool());
        const int pathPointCount = editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().size();

        click(QStringLiteral("rightInspectorAddPathPointButton"));
        QTRY_VERIFY(editor.boxes().at(0).toMap().value(QStringLiteral("pathPoints")).toList().size() > pathPointCount);

        QTRY_VERIFY(tabsObject = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorBoxEffectsTabs")));
        QVERIFY(tabsObject->setProperty("currentIndex", 1));
        QCoreApplication::processEvents();

        click(QStringLiteral("rightInspectorResetPerspectiveButton"));
        const QVariantList resetNw = editor.boxes().at(0).toMap().value(QStringLiteral("perspectiveNw")).toList();
        QCOMPARE(resetNw.at(0).toDouble(), 0.0);
        QCOMPARE(resetNw.at(1).toDouble(), 0.0);
    }

    void qmlRightInspectorLayersListSelectsAndMovesThroughEditor()
    {
        registerQmlTypes();

        EditorController editor;
        editor.newDocument();
        editor.createTextBox(10, 20, 100, 50);
        editor.updateSelectedText(QStringLiteral("Bottom"));
        editor.createTextBox(30, 40, 100, 50);
        editor.updateSelectedText(QStringLiteral("Middle"));
        editor.createTextBox(50, 60, 100, 50);
        editor.updateSelectedText(QStringLiteral("Top"));
        editor.selectBox(0);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
        engine.load(QUrl::fromLocalFile(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->resize(1400, 1000);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject* object = nullptr;
        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorLayersListView")));
        auto* listView = qobject_cast<QQuickItem*>(object);
        QVERIFY(listView);
        QTRY_VERIFY(listView->isVisible());
        QVERIFY(listView->isEnabled());

        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorLayerDelegate")));
        auto* delegate = qobject_cast<QQuickItem*>(object);
        QVERIFY(delegate);
        QTRY_VERIFY(delegate->isVisible());
        QVERIFY(delegate->isEnabled());

        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, delegate->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2)).toPoint());
        QTRY_COMPARE(editor.selectedIndex(), 2);

        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorLayerDownButton")));
        auto* downButton = qobject_cast<QQuickItem*>(object);
        QVERIFY(downButton);
        QTRY_VERIFY(downButton->isVisible());
        QVERIFY(downButton->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, downButton->mapToScene(QPointF(downButton->width() / 2, downButton->height() / 2)).toPoint());
        QTRY_COMPARE(editor.selectedIndex(), 1);
        QCOMPARE(editor.boxes().at(1).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Top"));

        QTRY_VERIFY(object = findVisualChildByName(window->contentItem(), QStringLiteral("rightInspectorLayerUpButton")));
        auto* upButton = qobject_cast<QQuickItem*>(object);
        QVERIFY(upButton);
        QTRY_VERIFY(upButton->isVisible());
        QVERIFY(upButton->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, upButton->mapToScene(QPointF(upButton->width() / 2, upButton->height() / 2)).toPoint());
        QTRY_COMPARE(editor.selectedIndex(), 2);
        QCOMPARE(editor.boxes().at(2).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Top"));
    }

    void qmlHasPageSelectorAndTextFXEffectControls()
    {
        QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = qmlSource();

        QVERIFY(source.contains(QStringLiteral("model: rightInspectorPanel.editor.pageLabels")));
        QVERIFY(source.contains(QStringLiteral("rightInspectorPanel.editor.goToPage(index)")));
        QVERIFY(source.contains(QStringLiteral("model: [qsTr(\"Vertical\"), qsTr(\"Horizontal\")]")));
        QVERIFY(source.contains(QStringLiteral("model: [qsTr(\"Straight\"), qsTr(\"Smooth\")]")));
        QVERIFY(source.contains(QStringLiteral("rightInspectorPanel.editor.addSelectedPathPoint()")));
        QVERIFY(source.contains(QStringLiteral("PathHandleInteractionState")));
        QVERIFY(source.contains(QStringLiteral("objectName: \"pathHandleInteractionState\"")));
        QVERIFY(source.contains(QStringLiteral("pathHandleInteraction.update(canvas, canvasX, canvasY, window.editor.leftMouseButtonDown())")));
        QVERIFY(source.contains(QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints")));
        QVERIFY(!source.contains(QStringLiteral("Double-click canvas to create text")));
        QVERIFY(!source.contains(QStringLiteral("Live editing uses QML layout; rendered effects apply on export.")));
    }
};

QTEST_MAIN(QmlShellSmokeTests)

#include "qml_shell_smoke_tests.moc"
