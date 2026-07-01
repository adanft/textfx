import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TextFX.Ui 1.0

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: qsTr("TextFX")

    readonly property var editor: Editor
    property alias canvas: canvasShell.canvasItem
    property real zoom: 1.0
    property real pageBaseScale: 1.0
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
    property var activePerspectiveDelegate: null
    property string activePerspectiveHandle: ""
    property real perspectiveStartCanvasX: 0
     property real perspectiveStartCanvasY: 0
     property real perspectiveStartX: 0
     property real perspectiveStartY: 0
     property real perspectiveX: 0
     property real perspectiveY: 0
     property int perspectiveRevision: 0
      property real perspectiveStartNwX: 0
      property real perspectiveStartNwY: 0
      property real perspectiveStartNeX: 0
      property real perspectiveStartNeY: 0
      property real perspectiveStartSeX: 0
      property real perspectiveStartSeY: 0
      property real perspectiveStartSwX: 0
      property real perspectiveStartSwY: 0
      property var activeRotateDelegate: null
      property real rotateStartRotation: 0
      property real rotateStartAngle: 0
      property real rotateDegrees: 0
      property var activeMoveDelegate: null
      property int activeMoveIndex: -1
      property real moveStartX: 0
      property real moveStartY: 0
      property real moveStartCanvasX: 0
      property real moveStartCanvasY: 0
      property real moveX: 0
      property real moveY: 0
        property int activePathHandleIndex: -1
        property var activePathHandlePlane: null
        property bool activePathHandlePerspective: false
        property bool pathHandleInteractionActive: false
        property bool pathHandleDragPressed: false
        property real activePathBoxWidth: 1
        property real activePathBoxHeight: 1
        property real activePathBoxRotation: 0
        property real pathHandlePressCanvasX: 0
        property real pathHandlePressCanvasY: 0
        property real pathHandlePressLocalX: 0
        property real pathHandlePressLocalY: 0
        property real pathHandleStartX: 0
        property real pathHandleStartY: 0

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
    }

    QtObject {
        id: editorLimits

        readonly property real minimumBoxSize: 12
        readonly property int minimumFontSize: 1
        readonly property int maximumFontSize: 512
        readonly property int minimumTextSpacing: -100
        readonly property int maximumTextSpacing: 300
        readonly property int minimumEffectSize: 0
        readonly property int maximumEffectSize: 128
        readonly property int minimumShadowOffset: -512
        readonly property int maximumShadowOffset: 512
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
        pageSourceWidth: pageImage.sourceSize.width
        pageSourceHeight: pageImage.sourceSize.height
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

    function fitPageScale() { return viewportMetrics.fitPageScale() }
    function pageScale() { return viewportMetrics.pageScale() }
    function pageDisplayWidth() { return viewportMetrics.pageDisplayWidth() }
    function pageDisplayHeight() { return viewportMetrics.pageDisplayHeight() }
    function pageLeft() { return viewportMetrics.pageLeft() }
    function pageTop() { return viewportMetrics.pageTop() }
    function viewDocScale() { return viewportMetrics.viewDocScale() }
    function livePreviewScale() { return viewportMetrics.livePreviewScale() }
    function documentToViewLength(value) { return viewportMetrics.documentToViewLength(value) }
    function selectionLineWidth() { return Math.max(1, documentToViewLength(2)) }
    function handleSize() { return Math.max(1, documentToViewLength(editorLimits.minimumBoxSize)) }
    function rotateHandleDistance() { return documentToViewLength(28) }
    function documentToViewX(x) { return viewportMetrics.documentToViewX(x) }
    function documentToViewY(y) { return viewportMetrics.documentToViewY(y) }
    function viewToDocumentX(x) { return viewportMetrics.viewToDocumentX(x) }
    function viewToDocumentY(y) { return viewportMetrics.viewToDocumentY(y) }

    function zoomAt(x, y, factor) {
        const nextState = viewportMetrics.zoomStateAt(x, y, factor)
        if (!nextState.changed)
            return
        window.panX = nextState.panX
        window.panY = nextState.panY
        window.zoom = nextState.zoom
    }

    function resetZoom() {
        const reset = viewportMetrics.resetState()
        window.zoom = reset.zoom
        window.panX = reset.panX
        window.panY = reset.panY
    }

    function selectedBox() {
        for (let i = 0; i < Editor.boxes.length; ++i) {
            if (Editor.boxes[i].index === Editor.selectedIndex)
                return Editor.boxes[i]
        }
        return null
    }

    function fontFamilyOptions(selected) {
        const options = []
        function add(value) {
            const family = String(value || "").trim()
            if (family.length > 0 && options.indexOf(family) < 0)
                options.push(family)
        }
        add(selected)
        if (typeof Qt.fontFamilies === "function") {
            const families = Qt.fontFamilies()
            for (let i = 0; i < families.length; ++i)
                add(families[i])
        }
        add("sans-serif")
        add("serif")
        add("monospace")
        return options
    }

    function noteSidePanelTextInputFocus(active) {
        sidePanelFocusedTextInputs = Math.max(0, sidePanelFocusedTextInputs + (active ? 1 : -1))
    }

    function qmlColor(hex) {
        const raw = String(hex || "000000ff")
        const value = raw.startsWith("#") ? raw.slice(1) : raw
        return "#" + value.slice(0, 6)
    }

    function openColorDialog(hex, setter) {
        chrome.openColorDialog(hex, setter)
    }

    function pointValue(point, index, fallback) {
        return point && point.length > index ? point[index] : fallback
    }

    function perspectiveBase(name, axis) {
        if (axis === 0)
            return name === "ne" || name === "e" || name === "se" ? 1 : name === "n" || name === "s" ? 0.5 : 0
        return name === "se" || name === "s" || name === "sw" ? 1 : name === "e" || name === "w" ? 0.5 : 0
    }

    function usesLivePerspectiveOffset(box, name, axis) {
        if (dragMode !== editorInteraction.dragModePerspective || !box || box.index !== Editor.selectedIndex)
            return false
        const handle = activePerspectiveHandle
        if (name === handle && (handle === "nw" || handle === "ne" || handle === "se" || handle === "sw"))
            return true
        if (handle === "n" && axis === 1) return name === "nw" || name === "ne" || name === "n"
        if (handle === "e" && axis === 0) return name === "ne" || name === "se" || name === "e"
        if (handle === "s" && axis === 1) return name === "sw" || name === "se" || name === "s"
        if (handle === "w" && axis === 0) return name === "nw" || name === "sw" || name === "w"
        return false
    }

    function modelPerspectiveOffset(box, name, axis) {
        if (name === "nw") return pointValue(box.perspectiveNw, axis, 0)
        if (name === "ne") return pointValue(box.perspectiveNe, axis, 0)
        if (name === "se") return pointValue(box.perspectiveSe, axis, 0)
        if (name === "sw") return pointValue(box.perspectiveSw, axis, 0)
        if (name === "n") return (pointValue(box.perspectiveNw, axis, 0) + pointValue(box.perspectiveNe, axis, 0)) / 2
        if (name === "e") return (pointValue(box.perspectiveNe, axis, 0) + pointValue(box.perspectiveSe, axis, 0)) / 2
        if (name === "s") return (pointValue(box.perspectiveSw, axis, 0) + pointValue(box.perspectiveSe, axis, 0)) / 2
        if (name === "w") return (pointValue(box.perspectiveNw, axis, 0) + pointValue(box.perspectiveSw, axis, 0)) / 2
        return 0
    }

    function perspectiveOffset(box, name, axis) {
        if (!usesLivePerspectiveOffset(box, name, axis))
            return modelPerspectiveOffset(box, name, axis)
        const dx = perspectiveX - perspectiveStartX
        const dy = perspectiveY - perspectiveStartY
        if (activePerspectiveHandle === "n") return name === "nw" ? perspectiveStartNwY + dy : name === "ne" ? perspectiveStartNeY + dy : (perspectiveStartNwY + perspectiveStartNeY) / 2 + dy
        if (activePerspectiveHandle === "e") return name === "ne" ? perspectiveStartNeX + dx : name === "se" ? perspectiveStartSeX + dx : (perspectiveStartNeX + perspectiveStartSeX) / 2 + dx
        if (activePerspectiveHandle === "s") return name === "sw" ? perspectiveStartSwY + dy : name === "se" ? perspectiveStartSeY + dy : (perspectiveStartSwY + perspectiveStartSeY) / 2 + dy
        if (activePerspectiveHandle === "w") return name === "nw" ? perspectiveStartNwX + dx : name === "sw" ? perspectiveStartSwX + dx : (perspectiveStartNwX + perspectiveStartSwX) / 2 + dx
        if (name === activePerspectiveHandle)
            return axis === 0 ? perspectiveX : perspectiveY
        return modelPerspectiveOffset(box, name, axis)
    }

    function visualHandlePosition(box, name, width, height) {
        if (!box || !box.perspective)
            return { x: perspectiveBase(name, 0) * width, y: perspectiveBase(name, 1) * height }
        if (name === "n" || name === "e" || name === "s" || name === "w") {
            const a = name === "n" || name === "w" ? perspectiveCorner(box, "nw", width, height) : perspectiveCorner(box, "se", width, height)
            const b = name === "n" || name === "e" ? perspectiveCorner(box, "ne", width, height) : perspectiveCorner(box, "sw", width, height)
            return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }
        }
        return perspectiveCorner(box, name, width, height)
    }

    function topMiddleVisualPoint(box, width, height) {
        if (!box || !box.perspective)
            return { x: width / 2, y: 0 }
        const nw = perspectiveCorner(box, "nw", width, height)
        const ne = perspectiveCorner(box, "ne", width, height)
        return { x: (nw.x + ne.x) / 2, y: (nw.y + ne.y) / 2 }
    }

    function rotateHandlePosition(box, width, height) {
        const top = topMiddleVisualPoint(box, width, height)
        if (!box || !box.perspective)
            return { x: top.x, y: top.y - rotateHandleDistance() }
        const nw = perspectiveCorner(box, "nw", width, height)
        const ne = perspectiveCorner(box, "ne", width, height)
        const edgeX = ne.x - nw.x
        const edgeY = ne.y - nw.y
        const edgeLength = Math.max(1, Math.sqrt(edgeX * edgeX + edgeY * edgeY))
        return { x: top.x + edgeY / edgeLength * rotateHandleDistance(), y: top.y - edgeX / edgeLength * rotateHandleDistance() }
    }

    function perspectiveLayoutCorner(box, name, width, height) {
        return {
            x: perspectiveBase(name, 0) * width + perspectiveOffset(box, name, 0),
            y: perspectiveBase(name, 1) * height + perspectiveOffset(box, name, 1)
        }
    }

    function perspectiveCorner(box, name, width, height) {
        const scale = Math.max(0.0001, viewDocScale())
        const corner = perspectiveLayoutCorner(box, name, width / scale, height / scale)
        return {
            x: corner.x * scale,
            y: corner.y * scale
        }
    }

    function perspectiveVisualBounds(box, width, height) {
        const nw = perspectiveCorner(box, "nw", width, height)
        const ne = perspectiveCorner(box, "ne", width, height)
        const se = perspectiveCorner(box, "se", width, height)
        const sw = perspectiveCorner(box, "sw", width, height)
        const left = Math.min(nw.x, ne.x, se.x, sw.x) - 1
        const top = Math.min(nw.y, ne.y, se.y, sw.y) - 1
        const right = Math.max(nw.x, ne.x, se.x, sw.x) + 1
        const bottom = Math.max(nw.y, ne.y, se.y, sw.y) + 1
        return { x: left, y: top, width: right - left, height: bottom - top }
    }

    function pointOnSegment(point, a, b) {
        const cross = (point.y - a.y) * (b.x - a.x) - (point.x - a.x) * (b.y - a.y)
        if (Math.abs(cross) > 0.001) return false
        const dot = (point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y)
        if (dot < 0) return false
        const lengthSquared = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)
        return dot <= lengthSquared
    }

    function pointInPerspectivePolygon(point, box, width, height) {
        const polygon = [perspectiveCorner(box, "nw", width, height), perspectiveCorner(box, "ne", width, height),
                         perspectiveCorner(box, "se", width, height), perspectiveCorner(box, "sw", width, height)]
        let inside = false
        for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
            const a = polygon[i]
            const b = polygon[j]
            if (pointOnSegment(point, a, b)) return true
            if (((a.y > point.y) !== (b.y > point.y)) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                inside = !inside
        }
        return inside
    }

    function perspectiveMargin(box) {
        if (!box || !box.perspective) return 0
        return Math.max(Math.abs(pointValue(box.perspectiveNw, 0, 0)), Math.abs(pointValue(box.perspectiveNw, 1, 0)),
                        Math.abs(pointValue(box.perspectiveNe, 0, 0)), Math.abs(pointValue(box.perspectiveNe, 1, 0)),
                        Math.abs(pointValue(box.perspectiveSe, 0, 0)), Math.abs(pointValue(box.perspectiveSe, 1, 0)),
                        Math.abs(pointValue(box.perspectiveSw, 0, 0)), Math.abs(pointValue(box.perspectiveSw, 1, 0))) * window.viewDocScale() + handleSize()
    }

    function perspectiveMatrix(box, width, height, renderScale, enabled) {
        if (!enabled || width <= 0 || height <= 0)
            return Qt.matrix4x4(1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1)
        const scale = renderScale > 0 ? renderScale : 1
        const layoutWidth = width / scale
        const layoutHeight = height / scale
        const p0 = perspectiveLayoutCorner(box, "nw", layoutWidth, layoutHeight)
        const p1 = perspectiveLayoutCorner(box, "ne", layoutWidth, layoutHeight)
        const p2 = perspectiveLayoutCorner(box, "se", layoutWidth, layoutHeight)
        const p3 = perspectiveLayoutCorner(box, "sw", layoutWidth, layoutHeight)
        const dx1 = p1.x - p2.x, dy1 = p1.y - p2.y
        const dx2 = p3.x - p2.x, dy2 = p3.y - p2.y
        const dx3 = p0.x - p1.x + p2.x - p3.x
        const dy3 = p0.y - p1.y + p2.y - p3.y
        let g = 0, h = 0
        const det = dx1 * dy2 - dx2 * dy1
        if (Math.abs(det) > 0.000001 && (Math.abs(dx3) > 0.000001 || Math.abs(dy3) > 0.000001)) {
            g = (dx3 * dy2 - dx2 * dy3) / det
            h = (dx1 * dy3 - dx3 * dy1) / det
        }
        const a = (p1.x - p0.x + g * p1.x) / layoutWidth
        const b = (p3.x - p0.x + h * p3.x) / layoutHeight
        const d = (p1.y - p0.y + g * p1.y) / layoutWidth
        const e = (p3.y - p0.y + h * p3.y) / layoutHeight
        return Qt.matrix4x4(a, b, 0, p0.x * scale,
                            d, e, 0, p0.y * scale,
                            0, 0, 1, 0,
                            g / width, h / height, 0, 1)
    }

    function angleDegrees(cx, cy, x, y) {
        return Math.atan2(y - cy, x - cx) * 180 / Math.PI
    }

    function angleDeltaDegrees(from, to) {
        let delta = to - from
        while (delta > 180) delta -= 360
        while (delta < -180) delta += 360
        return delta
    }

    function beginResizeDrag(delegate, handle, canvasX, canvasY) {
        dragMode = editorInteraction.dragModeResize
        boxResizeInteraction.begin(delegate, handle, canvasX, canvasY)
    }

    function updateResizeDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeResize || !activeResizeDelegate)
            return
        boxResizeInteraction.update(canvasX, canvasY)
    }

    function endResizeDrag(commit) {
        if (dragMode === editorInteraction.dragModeResize && commit) {
            const bounds = boxResizeInteraction.bounds()
            window.editor.setSelectedBounds(bounds.x, bounds.y, bounds.width, bounds.height)
        }
        boxResizeInteraction.reset()
        dragMode = editorInteraction.dragModeIdle
    }

    function beginPerspectiveDrag(delegate, handle, canvasX, canvasY) {
        activePerspectiveDelegate = delegate
        activePerspectiveHandle = handle
        dragMode = editorInteraction.dragModePerspective
        perspectiveStartCanvasX = canvasX
        perspectiveStartCanvasY = canvasY
        perspectiveStartNwX = modelPerspectiveOffset(delegate.boxModel, "nw", 0)
        perspectiveStartNwY = modelPerspectiveOffset(delegate.boxModel, "nw", 1)
        perspectiveStartNeX = modelPerspectiveOffset(delegate.boxModel, "ne", 0)
        perspectiveStartNeY = modelPerspectiveOffset(delegate.boxModel, "ne", 1)
        perspectiveStartSeX = modelPerspectiveOffset(delegate.boxModel, "se", 0)
        perspectiveStartSeY = modelPerspectiveOffset(delegate.boxModel, "se", 1)
        perspectiveStartSwX = modelPerspectiveOffset(delegate.boxModel, "sw", 0)
        perspectiveStartSwY = modelPerspectiveOffset(delegate.boxModel, "sw", 1)
        perspectiveStartX = modelPerspectiveOffset(delegate.boxModel, handle, 0)
        perspectiveStartY = modelPerspectiveOffset(delegate.boxModel, handle, 1)
        perspectiveX = perspectiveStartX
        perspectiveY = perspectiveStartY
    }

    function updatePerspectiveDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModePerspective || !activePerspectiveDelegate)
            return
        const startLocal = activePerspectiveDelegate.mapFromItem(canvas, perspectiveStartCanvasX, perspectiveStartCanvasY)
        const local = activePerspectiveDelegate.mapFromItem(canvas, canvasX, canvasY)
        const dx = (local.x - startLocal.x) / viewDocScale()
        const dy = (local.y - startLocal.y) / viewDocScale()
        perspectiveX = perspectiveStartX + (activePerspectiveHandle === "n" || activePerspectiveHandle === "s" ? 0 : dx)
        perspectiveY = perspectiveStartY + (activePerspectiveHandle === "e" || activePerspectiveHandle === "w" ? 0 : dy)
        ++perspectiveRevision
    }

    function commitPerspectiveCorner(name, x, y) {
        window.editor.setPerspectiveHandle(name, x, y)
    }

    function endPerspectiveDrag(commit) {
        if (dragMode === editorInteraction.dragModePerspective && commit) {
            const dx = perspectiveX - perspectiveStartX
            const dy = perspectiveY - perspectiveStartY
            if (activePerspectiveHandle === "n") { commitPerspectiveCorner("nw", perspectiveStartNwX, perspectiveStartNwY + dy); commitPerspectiveCorner("ne", perspectiveStartNeX, perspectiveStartNeY + dy) }
            else if (activePerspectiveHandle === "e") { commitPerspectiveCorner("ne", perspectiveStartNeX + dx, perspectiveStartNeY); commitPerspectiveCorner("se", perspectiveStartSeX + dx, perspectiveStartSeY) }
            else if (activePerspectiveHandle === "s") { commitPerspectiveCorner("sw", perspectiveStartSwX, perspectiveStartSwY + dy); commitPerspectiveCorner("se", perspectiveStartSeX, perspectiveStartSeY + dy) }
            else if (activePerspectiveHandle === "w") { commitPerspectiveCorner("nw", perspectiveStartNwX + dx, perspectiveStartNwY); commitPerspectiveCorner("sw", perspectiveStartSwX + dx, perspectiveStartSwY) }
            else window.editor.setPerspectiveHandle(activePerspectiveHandle, perspectiveX, perspectiveY)
        }
        activePerspectiveDelegate = null
        activePerspectiveHandle = ""
        dragMode = editorInteraction.dragModeIdle
    }

    function beginRotateDrag(delegate, canvasX, canvasY) {
        activeRotateDelegate = delegate
        dragMode = editorInteraction.dragModeRotate
        const cx = delegate.boxModel.x + delegate.boxModel.w / 2
        const cy = delegate.boxModel.y + delegate.boxModel.h / 2
        rotateStartRotation = delegate.boxModel.rotation
        rotateDegrees = rotateStartRotation
        rotateStartAngle = angleDegrees(cx, cy, viewToDocumentX(canvasX), viewToDocumentY(canvasY))
    }

    function updateRotateDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeRotate || !activeRotateDelegate)
            return
        const box = activeRotateDelegate.boxModel
        const cx = box.x + box.w / 2
        const cy = box.y + box.h / 2
        const angle = angleDegrees(cx, cy, viewToDocumentX(canvasX), viewToDocumentY(canvasY))
        rotateDegrees = rotateStartRotation + angleDeltaDegrees(rotateStartAngle, angle)
        activeRotateDelegate.rotation = rotateDegrees
    }

    function endRotateDrag(commit) {
        if (dragMode === editorInteraction.dragModeRotate && commit)
            window.editor.setSelectedRotation(rotateDegrees)
        else if (dragMode === editorInteraction.dragModeRotate && activeRotateDelegate)
            activeRotateDelegate.rotation = rotateStartRotation
        activeRotateDelegate = null
        dragMode = editorInteraction.dragModeIdle
    }

    function beginMoveDrag(delegate, index, canvasX, canvasY) {
        activeMoveDelegate = delegate
        activeMoveIndex = index
        dragMode = editorInteraction.dragModeMove
        moveStartX = delegate.boxModel.x
        moveStartY = delegate.boxModel.y
        moveStartCanvasX = canvasX
        moveStartCanvasY = canvasY
        moveX = moveStartX
        moveY = moveStartY
    }

    function updateMoveDrag(canvasX, canvasY) {
        if (dragMode !== editorInteraction.dragModeMove || !activeMoveDelegate)
            return
        moveX = moveStartX + (canvasX - moveStartCanvasX) / viewDocScale()
        moveY = moveStartY + (canvasY - moveStartCanvasY) / viewDocScale()
    }

    function endMoveDrag(commit) {
        if (dragMode === editorInteraction.dragModeMove && commit)
            window.editor.moveSelected(moveX - moveStartX, moveY - moveStartY)
        activeMoveDelegate = null
        activeMoveIndex = -1
        dragMode = editorInteraction.dragModeIdle
    }

    function beginPathHandleDrag(pathPlane, index, startX, startY, pressCanvasX, pressCanvasY) {
        activePathHandlePlane = pathPlane
        activePathBoxWidth = pathPlane ? pathPlane.width : 1
        activePathBoxHeight = pathPlane ? pathPlane.height : 1
        activePathHandlePerspective = !!(pathPlane && pathPlane.boxRef && pathPlane.boxRef.perspectiveActive)
        activePathBoxRotation = pathPlane && pathPlane.boxRef ? pathPlane.boxRef.rotation : 0
        activePathHandleIndex = index
        pathHandlePressCanvasX = pressCanvasX
        pathHandlePressCanvasY = pressCanvasY
        const pressLocal = pathPlane ? pathPlane.mapFromItem(canvas, pressCanvasX, pressCanvasY) : Qt.point(pressCanvasX, pressCanvasY)
        pathHandlePressLocalX = pressLocal.x
        pathHandlePressLocalY = pressLocal.y
        pathHandleStartX = startX
        pathHandleStartY = startY
        dragMode = editorInteraction.dragModePathHandle
        pathHandleDragPressed = true
        pathHandleInteractionActive = true
        window.editor.beginInteraction()
    }

    function updatePathHandleDragFromCanvas(canvasX, canvasY) {
        if (!pathHandleInteractionActive || !pathHandleDragPressed || dragMode !== editorInteraction.dragModePathHandle || activePathHandleIndex < 0)
            return
        if (!window.editor.leftMouseButtonDown()) {
            endPathHandleDrag()
            return
        }
        let localDx = 0
        let localDy = 0
        if (activePathHandlePerspective) {
            const local = activePathHandlePlane ? activePathHandlePlane.mapFromItem(canvas, canvasX, canvasY) : Qt.point(canvasX, canvasY)
            localDx = local.x - pathHandlePressLocalX
            localDy = local.y - pathHandlePressLocalY
        } else {
            const radians = -activePathBoxRotation * Math.PI / 180
            const dx = canvasX - pathHandlePressCanvasX
            const dy = canvasY - pathHandlePressCanvasY
            localDx = dx * Math.cos(radians) - dy * Math.sin(radians)
            localDy = dx * Math.sin(radians) + dy * Math.cos(radians)
        }
        window.editor.setPathHandle(activePathHandleIndex, (pathHandleStartX + localDx) / activePathBoxWidth, (pathHandleStartY + localDy) / activePathBoxHeight)
    }

    function endPathHandleDrag() {
        if (dragMode !== editorInteraction.dragModePathHandle)
            return false
        if (pathHandleInteractionActive)
            window.editor.endInteraction()
        pathHandleDragPressed = false
        pathHandleInteractionActive = false
        activePathHandlePlane = null
        activePathHandlePerspective = false
        activePathHandleIndex = -1
        dragMode = editorInteraction.dragModeIdle
        return true
    }

    function handleEscape() {
        if (Editor.editingText) {
            Editor.endTextEdit()
            canvas.forceActiveFocus()
        } else if (dragMode !== editorInteraction.dragModeIdle) {
            if (dragMode === editorInteraction.dragModeResize)
                endResizeDrag(false)
            else if (dragMode === editorInteraction.dragModePerspective)
                endPerspectiveDrag(false)
            else if (dragMode === editorInteraction.dragModeRotate)
                endRotateDrag(false)
            else if (dragMode === editorInteraction.dragModeMove)
                endMoveDrag(false)
            else if (dragMode === editorInteraction.dragModePathHandle)
                endPathHandleDrag()
            dragMode = editorInteraction.dragModeIdle
        } else if (Editor.selectedIndex >= 0) {
            Editor.selectBox(-1)
        }
    }

    menuBar: chrome.menuBar

    EditorChrome {
        id: chrome
        objectName: "editorChrome"
        editor: window.editor
        hostPalette: window.palette
        hostWidth: window.width
        hostHeight: window.height
        zoomCenterX: canvas.width / 2
        zoomCenterY: canvas.height / 2
        sidePanelTextInputFocused: window.sidePanelFocusedTextInputs > 0
        onZoomAtRequested: (x, y, factor) => window.zoomAt(x, y, factor)
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

                LeftInspectorPanel {
                    id: sidePanel
                    objectName: "leftInspectorPanel"
                    editor: window.editor
                    editorLimits: editorLimits
                    selectedBoxProvider: () => window.selectedBox()
                    fontFamilyOptionsProvider: selected => window.fontFamilyOptions(selected)
                    qmlColorProvider: hex => window.qmlColor(hex)
                    onColorDialogRequested: (hex, setter) => window.openColorDialog(hex, setter)
                    onTextInputFocusChanged: active => window.noteSidePanelTextInputFocus(active)
                }

                CentralCanvasShell {
                    id: canvasShell
                    objectName: "centralCanvasShell"
                    hostPalette: window.palette
                    editingText: Editor.editingText
                    onEscapePressed: window.handleEscape()
                    onDeletePressed: Editor.deleteSelected()
                    onCanvasPressed: (x, y, button, modifiers) => {
                        if (button === Qt.LeftButton)
                            Editor.endTextEdit()
                        canvasInteraction.beginPress(x, y, button, modifiers)
                    }
                    onCanvasPositionChanged: (x, y, pressed) => {
                        const update = canvasInteraction.updatePointer(x, y, pressed)
                        if (update.panned) { window.panX += update.panDx; window.panY += update.panDy }
                    }
                    onCanvasReleased: (x, y, button, modifiers) => {
                        const release = canvasInteraction.endRelease()
                        if (release.creating) {
                            const rectangle = release.rectangle
                            const w = rectangle.width / window.viewDocScale()
                            const h = rectangle.height / window.viewDocScale()
                            if (w >= editorLimits.minimumBoxSize && h >= editorLimits.minimumBoxSize) Editor.createTextBox(window.viewToDocumentX(rectangle.x), window.viewToDocumentY(rectangle.y), w, h)
                        }
                    }
                    onCanvasWheel: (x, y, angleDeltaY) => window.zoomAt(x, y, angleDeltaY > 0 ? 1.1 : 1 / 1.1)

                    Image {
                        id: pageImage
                        x: window.documentToViewX(0)
                        y: window.documentToViewY(0)
                        width: window.pageDisplayWidth() * window.zoom
                        height: window.pageDisplayHeight() * window.zoom
                        source: Editor.currentPageUrl
                        fillMode: Image.Stretch
                        asynchronous: true
                        visible: source.toString().length > 0
                        onStatusChanged: if (status === Image.Ready) window.pageBaseScale = window.fitPageScale()
                    }

                    Image {
                        id: rawOverlayImage
                        x: pageImage.x
                        y: pageImage.y
                        width: pageImage.width
                        height: pageImage.height
                        source: Editor.rawPageUrl
                        fillMode: Image.Stretch
                        asynchronous: true
                        opacity: 0.45
                        visible: Editor.rawVisible && source.toString().length > 0
                    }

                    Rectangle {
                        visible: window.dragMode === editorInteraction.dragModeCreate
                        x: Math.min(window.createStartX, window.createCurrentX)
                        y: Math.min(window.createStartY, window.createCurrentY)
                        width: Math.abs(window.createCurrentX - window.createStartX)
                        height: Math.abs(window.createCurrentY - window.createStartY)
                        color: Qt.alpha(window.palette.highlight, 0.14)
                        border.color: window.palette.highlight
                        border.width: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        visible: window.dragMode === editorInteraction.dragModePathHandle && window.pathHandleInteractionActive
                        enabled: visible
                        z: 100
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        onPositionChanged: mouse => {
                            window.updatePathHandleDragFromCanvas(mouse.x, mouse.y)
                        }
                        onReleased: window.endPathHandleDrag()
                        onCanceled: window.endPathHandleDrag()
                    }

                    Repeater {
                        model: Editor.boxes
                        delegate: TextBoxDelegate {
                            canvasItem: canvas
                            interaction: editorInteraction
                        }
                    }
                }

                RightInspectorPanel {
                    id: rightPanel
                    objectName: "rightInspectorPanel"
                    editor: window.editor
                    editorLimits: editorLimits
                    selectedBoxProvider: () => window.selectedBox()
                    qmlColorProvider: hex => window.qmlColor(hex)
                    onColorDialogRequested: (hex, setter) => window.openColorDialog(hex, setter)
                }
            }
        }
    }
}
