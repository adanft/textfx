import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import TextFX.Ui 1.0

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: qsTr("TextFX")

    readonly property var editor: Editor
    property real zoom: 1.0
    property real pageBaseScale: 1.0
    property real panX: 0
    property real panY: 0
    property real pressX: 0
    property real pressY: 0
    property real pointerX: 0
    property real pointerY: 0
    property int dragMode: 0
    property real createStartX: 0
    property real createStartY: 0
    property real createCurrentX: 0
    property real createCurrentY: 0
    property int sidePanelFocusedTextInputs: 0
    property string colorDialogSetter: ""
    property var activeResizeDelegate: null
    property string activeResizeHandle: ""
    property real resizeStartX: 0
    property real resizeStartY: 0
    property real resizeStartW: 0
    property real resizeStartH: 0
    property real resizeStartRotation: 0
    property real resizeStartCanvasX: 0
    property real resizeStartCanvasY: 0
    property real resizeX: 0
    property real resizeY: 0
    property real resizeW: 0
    property real resizeH: 0
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

    component ColorButton: Button {
        id: colorButton
        property color swatchColor: "#000000"
        property string swatchText: "#000000"
        Layout.fillWidth: true
        contentItem: RowLayout {
            spacing: 8
            Rectangle { width: 22; height: 16; radius: 3; color: colorButton.enabled ? colorButton.swatchColor : colorButton.palette.mid; border.color: colorButton.palette.mid }
            Label { text: colorButton.swatchText; enabled: colorButton.enabled; Layout.fillWidth: true }
        }
    }

    component TextStyleButton: Button {
        id: textStyleButton
        property string accessibleLabel: ""
        property int buttonSize: 24
        checkable: true
        width: buttonSize
        height: buttonSize
        implicitWidth: buttonSize
        implicitHeight: buttonSize
        Layout.preferredWidth: buttonSize
        Layout.preferredHeight: buttonSize
        font.family: "Symbols Nerd Font"
        font.pixelSize: 18
        ToolTip.text: accessibleLabel
        ToolTip.visible: hovered
        Accessible.name: accessibleLabel
    }

    component ShortcutMenuItem: MenuItem {
        id: shortcutMenuItem

        property string shortcutLabel: ""

        contentItem: Item {
            implicitWidth: shortcutRow.implicitWidth
            implicitHeight: shortcutRow.implicitHeight

            RowLayout {
                id: shortcutRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: shortcutMenuItem.checkable ? (Math.max(shortcutMenuItem.indicator ? shortcutMenuItem.indicator.width : 0, shortcutMenuItem.indicator ? shortcutMenuItem.indicator.implicitWidth : 0, 20) + shortcutMenuItem.spacing) : 0
                spacing: 24

                Label {
                    text: shortcutMenuItem.text
                    enabled: shortcutMenuItem.enabled
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: shortcutMenuItem.shortcutLabel
                    visible: shortcutMenuItem.shortcutLabel.length > 0
                    enabled: shortcutMenuItem.enabled
                    color: shortcutMenuItem.palette.mid
                }
            }
        }
    }

    function fitPageScale() {
        const sourceWidth = pageImage.sourceSize.width
        const sourceHeight = pageImage.sourceSize.height
        if (sourceWidth <= 0 || sourceHeight <= 0 || canvas.width <= 0 || canvas.height <= 0)
            return 1
        return Math.min(canvas.width / sourceWidth, canvas.height / sourceHeight)
    }

    function pageScale() { return pageBaseScale }
    function pageDisplayWidth() { return pageImage.sourceSize.width * pageScale() }
    function pageDisplayHeight() { return pageImage.sourceSize.height * pageScale() }
    function pageLeft() { return (canvas.width - pageDisplayWidth()) / 2 }
    function pageTop() { return (canvas.height - pageDisplayHeight()) / 2 }
    function viewDocScale() { return pageScale() * window.zoom }
    function documentToViewLength(value) { return value * viewDocScale() }
    function selectionLineWidth() { return Math.max(1, documentToViewLength(2)) }
    function handleSize() { return Math.max(1, documentToViewLength(12)) }
    function rotateHandleDistance() { return documentToViewLength(28) }
    function documentToViewX(x) { return window.panX + (pageLeft() + x * pageScale()) * window.zoom }
    function documentToViewY(y) { return window.panY + (pageTop() + y * pageScale()) * window.zoom }
    function viewToDocumentX(x) { return (x - window.panX) / viewDocScale() - pageLeft() / pageScale() }
    function viewToDocumentY(y) { return (y - window.panY) / viewDocScale() - pageTop() / pageScale() }

    function zoomAt(x, y, factor) {
        const oldZoom = window.zoom
        const nextZoom = Math.max(0.5, Math.min(6, oldZoom * factor))
        if (nextZoom === oldZoom)
            return
        window.panX = x - (x - window.panX) * (nextZoom / oldZoom)
        window.panY = y - (y - window.panY) * (nextZoom / oldZoom)
        window.zoom = nextZoom
    }

    function resetZoom() {
        window.zoom = 1.0
        window.panX = 0
        window.panY = 0
    }

    function toast(message) {
        toastLabel.text = message
        toastPopup.open()
        toastTimer.restart()
    }

    function localPathFromUrl(url) {
        const value = decodeURIComponent(String(url))
        if (value.startsWith("file://"))
            return value.slice(7)
        return value
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

    function dialogColorHex(colorValue) {
        const value = String(colorValue)
        if (/^#[0-9a-fA-F]{8}$/.test(value))
            return "#" + value.slice(3)
        return qmlColor(value)
    }

    function openColorDialog(hex, setter) {
        colorDialog.selectedColor = hex
        colorDialogSetter = setter
        colorDialog.open()
    }

    function applyColorDialogSelection(hex) {
        if (colorDialogSetter === "text") Editor.setSelectedTextColor(hex)
        else if (colorDialogSetter === "outline") Editor.setSelectedOutlineColor(hex)
        else if (colorDialogSetter === "shadow") Editor.setSelectedShadowColor(hex)
        else if (colorDialogSetter === "gradientA") Editor.setSelectedGradientColorA(hex)
        else if (colorDialogSetter === "gradientB") Editor.setSelectedGradientColorB(hex)
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
        if (dragMode !== 5 || !box || box.index !== Editor.selectedIndex)
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

    function boxHasRenderEffects(box) {
        return box && (box.outline || box.blur || box.shadow || box.gradient || box.perspective || box.path)
    }

    function selectedBoxHasRenderEffects() {
        const box = selectedBox()
        return boxHasRenderEffects(box)
    }

    function rotatePoint(x, y, radians) {
        const c = Math.cos(radians)
        const s = Math.sin(radians)
        return { x: x * c - y * s, y: x * s + y * c }
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
        activeResizeDelegate = delegate
        activeResizeHandle = handle
        dragMode = 4
        resizeStartX = delegate.boxModel.x
        resizeStartY = delegate.boxModel.y
        resizeStartW = delegate.boxModel.w
        resizeStartH = delegate.boxModel.h
        resizeStartRotation = delegate.boxModel.rotation * Math.PI / 180
        resizeStartCanvasX = canvasX
        resizeStartCanvasY = canvasY
        resizeX = resizeStartX
        resizeY = resizeStartY
        resizeW = resizeStartW
        resizeH = resizeStartH
    }

    function updateResizeDrag(canvasX, canvasY) {
        if (dragMode !== 4 || !activeResizeDelegate)
            return
        const delta = rotatePoint((canvasX - resizeStartCanvasX) / viewDocScale(),
                                  (canvasY - resizeStartCanvasY) / viewDocScale(),
                                  -resizeStartRotation)
        const pivotX = resizeStartW / 2
        const pivotY = resizeStartH / 2
        let left = 0
        let top = 0
        let right = resizeStartW
        let bottom = resizeStartH
        if (activeResizeHandle.indexOf("w") >= 0) left = Math.min(delta.x, right - 12)
        else if (activeResizeHandle.indexOf("e") >= 0) right = Math.max(resizeStartW + delta.x, left + 12)
        if (activeResizeHandle.indexOf("n") >= 0) top = Math.min(delta.y, bottom - 12)
        else if (activeResizeHandle.indexOf("s") >= 0) bottom = Math.max(resizeStartH + delta.y, top + 12)
        resizeW = right - left
        resizeH = bottom - top
        const newPivotX = resizeW / 2
        const newPivotY = resizeH / 2
        const moved = rotatePoint(left - pivotX + newPivotX, top - pivotY + newPivotY, resizeStartRotation)
        resizeX = resizeStartX + pivotX + moved.x - newPivotX
        resizeY = resizeStartY + pivotY + moved.y - newPivotY
    }

    function endResizeDrag(commit) {
        if (dragMode === 4 && commit)
            window.editor.setSelectedBounds(resizeX, resizeY, resizeW, resizeH)
        activeResizeDelegate = null
        activeResizeHandle = ""
        dragMode = 0
    }

    function beginPerspectiveDrag(delegate, handle, canvasX, canvasY) {
        activePerspectiveDelegate = delegate
        activePerspectiveHandle = handle
        dragMode = 5
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
        if (dragMode !== 5 || !activePerspectiveDelegate)
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
        if (dragMode === 5 && commit) {
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
        dragMode = 0
    }

    function beginRotateDrag(delegate, canvasX, canvasY) {
        activeRotateDelegate = delegate
        dragMode = 6
        const cx = delegate.boxModel.x + delegate.boxModel.w / 2
        const cy = delegate.boxModel.y + delegate.boxModel.h / 2
        rotateStartRotation = delegate.boxModel.rotation
        rotateDegrees = rotateStartRotation
        rotateStartAngle = angleDegrees(cx, cy, viewToDocumentX(canvasX), viewToDocumentY(canvasY))
    }

    function updateRotateDrag(canvasX, canvasY) {
        if (dragMode !== 6 || !activeRotateDelegate)
            return
        const box = activeRotateDelegate.boxModel
        const cx = box.x + box.w / 2
        const cy = box.y + box.h / 2
        const angle = angleDegrees(cx, cy, viewToDocumentX(canvasX), viewToDocumentY(canvasY))
        rotateDegrees = rotateStartRotation + angleDeltaDegrees(rotateStartAngle, angle)
        activeRotateDelegate.rotation = rotateDegrees
    }

    function endRotateDrag(commit) {
        if (dragMode === 6 && commit)
            window.editor.setSelectedRotation(rotateDegrees)
        else if (dragMode === 6 && activeRotateDelegate)
            activeRotateDelegate.rotation = rotateStartRotation
        activeRotateDelegate = null
        dragMode = 0
    }

    function beginMoveDrag(delegate, index, canvasX, canvasY) {
        activeMoveDelegate = delegate
        activeMoveIndex = index
        dragMode = 7
        moveStartX = delegate.boxModel.x
        moveStartY = delegate.boxModel.y
        moveStartCanvasX = canvasX
        moveStartCanvasY = canvasY
        moveX = moveStartX
        moveY = moveStartY
    }

    function updateMoveDrag(canvasX, canvasY) {
        if (dragMode !== 7 || !activeMoveDelegate)
            return
        moveX = moveStartX + (canvasX - moveStartCanvasX) / viewDocScale()
        moveY = moveStartY + (canvasY - moveStartCanvasY) / viewDocScale()
    }

    function endMoveDrag(commit) {
        if (dragMode === 7 && commit)
            window.editor.moveSelected(moveX - moveStartX, moveY - moveStartY)
        activeMoveDelegate = null
        activeMoveIndex = -1
        dragMode = 0
    }

    function handleEscape() {
        if (Editor.editingText) {
            Editor.endTextEdit()
            canvas.forceActiveFocus()
        } else if (dragMode !== 0) {
            if (dragMode === 4)
                endResizeDrag(false)
            else if (dragMode === 5)
                endPerspectiveDrag(false)
            else if (dragMode === 6)
                endRotateDrag(false)
            else if (dragMode === 7)
                endMoveDrag(false)
            dragMode = 0
        } else if (Editor.selectedIndex >= 0) {
            Editor.selectBox(-1)
        }
    }

    Connections {
        target: Editor
        function onNotificationChanged() { if (Editor.notification.length > 0) window.toast(Editor.notification) }
    }

    Action { id: openAction; text: qsTr("Open"); shortcut: StandardKey.Open; enabled: Editor.actionEnabled("open"); onTriggered: openProjectDialog.open() }
    Action { id: saveAction; text: qsTr("Save"); shortcut: StandardKey.Save; enabled: Editor.hasProject; onTriggered: Editor.save() }
    Action { id: saveAllAction; text: qsTr("Save All"); shortcut: "Ctrl+Shift+S"; enabled: Editor.hasProject; onTriggered: Editor.saveAll() }
    Action { id: previousPageAction; text: qsTr("Previous"); shortcut: "PgUp"; enabled: Editor.canGoPrevious; onTriggered: Editor.previousPage() }
    Action { id: nextPageAction; text: qsTr("Next"); shortcut: "PgDown"; enabled: Editor.canGoNext; onTriggered: Editor.nextPage() }
    Action { id: copyAction; text: qsTr("Copy"); shortcut: StandardKey.Copy; enabled: Editor.actionEnabled("copy") && !Editor.editingText; onTriggered: Editor.copySelected() }
    Action { id: pasteAction; text: qsTr("Paste"); shortcut: StandardKey.Paste; enabled: Editor.actionEnabled("paste") && !Editor.editingText; onTriggered: Editor.pasteBox() }
    Action { id: duplicateAction; text: qsTr("Duplicate"); enabled: Editor.actionEnabled("duplicate") && !Editor.editingText; onTriggered: Editor.duplicateSelected() }
    Action { id: deleteAction; text: qsTr("Delete"); shortcut: StandardKey.Delete; enabled: Editor.actionEnabled("delete") && !Editor.editingText; onTriggered: Editor.deleteSelected() }
    Action { id: quitAction; text: qsTr("Quit"); onTriggered: Qt.quit() }
    Action { id: zoomInAction; text: qsTr("Zoom In"); shortcut: "Ctrl++"; onTriggered: window.zoomAt(canvas.width / 2, canvas.height / 2, 1.1) }
    Action { id: zoomOutAction; text: qsTr("Zoom Out"); shortcut: "Ctrl+-"; onTriggered: window.zoomAt(canvas.width / 2, canvas.height / 2, 1 / 1.1) }
    Action { id: resetZoomAction; text: qsTr("Reset Zoom"); shortcut: "Ctrl+0"; onTriggered: window.resetZoom() }
    Action { id: rawOverlayAction; text: qsTr("Show Raw Overlay"); checkable: true; checked: Editor.rawVisible; shortcut: "Ctrl+H"; onTriggered: Editor.rawVisible = checked }

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            ShortcutMenuItem { action: openAction; shortcutLabel: "Ctrl+O" }
            ShortcutMenuItem { action: saveAction; shortcutLabel: "Ctrl+S" }
            ShortcutMenuItem { action: saveAllAction; shortcutLabel: "Ctrl+Shift+S" }
            MenuSeparator {}
            ShortcutMenuItem { action: quitAction }
        }
        Menu {
            title: qsTr("Edit")
            ShortcutMenuItem { action: copyAction; shortcutLabel: "Ctrl+C" }
            ShortcutMenuItem { action: pasteAction; shortcutLabel: "Ctrl+V" }
            ShortcutMenuItem { action: deleteAction; shortcutLabel: "Del" }
            ShortcutMenuItem { action: duplicateAction }
        }
        Menu {
            title: qsTr("View")
            ShortcutMenuItem { action: zoomInAction; shortcutLabel: "Ctrl++" }
            ShortcutMenuItem { action: zoomOutAction; shortcutLabel: "Ctrl+-" }
            ShortcutMenuItem { action: resetZoomAction; shortcutLabel: "Ctrl+0" }
            MenuSeparator {}
            ShortcutMenuItem { action: rawOverlayAction; shortcutLabel: "Ctrl+H" }
        }
    }

    Shortcut { sequence: "Ctrl+="; onActivated: zoomInAction.trigger() }
    Shortcut { sequence: "Ctrl+Space"; enabled: !Editor.editingText && window.sidePanelFocusedTextInputs === 0; onActivated: Editor.applyNextPageText() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: window.handleEscape() }

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

                Pane {
                    id: sidePanel
                    z: 1
                    SplitView.minimumWidth: 240
                    SplitView.preferredWidth: 280
                    SplitView.fillHeight: true

                    ScrollView {
                        id: leftPanelScroll
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: leftPanelScroll.availableWidth
                            // Keep the layout at least viewport-tall so Page Texts can consume spare vertical space.
                            height: Math.max(implicitHeight, leftPanelScroll.availableHeight)
                            spacing: 8

                             ColumnLayout {
                                id: textPropertiesSection

                                readonly property bool sectionReady: Editor.selectedIndex >= 0

                                 Layout.fillWidth: true
                                 Layout.minimumWidth: 0
                                enabled: sectionReady

                                Label { text: qsTr("Text Properties"); font.bold: true; enabled: textPropertiesSection.sectionReady }

                                 GroupBox {
                                     Layout.fillWidth: true
                                     Layout.minimumWidth: 0
                                     enabled: textPropertiesSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Label { text: qsTr("Font Family") }
                                        ComboBox {
                                            id: fontFamilyCombo
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            model: window.fontFamilyOptions(window.selectedBox() ? window.selectedBox().fontFamily : "")
                                            currentIndex: Math.max(0, indexOfValue(window.selectedBox() ? window.selectedBox().fontFamily : ""))
                                            contentItem: Label {
                                                text: fontFamilyCombo.displayText
                                                font: fontFamilyCombo.font
                                                enabled: fontFamilyCombo.enabled
                                                elide: Text.ElideRight
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onActivated: Editor.setSelectedFontFamily(currentText)
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 8
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                Label { text: qsTr("Size") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: 1; to: 512; value: window.selectedBox() ? window.selectedBox().fontSize : 16; onValueModified: Editor.setSelectedFontSize(value) }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                Label { text: qsTr("Leading") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: -100; to: 300; value: window.selectedBox() ? window.selectedBox().lineSpacing : 0; onValueModified: Editor.setSelectedLineSpacing(value) }
                                            }
                                        }

                                        Label { text: qsTr("Tracking") }
                                        SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: -100; to: 300; value: window.selectedBox() ? window.selectedBox().letterSpacing : 0; onValueModified: Editor.setSelectedLetterSpacing(value) }

                                        Label { text: qsTr("Text Color") }
                                        ColorButton { swatchText: window.selectedBox() ? window.qmlColor(window.selectedBox().color) : "#000000"; swatchColor: swatchText; onClicked: window.openColorDialog(swatchText, "text") }

                                        Label { text: qsTr("Style") }
                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 6
                                            TextStyleButton { text: String.fromCodePoint(0xf0264); accessibleLabel: qsTr("Bold"); checked: window.selectedBox() ? window.selectedBox().bold : false; onClicked: Editor.setSelectedBold(checked) }
                                            TextStyleButton { text: String.fromCodePoint(0xf0277); accessibleLabel: qsTr("Italic"); checked: window.selectedBox() ? window.selectedBox().italic : false; onClicked: Editor.setSelectedItalic(checked) }
                                            TextStyleButton { text: String.fromCodePoint(0xf0b36); accessibleLabel: qsTr("Uppercase"); checked: window.selectedBox() ? window.selectedBox().uppercase : false; onClicked: Editor.setSelectedUppercase(checked) }
                                        }

                                        Label { text: qsTr("Paragraph") }
                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 6
                                            TextStyleButton { text: String.fromCodePoint(0xf0262); accessibleLabel: qsTr("Align Left"); checked: window.selectedBox() ? window.selectedBox().alignment === 0 : false; onClicked: Editor.setSelectedAlignment(0) }
                                            TextStyleButton { text: String.fromCodePoint(0xf0260); accessibleLabel: qsTr("Align Center"); checked: window.selectedBox() ? window.selectedBox().alignment === 1 : false; onClicked: Editor.setSelectedAlignment(1) }
                                            TextStyleButton { text: String.fromCodePoint(0xf0263); accessibleLabel: qsTr("Align Right"); checked: window.selectedBox() ? window.selectedBox().alignment === 2 : false; onClicked: Editor.setSelectedAlignment(2) }
                                        }
                                    }
                                }
                            }

                             ColumnLayout {
                                id: textPresetsSection

                                readonly property bool sectionReady: Editor.hasProject && Editor.selectedIndex >= 0

                                 Layout.fillWidth: true
                                 Layout.minimumWidth: 0
                                enabled: sectionReady

                                Label { text: qsTr("Text Presets"); font.bold: true; enabled: textPresetsSection.sectionReady }

                                 GroupBox {
                                     Layout.fillWidth: true
                                     Layout.minimumWidth: 0
                                     enabled: textPresetsSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            ComboBox {
                                                id: presetSelect
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                model: Editor.presets
                                                textRole: "name"
                                                currentIndex: Editor.selectedPresetIndex >= 0 ? Editor.selectedPresetIndex : (Editor.presets.length > 0 ? 0 : -1)
                                                contentItem: Label {
                                                    text: presetSelect.displayText
                                                    font: presetSelect.font
                                                    enabled: presetSelect.enabled
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                onActivated: index => Editor.selectPreset(index)
                                            }
                                            Button { text: qsTr("Apply"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0; onClicked: Editor.applySelectedPreset() }
                                        }
                                        TextField { id: presetNameField; Layout.fillWidth: true; Layout.minimumWidth: 0; placeholderText: qsTr("Preset name"); enabled: Editor.hasProject && Editor.selectedIndex >= 0; onActiveFocusChanged: window.noteSidePanelTextInputFocus(activeFocus) }
                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 6
                                            Button { text: qsTr("Create"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && presetNameField.text.trim().length > 0; onClicked: { Editor.addPreset(presetNameField.text); presetNameField.text = "" } }
                                            Button { text: qsTr("Update"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0; onClicked: Editor.updateSelectedPreset() }
                                            Button { text: qsTr("Rename"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0 && presetNameField.text.trim().length > 0 && !(Editor.presets[Editor.selectedPresetIndex] || {}).isDefault; onClicked: { Editor.renameSelectedPreset(presetNameField.text); presetNameField.text = "" } }
                                            Button { text: qsTr("Delete"); enabled: Editor.hasProject && Editor.selectedIndex >= 0 && Editor.selectedPresetIndex >= 0 && !(Editor.presets[Editor.selectedPresetIndex] || {}).isDefault; onClicked: Editor.deleteSelectedPreset() }
                                        }
                                    }
                                }
                            }

                             ColumnLayout {
                                id: pageTextsSection

                                readonly property bool sectionReady: Editor.hasProject && Editor.pageTexts.length > 0

                                 Layout.fillWidth: true
                                 Layout.minimumWidth: 0
                                 Layout.fillHeight: true
                                enabled: sectionReady

                                 RowLayout {
                                     Layout.fillWidth: true
                                     Layout.minimumWidth: 0
                                     Label { text: qsTr("Page Texts"); font.bold: true; enabled: pageTextsSection.sectionReady }
                                     Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; text: Editor.pageTexts.length; enabled: pageTextsSection.sectionReady }
                                 }

                                 Frame {
                                    id: pageTextsFrame

                                    readonly property real minimumListHeight: 200

                                     Layout.fillWidth: true
                                       Layout.minimumWidth: 0
                                     Layout.minimumHeight: minimumListHeight + topPadding + bottomPadding
                                       Layout.fillHeight: true
                                      enabled: pageTextsSection.sectionReady
                                      background: Rectangle { color: pageTextsFrame.palette.base; border.color: pageTextsFrame.palette.mid; border.width: 1 }

                                      ListView {
                                         id: pageTextsList

                                         anchors.fill: parent
                                         Layout.fillWidth: true
                                         Layout.fillHeight: true
                                        Layout.preferredHeight: pageTextsFrame.minimumListHeight
                                          clip: true
                                          model: Editor.pageTexts
                                         delegate: ItemDelegate {
                                             id: pageTextDelegate
                                             width: ListView.view.width
                                             hoverEnabled: true
                                             text: modelData
                                             contentItem: Label {
                                                 text: pageTextDelegate.text
                                                 font: pageTextDelegate.font
                                                 enabled: pageTextDelegate.enabled
                                                 elide: Text.ElideRight
                                                 verticalAlignment: Text.AlignVCenter
                                             }
                                             background: Rectangle { color: pageTextDelegate.down || pageTextDelegate.hovered ? Qt.alpha(pageTextDelegate.palette.highlight, pageTextDelegate.down ? 0.24 : 0.14) : "transparent" }
                                             onClicked: Editor.applyPageText(index)
                                         }
                                     }
                                 }
                            }
                        }
                    }
                }

                Item {
                    id: canvasSlot
                    z: 0
                    SplitView.minimumWidth: 240
                    SplitView.fillWidth: true
                    SplitView.fillHeight: true

                    Rectangle {
                        id: canvas
                        z: 0
                        x: -canvasSlot.x
                        width: mainSplit.width
                        height: canvasSlot.height
                        color: window.palette.base
                        clip: true
                        focus: true
                        property bool effectsPreviewDisplayable: Editor.effectsPreviewActive && effectsPreviewImage.readySource === Editor.previewImageUrl.toString() && effectsPreviewImage.readySource.length > 0
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Escape) { window.handleEscape(); event.accepted = true; return }
                            if (Editor.editingText) return
                            if (event.key === Qt.Key_Delete) { Editor.deleteSelected(); event.accepted = true }
                        }

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
                        id: effectsPreviewImage
                        x: pageImage.x
                        y: pageImage.y
                        width: pageImage.width
                        height: pageImage.height
                        source: Editor.previewImageUrl
                        fillMode: Image.Stretch
                        asynchronous: true
                        cache: false
                        property string readySource: ""
                        onSourceChanged: readySource = ""
                        onStatusChanged: readySource = status === Image.Ready ? source.toString() : ""
                        visible: canvas.effectsPreviewDisplayable
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

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        onPressed: mouse => {
                            canvas.forceActiveFocus()
                            if (mouse.button === Qt.LeftButton)
                                Editor.endTextEdit()
                            window.pressX = mouse.x
                            window.pressY = mouse.y
                            if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                                window.dragMode = 3
                                window.createStartX = mouse.x
                                window.createStartY = mouse.y
                                window.createCurrentX = mouse.x
                                window.createCurrentY = mouse.y
                            } else {
                                window.dragMode = mouse.button === Qt.LeftButton || mouse.button === Qt.MiddleButton || mouse.button === Qt.RightButton ? 2 : 0
                            }
                        }
                        onPositionChanged: mouse => {
                            window.pointerX = mouse.x
                            window.pointerY = mouse.y
                            if (!pressed) return
                            const dx = mouse.x - window.pressX
                            const dy = mouse.y - window.pressY
                            if (window.dragMode === 2) { window.panX += dx; window.panY += dy }
                            else if (window.dragMode === 3) { window.createCurrentX = mouse.x; window.createCurrentY = mouse.y }
                            window.pressX = mouse.x
                            window.pressY = mouse.y
                        }
                        onReleased: mouse => {
                            if (window.dragMode === 3) {
                                const left = Math.min(window.createStartX, window.createCurrentX)
                                const top = Math.min(window.createStartY, window.createCurrentY)
                                const right = Math.max(window.createStartX, window.createCurrentX)
                                const bottom = Math.max(window.createStartY, window.createCurrentY)
                                const w = (right - left) / window.viewDocScale()
                                const h = (bottom - top) / window.viewDocScale()
                                if (w >= 12 && h >= 12) Editor.createTextBox(window.viewToDocumentX(left), window.viewToDocumentY(top), w, h)
                            }
                            window.dragMode = 0
                        }
                        onWheel: wheel => {
                            window.zoomAt(wheel.x, wheel.y, wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1)
                            wheel.accepted = true
                        }
                    }

                    Rectangle {
                        visible: window.dragMode === 3
                        x: Math.min(window.createStartX, window.createCurrentX)
                        y: Math.min(window.createStartY, window.createCurrentY)
                        width: Math.abs(window.createCurrentX - window.createStartX)
                        height: Math.abs(window.createCurrentY - window.createStartY)
                        color: Qt.alpha(window.palette.highlight, 0.14)
                        border.color: window.palette.highlight
                        border.width: 1
                    }

                    Repeater {
                        model: Editor.boxes
                        delegate: Rectangle {
                            id: boxDelegate

                            readonly property var rootWindow: ApplicationWindow.window
                            readonly property var editorRef: rootWindow ? rootWindow.editor : null
                            property bool selected: editorRef && modelData.index === editorRef.selectedIndex
                            property bool editingSelected: selected && editorRef.editingText
                            property var boxModel: modelData
                            property bool perspectiveActive: boxModel.perspective && !editingSelected
                            property bool moveActive: rootWindow.dragMode === 7 && rootWindow.activeMoveIndex === modelData.index
                            property bool resizeActive: rootWindow.dragMode === 4 && rootWindow.activeResizeDelegate === boxDelegate
                            property real visualDocX: moveActive ? rootWindow.moveX : resizeActive ? rootWindow.resizeX : modelData.x
                            property real visualDocY: moveActive ? rootWindow.moveY : resizeActive ? rootWindow.resizeY : modelData.y
                            property real visualDocW: resizeActive ? rootWindow.resizeW : modelData.w
                            property real visualDocH: resizeActive ? rootWindow.resizeH : modelData.h
                            x: rootWindow.documentToViewX(visualDocX)
                            y: rootWindow.documentToViewY(visualDocY)
                            width: visualDocW * rootWindow.viewDocScale()
                            height: visualDocH * rootWindow.viewDocScale()
                            color: "transparent"
                            border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))
                            border.color: editingSelected ? Qt.rgba(1, 0.84, 0, 1) : selected ? rootWindow.palette.highlight : rootWindow.palette.mid
                            rotation: modelData.rotation

                            Canvas {
                                id: perspectiveBorder

                                property var boxRef: parent
                                property var rootWindow: boxRef.rootWindow
                                property var editorRef: boxRef.editorRef
                                property real margin: rootWindow.perspectiveMargin(boxRef.boxModel)
                                x: -margin
                                y: -margin
                                width: parent.width + margin * 2
                                height: parent.height + margin * 2
                                visible: boxRef.selected && boxRef.perspectiveActive
                                z: 19
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    const nw = rootWindow.perspectiveCorner(boxRef.boxModel, "nw", boxRef.width, boxRef.height)
                                    const ne = rootWindow.perspectiveCorner(boxRef.boxModel, "ne", boxRef.width, boxRef.height)
                                    const se = rootWindow.perspectiveCorner(boxRef.boxModel, "se", boxRef.width, boxRef.height)
                                    const sw = rootWindow.perspectiveCorner(boxRef.boxModel, "sw", boxRef.width, boxRef.height)
                                    ctx.beginPath()
                                    ctx.moveTo(nw.x + margin, nw.y + margin)
                                    ctx.lineTo(ne.x + margin, ne.y + margin)
                                    ctx.lineTo(se.x + margin, se.y + margin)
                                    ctx.lineTo(sw.x + margin, sw.y + margin)
                                    ctx.closePath()
                                    ctx.lineWidth = rootWindow.selectionLineWidth()
                                    ctx.strokeStyle = rootWindow.palette.highlight
                                    ctx.stroke()
                                }
                                Connections {
                                    target: perspectiveBorder.rootWindow
                                    function onZoomChanged() { perspectiveBorder.requestPaint() }
                                    function onPerspectiveRevisionChanged() { perspectiveBorder.requestPaint() }
                                }
                                Connections {
                                    target: perspectiveBorder.editorRef
                                    function onDocumentChanged() { perspectiveBorder.requestPaint() }
                                }
                            }

                            Item {
                                id: boxTextPerspective

                                property var boxRef: parent
                                property var rootWindow: boxRef.rootWindow
                                property var editorRef: boxRef.editorRef
                                z: 1
                                anchors.fill: parent
                                transform: Matrix4x4 { matrix: boxTextPerspective.rootWindow.perspectiveMatrix(boxTextPerspective.boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, boxTextPerspective.rootWindow.viewDocScale(), boxTextPerspective.boxRef.perspectiveActive) }

                                OutlinedTextItem {
                                    id: boxOutlinedText

                                    property var boxRef: boxTextPerspective.boxRef
                                    property var rootWindow: boxTextPerspective.rootWindow
                                    property var editorRef: boxTextPerspective.editorRef
                                    anchors.fill: parent
                                    visible: !canvas.effectsPreviewDisplayable || (boxRef.selected && editorRef.editingText)
                                    text: boxRef.selected && editorRef.editingText ? boxTextArea.text : (modelData.uppercase ? String(modelData.text).toUpperCase() : modelData.text)
                                    color: rootWindow.qmlColor(modelData.color)
                                    fontFamily: modelData.fontFamily
                                    pixelSize: Math.max(1, modelData.fontSize)
                                    bold: modelData.bold
                                    italic: modelData.italic
                                    letterSpacing: modelData.letterSpacing
                                    lineSpacing: modelData.lineSpacing
                                    horizontalAlignment: modelData.alignment === 1 ? Text.AlignHCenter : modelData.alignment === 2 ? Text.AlignRight : Text.AlignLeft
                                    outlineColor: rootWindow.qmlColor(modelData.outlineColor)
                                    outlineSize: modelData.outline && modelData.outlineSize > 0 ? modelData.outlineSize : 0
                                    renderScale: rootWindow.viewDocScale()
                                }
                            }

                            TextArea {
                                id: boxTextArea
                                objectName: "boxTextArea"

                                property var boxRef: parent
                                property var rootWindow: boxRef.rootWindow
                                property var editorRef: boxRef.editorRef
                                function focusForEdit() {
                                    if (boxRef.selected && editorRef.editingText && !activeFocus)
                                        forceActiveFocus()
                                }
                                z: 1
                                anchors.fill: parent
                                visible: boxRef.selected && editorRef.editingText
                                text: modelData.uppercase ? String(modelData.text).toUpperCase() : modelData.text
                                color: "transparent"
                                selectedTextColor: "transparent"
                                selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)
                                placeholderTextColor: "transparent"
                                font.family: modelData.resolvedFontFamily
                                font.pixelSize: Math.max(1, modelData.fontSize * rootWindow.viewDocScale())
                                font.bold: modelData.bold
                                font.weight: modelData.bold ? Font.Bold : Font.Normal
                                font.italic: modelData.italic
                                font.letterSpacing: modelData.letterSpacing * rootWindow.viewDocScale()
                                // Qt Quick TextArea/TextEdit in the supported runtime does not expose lineHeight;
                                // modelData.lineSpacing remains render-only to avoid assigning unsupported QML properties.
                                horizontalAlignment: modelData.alignment === 1 ? TextEdit.AlignHCenter : modelData.alignment === 2 ? TextEdit.AlignRight : TextEdit.AlignLeft
                                padding: 0
                                topPadding: 0
                                leftPadding: 0
                                rightPadding: 0
                                bottomPadding: 0
                                background: null
                                cursorDelegate: Rectangle {
                                    width: 2
                                    color: boxTextArea.rootWindow.palette.highlight
                                    SequentialAnimation on opacity {
                                        loops: Animation.Infinite
                                        running: boxTextArea.visible && boxTextArea.activeFocus
                                        NumberAnimation { to: 0; duration: 450 }
                                        NumberAnimation { to: 1; duration: 450 }
                                    }
                                }
                                wrapMode: TextEdit.Wrap
                                selectByMouse: boxRef.selected && editorRef.editingText
                                readOnly: !(boxRef.selected && editorRef.editingText)
                                Component.onCompleted: Qt.callLater(focusForEdit)
                                onVisibleChanged: if (visible) Qt.callLater(focusForEdit)
                                Keys.onPressed: event => {
                                    if (event.key === Qt.Key_Escape) { rootWindow.handleEscape(); event.accepted = true }
                                }
                                onActiveFocusChanged: if (editorRef) activeFocus ? editorRef.beginTextEdit() : editorRef.endTextEdit()
                                onTextChanged: if (activeFocus && editorRef) editorRef.updateSelectedText(text)
                            }

                            MouseArea {
                                property var boxRef: parent
                                property var rootWindow: boxRef.rootWindow
                                property var editorRef: boxRef.editorRef
                                z: boxRef.selected && editorRef.editingText ? -1 : 10
                                readonly property var visualBounds: boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds(boxRef.boxModel, boxRef.width, boxRef.height) : ({ x: 0, y: 0, width: boxRef.width, height: boxRef.height })
                                x: visualBounds.x
                                y: visualBounds.y
                                width: visualBounds.width
                                height: visualBounds.height
                                acceptedButtons: Qt.LeftButton
                                preventStealing: true
                                enabled: !(boxRef.selected && editorRef.editingText)
                                onPressed: mouse => {
                                    const local = mapToItem(boxRef, mouse.x, mouse.y)
                                    if (boxRef.perspectiveActive && !rootWindow.pointInPerspectivePolygon(local, boxRef.boxModel, boxRef.width, boxRef.height)) {
                                        mouse.accepted = false
                                        return
                                    }
                                    mouse.accepted = true
                                    canvas.forceActiveFocus()
                                    editorRef.selectBox(boxRef.boxModel.index)
                                    editorRef.beginInteraction()
                                    const point = mapToItem(canvas, mouse.x, mouse.y)
                                    rootWindow.beginMoveDrag(boxRef, boxRef.boxModel.index, point.x, point.y)
                                }
                                onPositionChanged: mouse => {
                                    if (!pressed || boxRef.boxModel.index !== editorRef.selectedIndex) return
                                    const point = mapToItem(canvas, mouse.x, mouse.y)
                                    rootWindow.updateMoveDrag(point.x, point.y)
                                }
                                onReleased: { rootWindow.endMoveDrag(true); editorRef.endInteraction() }
                                onCanceled: { rootWindow.endMoveDrag(false); editorRef.endInteraction() }
                                onDoubleClicked: mouse => {
                                    editorRef.selectBox(boxRef.boxModel.index)
                                    editorRef.beginTextEdit()
                                    boxTextArea.forceActiveFocus()
                                }
                            }

                            Repeater {
                                model: [
                                    {name: "nw"}, {name: "n"}, {name: "ne"}, {name: "e"},
                                    {name: "se"}, {name: "s"}, {name: "sw"}, {name: "w"}
                                ]
                                delegate: Rectangle {
                                    id: resizeHandle

                                    property var boxRef: parent
                                    property var rootWindow: boxRef.rootWindow
                                    property var editorRef: boxRef.editorRef
                                    z: 20
                                    width: rootWindow.handleSize(); height: width; radius: width / 2; color: rootWindow.palette.highlight
                                    x: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).x - width / 2
                                    y: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).y - height / 2
                                    visible: boxRef.selected
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton
                                        preventStealing: true
                                        cursorShape: Qt.SizeFDiagCursor
                                        onPressed: mouse => {
                                            mouse.accepted = true
                                            resizeHandle.editorRef.selectBox(resizeHandle.boxRef.boxModel.index)
                                            resizeHandle.editorRef.beginInteraction()
                                            const point = mapToItem(canvas, mouse.x, mouse.y)
                                            if (resizeHandle.boxRef.perspectiveActive)
                                                resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)
                                            else
                                                resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y)
                                        }
                                        onPositionChanged: mouse => {
                                            if (!pressed) return
                                            const point = mapToItem(canvas, mouse.x, mouse.y)
                                            if (resizeHandle.boxRef.perspectiveActive) {
                                                resizeHandle.rootWindow.updatePerspectiveDrag(point.x, point.y)
                                                return
                                            }
                                            resizeHandle.rootWindow.updateResizeDrag(point.x, point.y)
                                        }
                                        onReleased: {
                                            if (resizeHandle.boxRef.perspectiveActive)
                                                resizeHandle.rootWindow.endPerspectiveDrag(true)
                                            else
                                                resizeHandle.rootWindow.endResizeDrag(true)
                                            resizeHandle.editorRef.endInteraction()
                                        }
                                        onCanceled: {
                                            if (resizeHandle.boxRef.perspectiveActive)
                                                resizeHandle.rootWindow.endPerspectiveDrag(false)
                                            else
                                                resizeHandle.rootWindow.endResizeDrag(false)
                                            resizeHandle.editorRef.endInteraction()
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                id: rotateHandle

                                property var boxRef: parent
                                property var rootWindow: boxRef.rootWindow
                                property var editorRef: boxRef.editorRef
                                z: 20
                                width: rootWindow.handleSize(); height: width; radius: width / 2; color: rootWindow.palette.highlight
                                x: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).x - width / 2
                                y: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).y - height / 2
                                visible: boxRef.selected
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    preventStealing: true
                                    onPressed: mouse => {
                                        mouse.accepted = true
                                        rotateHandle.editorRef.selectBox(rotateHandle.boxRef.boxModel.index)
                                        rotateHandle.editorRef.beginInteraction()
                                        const point = mapToItem(canvas, mouse.x, mouse.y)
                                        rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef, point.x, point.y)
                                    }
                                    onPositionChanged: mouse => {
                                        if (!pressed) return
                                        const point = mapToItem(canvas, mouse.x, mouse.y)
                                        rotateHandle.rootWindow.updateRotateDrag(point.x, point.y)
                                    }
                                    onReleased: { rotateHandle.rootWindow.endRotateDrag(true); rotateHandle.editorRef.endInteraction() }
                                    onCanceled: { rotateHandle.rootWindow.endRotateDrag(false); rotateHandle.editorRef.endInteraction() }
                                }
                            }
                            Repeater {
                                model: parent.boxModel.pathPoints
                                delegate: Rectangle {
                                    id: pathHandle

                                    property var boxRef: parent
                                    property var rootWindow: boxRef.rootWindow
                                    property var editorRef: boxRef.editorRef
                                    width: Math.max(1, rootWindow.documentToViewLength(10)); height: width; color: rootWindow.palette.highlight; visible: boxRef.selected && boxRef.boxModel.path
                                    x: rootWindow.pointValue(modelData, 0, 0.5) * boxRef.width - width / 2
                                    y: rootWindow.pointValue(modelData, 1, 0.5) * boxRef.height - height / 2
                                    MouseArea { anchors.fill: parent; drag.target: parent; onPressed: pathHandle.editorRef.beginInteraction(); onReleased: pathHandle.editorRef.endInteraction(); onCanceled: pathHandle.editorRef.endInteraction(); onPositionChanged: pathHandle.editorRef.setPathHandle(index, (pathHandle.x + pathHandle.width / 2) / pathHandle.boxRef.width, (pathHandle.y + pathHandle.height / 2) / pathHandle.boxRef.height) }
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: Editor.boxes.length === 0
                        text: Editor.hasProject ? qsTr("Double-click canvas to create text") : qsTr("Create or open a project to start")
                        color: window.palette.mid
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        visible: window.selectedBoxHasRenderEffects()
                        text: qsTr("Live editing uses QML layout; rendered effects apply on export.")
                        color: window.palette.highlight
                    }
                    }
                }

                Pane {
                    id: rightPanel
                    z: 1
                    SplitView.minimumWidth: 180
                    SplitView.preferredWidth: 260
                    SplitView.fillHeight: true

                    ScrollView {
                        id: rightPanelScroll
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: rightPanelScroll.availableWidth
                            spacing: 8

                            ColumnLayout {
                                id: navigationSection

                                readonly property bool sectionReady: Editor.pageCount > 0

                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                Label { text: qsTr("Navigation"); font.bold: true; enabled: navigationSection.sectionReady }

                                GroupBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    enabled: navigationSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Label { text: Editor.currentPageName.length > 0 ? Editor.currentPageName : qsTr("No page"); font.bold: true; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 6
                                            Button { text: qsTr("Previous"); enabled: Editor.canGoPrevious; onClicked: Editor.previousPage() }
                                            Button { text: qsTr("Next"); enabled: Editor.canGoNext; onClicked: Editor.nextPage() }
                                        }
                                        ComboBox {
                                            id: pageSelect
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            model: Editor.pageLabels
                                            currentIndex: Editor.currentPageIndex
                                            enabled: Editor.pageCount > 0
                                            contentItem: Label { text: pageSelect.displayText; font: pageSelect.font; enabled: pageSelect.enabled; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            onActivated: index => Editor.goToPage(index)
                                        }
                                        Label { text: Editor.pageCount > 0 ? qsTr("Page %1 of %2").arg(Editor.currentPageIndex + 1).arg(Editor.pageCount) : qsTr("Page 0 of 0"); color: window.palette.mid; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                        CheckBox { text: qsTr("Show Raw"); checked: Editor.rawVisible; onToggled: Editor.rawVisible = checked }
                                    }
                                }
                            }

                            ColumnLayout {
                                id: boxEffectsSection

                                readonly property bool sectionReady: Editor.selectedIndex >= 0

                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                enabled: sectionReady

                                Label { text: qsTr("Box Effects"); font.bold: true; enabled: boxEffectsSection.sectionReady }

                                GroupBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    enabled: boxEffectsSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        TabBar {
                                            id: boxEffectsTabs
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            TabButton { text: qsTr("Rotation") }
                                            TabButton { text: qsTr("Perspective") }
                                        }

                                        StackLayout {
                                            currentIndex: boxEffectsTabs.currentIndex
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                Label { text: qsTr("Rotation") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: -360; to: 360; value: window.selectedBox() ? Math.round(window.selectedBox().rotation) : 0; onValueModified: Editor.setSelectedRotation(value) }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Perspective Handles"); checked: window.selectedBox() ? window.selectedBox().perspective : false; onClicked: Editor.setSelectedPerspectiveEnabled(checked) }
                                                Button { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Reset Perspective"); onClicked: Editor.resetSelectedPerspective() }
                                            }
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                id: textEffectsSection

                                readonly property bool sectionReady: Editor.selectedIndex >= 0

                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                enabled: sectionReady

                                Label { text: qsTr("Text Effects"); font.bold: true; enabled: textEffectsSection.sectionReady }

                                GroupBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    enabled: textEffectsSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        TabBar {
                                            id: textEffectsTabs
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            TabButton { text: qsTr("Outline") }
                                            TabButton { text: qsTr("Blur") }
                                            TabButton { text: qsTr("Shadow") }
                                            TabButton { text: qsTr("Gradient") }
                                            TabButton { text: qsTr("Path") }
                                        }

                                        StackLayout {
                                            currentIndex: textEffectsTabs.currentIndex
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Enabled"); checked: window.selectedBox() ? window.selectedBox().outline : false; onClicked: Editor.setSelectedOutlineEnabled(checked) }
                                                Label { text: qsTr("Color") }
                                                ColorButton { swatchText: window.selectedBox() ? window.qmlColor(window.selectedBox().outlineColor) : "#ffffff"; swatchColor: swatchText; onClicked: window.openColorDialog(swatchText, "outline") }
                                                Label { text: qsTr("Size") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: 0; to: 128; value: window.selectedBox() ? window.selectedBox().outlineSize : 2; onValueModified: Editor.setSelectedOutlineSize(value) }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Enabled"); checked: window.selectedBox() ? window.selectedBox().blur : false; onClicked: Editor.setSelectedBlurEnabled(checked) }
                                                Label { text: qsTr("Size") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: 0; to: 128; value: window.selectedBox() ? window.selectedBox().blurSize : 0; onValueModified: Editor.setSelectedBlurSize(value) }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Enabled"); checked: window.selectedBox() ? window.selectedBox().shadow : false; onClicked: Editor.setSelectedShadowEnabled(checked) }
                                                Label { text: qsTr("Color") }
                                                ColorButton { swatchText: window.selectedBox() ? window.qmlColor(window.selectedBox().shadowColor) : "#000000"; swatchColor: swatchText; onClicked: window.openColorDialog(swatchText, "shadow") }
                                                Label { text: qsTr("Offset X") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: -512; to: 512; value: window.selectedBox() ? window.selectedBox().shadowOffsetX : 4; onValueModified: Editor.setSelectedShadowOffsetX(value) }
                                                Label { text: qsTr("Offset Y") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: -512; to: 512; value: window.selectedBox() ? window.selectedBox().shadowOffsetY : 4; onValueModified: Editor.setSelectedShadowOffsetY(value) }
                                                Label { text: qsTr("Blur") }
                                                SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: 0; to: 128; value: window.selectedBox() ? window.selectedBox().shadowBlurSize : 0; onValueModified: Editor.setSelectedShadowBlurSize(value) }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Enabled"); checked: window.selectedBox() ? window.selectedBox().gradient : false; onClicked: Editor.setSelectedGradientEnabled(checked) }
                                                Label { text: qsTr("Direction") }
                                                ComboBox { Layout.fillWidth: true; Layout.minimumWidth: 0; model: [qsTr("Vertical"), qsTr("Horizontal")]; currentIndex: window.selectedBox() ? Math.min(window.selectedBox().gradientDirection, 1) : 0; onActivated: Editor.setSelectedGradientDirection(currentIndex) }
                                                Label { text: qsTr("Color A") }
                                                ColorButton { swatchText: window.selectedBox() ? window.qmlColor(window.selectedBox().gradientColorA) : "#ffffff"; swatchColor: swatchText; onClicked: window.openColorDialog(swatchText, "gradientA") }
                                                Label { text: qsTr("Color B") }
                                                ColorButton { swatchText: window.selectedBox() ? window.qmlColor(window.selectedBox().gradientColorB) : "#000000"; swatchColor: swatchText; onClicked: window.openColorDialog(swatchText, "gradientB") }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                CheckBox { text: qsTr("Path Handles"); checked: window.selectedBox() ? window.selectedBox().path : false; onClicked: Editor.setSelectedPathEnabled(checked) }
                                                Label { text: qsTr("Mode") }
                                                ComboBox { Layout.fillWidth: true; Layout.minimumWidth: 0; model: [qsTr("Straight"), qsTr("Smooth")]; currentIndex: window.selectedBox() ? Math.min(window.selectedBox().pathMode, 1) : 0; onActivated: Editor.setSelectedPathMode(currentIndex) }
                                                Button { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Add Path Point"); onClicked: Editor.addSelectedPathPoint() }
                                                Button { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("Reset Path"); onClicked: Editor.resetSelectedPath() }
                                            }
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                id: layersSection

                                readonly property bool sectionReady: Editor.layers.length > 0

                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.fillHeight: true
                                enabled: sectionReady

                                Label { text: qsTr("Layers"); font.bold: true; enabled: layersSection.sectionReady }

                                GroupBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.fillHeight: true
                                    enabled: layersSection.sectionReady

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 6
                                            Button { text: qsTr("Up"); enabled: Editor.selectedIndex >= 0 && Editor.selectedIndex < Editor.boxes.length - 1; onClicked: Editor.moveLayer(Editor.selectedIndex + 1) }
                                            Button { text: qsTr("Down"); enabled: Editor.selectedIndex > 0; onClicked: Editor.moveLayer(Editor.selectedIndex - 1) }
                                        }
                                        ListView {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredHeight: 160
                                            Layout.fillHeight: true
                                            clip: true
                                            model: Editor.layers
                                            delegate: ItemDelegate {
                                                id: layerDelegate

                                                width: ListView.view.width
                                                text: qsTr("Layer %1 — %2").arg(modelData.index + 1).arg(modelData.text.length > 0 ? modelData.text : qsTr("Text"))
                                                highlighted: modelData.index === Editor.selectedIndex
                                                contentItem: Label { text: layerDelegate.text; font: layerDelegate.font; enabled: layerDelegate.enabled; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                onClicked: Editor.selectBox(modelData.index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: toastPopup
        x: window.width - width - 24
        y: window.height - height - 24
        modal: false
        closePolicy: Popup.NoAutoClose
        Label { id: toastLabel; padding: 12; color: window.palette.highlightedText }
        background: Rectangle { color: window.palette.highlight; radius: 6 }
    }

    Timer { id: toastTimer; interval: 2200; onTriggered: toastPopup.close() }

    FolderDialog {
        id: openProjectDialog
        title: qsTr("Open Project")
        onAccepted: Editor.openProject(window.localPathFromUrl(selectedFolder))
    }

    ColorDialog {
        id: colorDialog
        onAccepted: window.applyColorDialogSelection(window.dialogColorHex(selectedColor))
    }
}
