import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TextFX.Ui 1.0
import "../features/leftPanel"
import "../features/rightPanel"
import "../features/canvas"
import "../features/canvas/interactions"
import "../features/canvas/geometry"

ApplicationWindow {
    id: window

    readonly property var editor: Editor
    property alias canvas: canvasView.canvasItem
    property real zoom: 1
    property real pageBaseScale: 1
    property real panX: 0
    property real panY: 0
    property alias pressX: canvasInteraction.pressX
    property alias pressY: canvasInteraction.pressY
    property alias pointerX: canvasInteraction.pointerX
    property alias pointerY: canvasInteraction.pointerY
    property alias dragMode: canvasInteraction.dragMode
    property alias createStartX: canvasInteraction.createStartX
    property alias createStartY: canvasInteraction.createStartY
    property alias createCurrentX: canvasInteraction.createCurrentX
    property alias createCurrentY: canvasInteraction.createCurrentY
    property int sidePanelFocusedTextInputs: 0
    property alias activeResizeDelegate: boxResizeInteraction.activeResizeDelegate
    property alias activeResizeHandle: boxResizeInteraction.activeResizeHandle
    property alias resizeStartX: boxResizeInteraction.resizeStartX
    property alias resizeStartY: boxResizeInteraction.resizeStartY
    property alias resizeStartW: boxResizeInteraction.resizeStartW
    property alias resizeStartH: boxResizeInteraction.resizeStartH
    property alias resizeStartRotation: boxResizeInteraction.resizeStartRotation
    property alias resizeStartCanvasX: boxResizeInteraction.resizeStartCanvasX
    property alias resizeStartCanvasY: boxResizeInteraction.resizeStartCanvasY
    property alias resizeX: boxResizeInteraction.resizeX
    property alias resizeY: boxResizeInteraction.resizeY
    property alias resizeW: boxResizeInteraction.resizeW
    property alias resizeH: boxResizeInteraction.resizeH
    property alias activePerspectiveDelegate: perspectiveInteraction.activePerspectiveDelegate
    property alias activePerspectiveHandle: perspectiveInteraction.activePerspectiveHandle
    property alias perspectiveStartCanvasX: perspectiveInteraction.perspectiveStartCanvasX
    property alias perspectiveStartCanvasY: perspectiveInteraction.perspectiveStartCanvasY
    property alias perspectiveStartX: perspectiveInteraction.perspectiveStartX
    property alias perspectiveStartY: perspectiveInteraction.perspectiveStartY
    property alias perspectiveX: perspectiveInteraction.perspectiveX
    property alias perspectiveY: perspectiveInteraction.perspectiveY
    property alias perspectiveRevision: perspectiveInteraction.perspectiveRevision
    property alias perspectiveStartNwX: perspectiveInteraction.perspectiveStartNwX
    property alias perspectiveStartNwY: perspectiveInteraction.perspectiveStartNwY
    property alias perspectiveStartNeX: perspectiveInteraction.perspectiveStartNeX
    property alias perspectiveStartNeY: perspectiveInteraction.perspectiveStartNeY
    property alias perspectiveStartSeX: perspectiveInteraction.perspectiveStartSeX
    property alias perspectiveStartSeY: perspectiveInteraction.perspectiveStartSeY
    property alias perspectiveStartSwX: perspectiveInteraction.perspectiveStartSwX
    property alias perspectiveStartSwY: perspectiveInteraction.perspectiveStartSwY
    property alias activeRotateDelegate: boxRotateInteraction.activeRotateDelegate
    property alias rotateStartRotation: boxRotateInteraction.rotateStartRotation
    property alias rotateStartAngle: boxRotateInteraction.rotateStartAngle
    property alias rotateDegrees: boxRotateInteraction.rotateDegrees
    property alias activeMoveDelegate: boxMoveInteraction.activeMoveDelegate
    property alias activeMoveIndex: boxMoveInteraction.activeMoveIndex
    property alias moveStartX: boxMoveInteraction.moveStartX
    property alias moveStartY: boxMoveInteraction.moveStartY
    property alias moveStartCanvasX: boxMoveInteraction.moveStartCanvasX
    property alias moveStartCanvasY: boxMoveInteraction.moveStartCanvasY
    property alias moveX: boxMoveInteraction.moveX
    property alias moveY: boxMoveInteraction.moveY
    property alias activePathHandleIndex: pathHandleInteraction.activePathHandleIndex
    property alias activePathHandlePlane: pathHandleInteraction.activePathHandlePlane
    property alias activePathHandlePerspective: pathHandleInteraction.activePathHandlePerspective
    property alias pathHandleInteractionActive: pathHandleInteraction.pathHandleInteractionActive
    property alias pathHandleDragPressed: pathHandleInteraction.pathHandleDragPressed
    property alias activePathBoxWidth: pathHandleInteraction.activePathBoxWidth
    property alias activePathBoxHeight: pathHandleInteraction.activePathBoxHeight
    property alias activePathBoxRotation: pathHandleInteraction.activePathBoxRotation
    property alias pathHandlePressCanvasX: pathHandleInteraction.pathHandlePressCanvasX
    property alias pathHandlePressCanvasY: pathHandleInteraction.pathHandlePressCanvasY
    property alias pathHandlePressLocalX: pathHandleInteraction.pathHandlePressLocalX
    property alias pathHandlePressLocalY: pathHandleInteraction.pathHandlePressLocalY
    property alias pathHandleStartX: pathHandleInteraction.pathHandleStartX
    property alias pathHandleStartY: pathHandleInteraction.pathHandleStartY
    property var activePaintPoints: []
    property var activePaintPreviewPoints: []
    property string activePaintTarget: ""
    property int activePaintPageIndex: -1
    property int paintPreviewPublishRevision: 0
    readonly property bool pageNavigationEnabled: dragMode !== editorInteraction.dragModePaint

    function fitPageScale() {
        return viewportMetrics.fitPageScale();
    }

    function pageScale() {
        return viewportMetrics.pageScale();
    }

    function pageDisplayWidth() {
        return viewportMetrics.pageDisplayWidth();
    }

    function pageDisplayHeight() {
        return viewportMetrics.pageDisplayHeight();
    }

    function pageLeft() {
        return viewportMetrics.pageLeft();
    }

    function pageTop() {
        return viewportMetrics.pageTop();
    }

    function viewDocScale() {
        return viewportMetrics.viewDocScale();
    }

    function livePreviewScale() {
        return viewportMetrics.livePreviewScale();
    }

    function documentToViewLength(value) {
        return viewportMetrics.documentToViewLength(value);
    }

    function selectionLineWidth() {
        return Math.max(1, documentToViewLength(2));
    }

    function handleSize() {
        return Math.max(1, documentToViewLength(editorLimits.minimumBoxSize));
    }

    function rotateHandleDistance() {
        return documentToViewLength(28);
    }

    function documentToViewX(x) {
        return viewportMetrics.documentToViewX(x);
    }

    function documentToViewY(y) {
        return viewportMetrics.documentToViewY(y);
    }

    function viewToDocumentX(x) {
        return viewportMetrics.viewToDocumentX(x);
    }

    function viewToDocumentY(y) {
        return viewportMetrics.viewToDocumentY(y);
    }

    function zoomAt(x, y, factor) {
        const nextState = viewportMetrics.zoomStateAt(x, y, factor);
        if (!nextState.changed)
            return ;

        window.panX = nextState.panX;
        window.panY = nextState.panY;
        window.zoom = nextState.zoom;
    }

    function resetZoom() {
        const reset = viewportMetrics.resetState();
        window.zoom = reset.zoom;
        window.panX = reset.panX;
        window.panY = reset.panY;
    }

    function setZoomAtCenter(zoom) {
        if (zoom <= 0 || zoom === window.zoom)
            return ;

        window.zoomAt(canvas.width / 2, canvas.height / 2, zoom / window.zoom);
    }

    function setDisplayScaleAtCenter(displayScale) {
        if (displayScale <= 0 || window.pageBaseScale <= 0)
            return ;

        window.setZoomAtCenter(displayScale / window.pageBaseScale);
    }

    function fontFamilyOptions(selected) {
        const options = [];

        function add(value) {
            const family = String(value || "").trim();
            if (family.length > 0 && options.indexOf(family) < 0)
                options.push(family);

        }

        add(selected);
        if (typeof Qt.fontFamilies === "function") {
            const families = Qt.fontFamilies();
            for (let i = 0; i < families.length; ++i) add(families[i])
        }
        add("sans-serif");
        add("serif");
        add("monospace");
        return options;
    }

    function noteSidePanelTextInputFocus(active) {
        sidePanelFocusedTextInputs = Math.max(0, sidePanelFocusedTextInputs + (active ? 1 : -1));
    }

    function qmlColor(hex) {
        const raw = String(hex || "000000ff");
        const value = raw.startsWith("#") ? raw.slice(1) : raw;
        return "#" + value.slice(0, 6);
    }

    function openColorDialog(hex, setter) {
        chrome.openColorDialog(hex, setter);
    }

    function pointValue(point, index, fallback) {
        return perspectiveGeometry.pointValue(point, index, fallback);
    }

    function perspectiveBase(name, axis) {
        return perspectiveGeometry.perspectiveBase(name, axis);
    }

    function modelPerspectiveOffset(box, name, axis) {
        return perspectiveGeometry.modelPerspectiveOffset(box, name, axis);
    }

    function visualHandlePosition(box, name, width, height) {
        return perspectiveGeometry.visualHandlePosition(box, name, width, height);
    }

    function topMiddleVisualPoint(box, width, height) {
        return perspectiveGeometry.topMiddleVisualPoint(box, width, height);
    }

    function rotateHandlePosition(box, width, height) {
        return perspectiveGeometry.rotateHandlePosition(box, width, height);
    }

    function perspectiveCorner(box, name, width, height) {
        return perspectiveGeometry.perspectiveCorner(box, name, width, height);
    }

    function perspectiveVisualBounds(box, width, height) {
        return perspectiveGeometry.perspectiveVisualBounds(box, width, height);
    }

    function pointInPerspectivePolygon(point, box, width, height) {
        return perspectiveGeometry.pointInPerspectivePolygon(point, box, width, height);
    }

    function perspectiveMargin(box) {
        return perspectiveGeometry.perspectiveMargin(box);
    }

    function perspectiveMatrix(box, width, height, renderScale, enabled) {
        return perspectiveGeometry.perspectiveMatrix(box, width, height, renderScale, enabled);
    }

    function beginResizeDrag(delegate, handle, canvasX, canvasY) {
        dragMode = editorInteraction.dragModeResize;
        boxResizeInteraction.begin(delegate, handle, canvasX, canvasY);
    }

    function updateResizeDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeResize || !activeResizeDelegate)
            return ;

        boxResizeInteraction.update(canvasX, canvasY);
        const bounds = boxResizeInteraction.bounds();
        window.editor.setSelectedBounds(bounds.x, bounds.y, bounds.width, bounds.height);
    }

    function endResizeDrag(commit) {
        if (dragMode === editorInteraction.dragModeResize && !commit) {
            window.editor.setSelectedBounds(boxResizeInteraction.resizeStartX, boxResizeInteraction.resizeStartY, boxResizeInteraction.resizeStartW, boxResizeInteraction.resizeStartH);
        }
        boxResizeInteraction.reset();
        dragMode = editorInteraction.dragModeIdle;
    }

    function beginPerspectiveDrag(delegate, handle, canvasX, canvasY) {
        dragMode = editorInteraction.dragModePerspective;
        perspectiveInteraction.begin(delegate, handle, canvasX, canvasY);
    }

    function updatePerspectiveDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModePerspective || !activePerspectiveDelegate)
            return ;

        perspectiveInteraction.update(canvas, canvasX, canvasY);
        applyPerspectiveHandles(perspectiveInteraction.commitHandles());
    }

    function applyPerspectiveHandles(handles) {
        for (let i = 0; i < handles.length; ++i)
            window.editor.setPerspectiveHandle(handles[i].name, handles[i].x, handles[i].y);

    }

    function restorePerspectiveStartHandles() {
        applyPerspectiveHandles([{
            "name": "nw",
            "x": perspectiveInteraction.perspectiveStartNwX,
            "y": perspectiveInteraction.perspectiveStartNwY
        }, {
            "name": "ne",
            "x": perspectiveInteraction.perspectiveStartNeX,
            "y": perspectiveInteraction.perspectiveStartNeY
        }, {
            "name": "se",
            "x": perspectiveInteraction.perspectiveStartSeX,
            "y": perspectiveInteraction.perspectiveStartSeY
        }, {
            "name": "sw",
            "x": perspectiveInteraction.perspectiveStartSwX,
            "y": perspectiveInteraction.perspectiveStartSwY
        }]);
    }

    function endPerspectiveDrag(commit) {
        if (dragMode === editorInteraction.dragModePerspective && !commit)
            restorePerspectiveStartHandles();
        perspectiveInteraction.reset();
        dragMode = editorInteraction.dragModeIdle;
    }

    function beginRotateDrag(delegate, canvasX, canvasY) {
        dragMode = editorInteraction.dragModeRotate;
        boxRotateInteraction.begin(delegate, viewToDocumentX(canvasX), viewToDocumentY(canvasY));
    }

    function updateRotateDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeRotate || !activeRotateDelegate)
            return ;

        boxRotateInteraction.update(viewToDocumentX(canvasX), viewToDocumentY(canvasY));
        window.editor.setSelectedRotation(boxRotateInteraction.rotateDegrees);
    }

    function endRotateDrag(commit) {
        if (dragMode === editorInteraction.dragModeRotate && !commit)
            window.editor.setSelectedRotation(boxRotateInteraction.rotateStartRotation);
        boxRotateInteraction.reset();
        dragMode = editorInteraction.dragModeIdle;
    }

    function beginMoveDrag(delegate, index, canvasX, canvasY) {
        dragMode = editorInteraction.dragModeMove;
        boxMoveInteraction.begin(delegate, index, canvasX, canvasY);
    }

    function updateMoveDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeMove || !activeMoveDelegate)
            return ;

        boxMoveInteraction.update(canvasX, canvasY);
        window.editor.setSelectedBounds(boxMoveInteraction.moveX, boxMoveInteraction.moveY, activeMoveDelegate.boxModel.w, activeMoveDelegate.boxModel.h);
    }

    function endMoveDrag(commit) {
        if (dragMode === editorInteraction.dragModeMove && !commit) {
            window.editor.setSelectedBounds(boxMoveInteraction.moveStartX, boxMoveInteraction.moveStartY, activeMoveDelegate.boxModel.w, activeMoveDelegate.boxModel.h);
        }
        boxMoveInteraction.reset();
        dragMode = editorInteraction.dragModeIdle;
    }

    function beginPathHandleDrag(pathPlane, index, startX, startY, pressCanvasX, pressCanvasY) {
        dragMode = editorInteraction.dragModePathHandle;
        pathHandleInteraction.begin(pathPlane, index, startX, startY, canvas, pressCanvasX, pressCanvasY);
        window.editor.beginInteraction();
    }

    function updatePathHandleDragFromCanvas(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModePathHandle)
            return ;

        const result = pathHandleInteraction.update(canvas, canvasX, canvasY, window.editor.leftMouseButtonDown());
        if (result.ended) {
            endPathHandleDrag();
            return ;
        }
        if (result.changed)
            window.editor.setPathHandle(result.index, result.x, result.y);

    }

    function endPathHandleDrag() {
        if (dragMode !== editorInteraction.dragModePathHandle)
            return false;

        if (pathHandleInteraction.reset())
            window.editor.endInteraction();

        dragMode = editorInteraction.dragModeIdle;
        return true;
    }

    function handleEscape() {
        if (Editor.editingText) {
            Editor.endTextEdit();
            canvas.forceActiveFocus();
        } else if (dragMode !== editorInteraction.dragModeIdle) {
            if (dragMode === editorInteraction.dragModeResize)
                endResizeDrag(false);
            else if (dragMode === editorInteraction.dragModePerspective)
                endPerspectiveDrag(false);
            else if (dragMode === editorInteraction.dragModeRotate)
                endRotateDrag(false);
            else if (dragMode === editorInteraction.dragModeMove)
                endMoveDrag(false);
            else if (dragMode === editorInteraction.dragModePathHandle)
                endPathHandleDrag();
            else if (dragMode === editorInteraction.dragModePaint)
                cancelPaintDrag();
            dragMode = editorInteraction.dragModeIdle;
        } else if (Editor.selectedIndex >= 0) {
            Editor.selectBox(-1);
        }
    }

    function paintPoint(x, y) {
        return [window.viewToDocumentX(x), window.viewToDocumentY(y)];
    }

    function activePaintStroke(target) {
        if (window.activePaintTarget !== target || Editor.currentPageIndex !== activePaintPageIndex || rightPanel.paintEraserMode || activePaintPreviewPoints.length === 0)
            return {};

        return {
            "color": rightPanel.paintBrushColor,
            "size": rightPanel.paintBrushSize,
            "opacity": rightPanel.paintBrushOpacity,
            "points": activePaintPreviewPoints
        };
    }

    function publishPaintPreview() {
        paintPreviewPublishTimer.stop();
        if (dragMode !== editorInteraction.dragModePaint || rightPanel.paintEraserMode || activePaintPoints.length === 0)
            return ;
        activePaintPreviewPoints = activePaintPoints.slice();
        ++paintPreviewPublishRevision;
    }

    function clearPaintDrag() {
        paintPreviewPublishTimer.stop();
        activePaintPoints = [];
        activePaintPreviewPoints = [];
        activePaintTarget = "";
        activePaintPageIndex = -1;
        dragMode = editorInteraction.dragModeIdle;
    }

    function paintDragPageMatchesOrigin() {
        return activePaintPageIndex >= 0 && Editor.currentPageIndex === activePaintPageIndex;
    }

    function schedulePaintPreviewPublish() {
        if (!paintPreviewPublishTimer.running)
            paintPreviewPublishTimer.start();
    }

    function beginPaintDrag(x, y) {
        if (!rightPanel.paintMode)
            return false;
        Editor.endTextEdit();
        dragMode = editorInteraction.dragModePaint;
        activePaintTarget = rightPanel.paintTarget;
        activePaintPageIndex = Editor.currentPageIndex;
        const point = paintPoint(x, y);
        if (rightPanel.paintEraserMode) {
            Editor.erasePaintAt(activePaintTarget, point[0], point[1], rightPanel.paintEraserSize);
        } else {
            activePaintPoints = [point];
            publishPaintPreview();
        }
        return true;
    }

    function updatePaintDrag(x, y) {
        if (dragMode !== editorInteraction.dragModePaint)
            return ;
        if (!paintDragPageMatchesOrigin()) {
            cancelPaintDrag();
            return ;
        }
        const point = paintPoint(x, y);
        if (rightPanel.paintEraserMode) {
            Editor.erasePaintAt(activePaintTarget, point[0], point[1], rightPanel.paintEraserSize);
            return ;
        }
        activePaintPoints.push(point);
        schedulePaintPreviewPublish();
    }

    function endPaintDrag() {
        if (dragMode !== editorInteraction.dragModePaint)
            return false;
        if (paintDragPageMatchesOrigin())
            publishPaintPreview();
        if (!rightPanel.paintEraserMode && activePaintPoints.length > 0 && paintDragPageMatchesOrigin())
            Editor.addPaintStroke(activePaintTarget, rightPanel.paintBrushColor, rightPanel.paintBrushSize, rightPanel.paintBrushOpacity, activePaintPoints);
        clearPaintDrag();
        return true;
    }

    function cancelPaintDrag() {
        if (dragMode !== editorInteraction.dragModePaint)
            return false;
        clearPaintDrag();
        return true;
    }

    width: 1280
    height: 800
    visible: true
    title: qsTr("TextFX")
    menuBar: chrome.menuBar

    QtObject {
        id: editorInteraction

        readonly property int dragModeIdle: 0
        readonly property int dragModePan: 2
        readonly property int dragModeCreate: 3
        readonly property int dragModeResize: 4
        readonly property int dragModePerspective: 5
        readonly property int dragModeRotate: 6
        readonly property int dragModeMove: 7
        readonly property int dragModePathHandle: 8
        readonly property int dragModePaint: 9
    }

    Timer {
        id: paintPreviewPublishTimer

        interval: 16
        repeat: false
        onTriggered: window.publishPaintPreview()
    }

    QtObject {
        id: editorLimits

        readonly property real minimumBoxSize: EditorLimits.minimumBoxSize
        readonly property int minimumFontSize: EditorLimits.minimumFontSize
        readonly property int maximumFontSize: EditorLimits.maximumFontSize
        readonly property int minimumTextSpacing: EditorLimits.minimumTextSpacing
        readonly property int maximumTextSpacing: EditorLimits.maximumTextSpacing
        readonly property int minimumEffectSize: EditorLimits.minimumEffectSize
        readonly property int maximumEffectSize: EditorLimits.maximumEffectSize
        readonly property int minimumShadowOffset: EditorLimits.minimumShadowOffset
        readonly property int maximumShadowOffset: EditorLimits.maximumShadowOffset
    }

    ViewportMetrics {
        id: viewportMetrics

        objectName: "viewportMetrics"
        zoom: window.zoom
        pageBaseScale: window.pageBaseScale
        panX: window.panX
        panY: window.panY
        canvasWidth: canvas.width
        canvasHeight: canvas.height
        pageSourceWidth: canvasView.pageSourceWidth
        pageSourceHeight: canvasView.pageSourceHeight
    }

    CanvasInteractionState {
        id: canvasInteraction

        objectName: "canvasInteractionState"
        idleMode: editorInteraction.dragModeIdle
        panMode: editorInteraction.dragModePan
        createMode: editorInteraction.dragModeCreate
    }

    BoxResizeInteractionState {
        id: boxResizeInteraction

        objectName: "boxResizeInteractionState"
        documentScale: window.viewDocScale()
        minimumBoxSize: editorLimits.minimumBoxSize
    }

    BoxMoveInteractionState {
        id: boxMoveInteraction

        objectName: "boxMoveInteractionState"
        documentScale: window.viewDocScale()
    }

    BoxRotateInteractionState {
        id: boxRotateInteraction

        objectName: "boxRotateInteractionState"
    }

    PerspectiveInteractionState {
        id: perspectiveInteraction

        objectName: "perspectiveInteractionState"
        documentScale: window.viewDocScale()
    }

    PathHandleInteractionState {
        id: pathHandleInteraction

        objectName: "pathHandleInteractionState"
    }

    PerspectiveGeometry {
        id: perspectiveGeometry

        objectName: "perspectiveGeometry"
        documentScale: window.viewDocScale()
        handleSize: window.handleSize()
        rotateHandleDistance: window.rotateHandleDistance()
        livePerspectiveActive: window.dragMode === editorInteraction.dragModePerspective && !!window.activePerspectiveDelegate && window.activePerspectiveDelegate.boxModel && window.activePerspectiveDelegate.boxModel.index === Editor.selectedIndex
        activePerspectiveBoxIndex: perspectiveInteraction.activePerspectiveBoxIndex
        activePerspectiveHandle: window.activePerspectiveHandle
        perspectiveStartX: window.perspectiveStartX
        perspectiveStartY: window.perspectiveStartY
        perspectiveX: window.perspectiveX
        perspectiveY: window.perspectiveY
        perspectiveStartNwX: window.perspectiveStartNwX
        perspectiveStartNwY: window.perspectiveStartNwY
        perspectiveStartNeX: window.perspectiveStartNeX
        perspectiveStartNeY: window.perspectiveStartNeY
        perspectiveStartSeX: window.perspectiveStartSeX
        perspectiveStartSeY: window.perspectiveStartSeY
        perspectiveStartSwX: window.perspectiveStartSwX
        perspectiveStartSwY: window.perspectiveStartSwY
    }

    EditorChrome {
        id: chrome

        objectName: "editorChrome"
        editor: window.editor
        hostPalette: window.palette
        hostWidth: window.width
        hostHeight: window.height
        zoomCenterX: canvas.width / 2
        zoomCenterY: canvas.height / 2
        pageNavigationEnabled: window.pageNavigationEnabled
        paintBrushColorTarget: rightPanel
        sidePanelTextInputFocused: window.sidePanelFocusedTextInputs > 0
        onZoomAtRequested: (x, y, factor) => {
            return window.zoomAt(x, y, factor);
        }
        onResetZoomRequested: window.resetZoom()
        onEscapeRequested: window.handleEscape()
    }

    Rectangle {
        anchors.fill: parent
        color: window.palette.window

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            SplitView {
                id: mainSplit

                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal
                visible: Editor.hasProject
                enabled: Editor.hasProject

                LeftInspectorPanel {
                    id: sidePanel

                    objectName: "leftInspectorPanel"
                    editor: window.editor
                    editorLimits: editorLimits
                    fontFamilyOptionsProvider: (selected) => {
                        return window.fontFamilyOptions(selected);
                    }
                    qmlColorProvider: (hex) => {
                        return window.qmlColor(hex);
                    }
                    onColorDialogRequested: (hex, setter) => {
                        return window.openColorDialog(hex, setter);
                    }
                    onTextInputFocusChanged: (active) => {
                        return window.noteSidePanelTextInputFocus(active);
                    }
                }

                CanvasView {
                    id: canvasView

                    appWindow: window
                    editor: window.editor
                    rightPanel: rightPanel
                    editorLimits: editorLimits
                    editorInteraction: editorInteraction
                    canvasInteraction: canvasInteraction
                }

                RightInspectorPanel {
                    id: rightPanel

                    objectName: "rightInspectorPanel"
                    editor: window.editor
                    editorLimits: editorLimits
                    displayScale: viewportMetrics.viewDocScale()
                    minimumDisplayScale: window.pageBaseScale * viewportMetrics.minimumZoom
                    maximumDisplayScale: window.pageBaseScale * viewportMetrics.maximumZoom
                    pageNavigationEnabled: window.pageNavigationEnabled
                    qmlColorProvider: (hex) => {
                        return window.qmlColor(hex);
                    }
                    onColorDialogRequested: (hex, setter) => {
                        return window.openColorDialog(hex, setter);
                    }
                    onZoomRequested: (displayScale) => {
                        return window.setDisplayScaleAtCenter(displayScale);
                    }
                }

            }

        }

        ColumnLayout {
            id: startScreen

            objectName: "startScreen"
            anchors.centerIn: parent
            spacing: 12
            visible: !Editor.hasProject

            Button {
                objectName: "startOpenButton"
                text: qsTr("Open ")
                Layout.preferredWidth: 180
                onClicked: chrome.openProjectPicker()
            }

            Button {
                objectName: "startNewButton"
                text: qsTr("New ")
                Layout.preferredWidth: 180
                onClicked: chrome.openNewProjectPicker()
            }

        }

    }

}
