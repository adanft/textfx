pragma ComponentBehavior: Bound

import QtQuick
import "text"

CentralCanvasShell {
    id: canvasView

    property var appWindow
    property var editor
    property var rightPanel
    property var editorLimits
    property var editorInteraction
    property var canvasInteraction
    readonly property Item canvas: canvasView.canvasItem
    readonly property real pageSourceWidth: pageImage.sourceSize.width
    readonly property real pageSourceHeight: pageImage.sourceSize.height

    objectName: "centralCanvasShell"
    hostPalette: canvasView.appWindow.palette
    editingText: canvasView.editor.editingText
    onEscapePressed: canvasView.appWindow.handleEscape()
    onCopyPressed: {
        if (canvasView.editor.actionEnabled("copy"))
            canvasView.editor.copySelected();
    }
    onPastePressed: {
        if (canvasView.editor.actionEnabled("paste"))
            canvasView.editor.pasteBox();
    }
    onDeletePressed: canvasView.editor.deleteSelected()
    onCanvasPressed: (x, y, button, modifiers) => {
        if (canvasView.rightPanel.paintMode)
            return ;
        if (button === Qt.LeftButton)
            canvasView.editor.endTextEdit();

        canvasView.canvasInteraction.beginPress(x, y, button, modifiers);
    }
    onCanvasPositionChanged: (x, y, pressed) => {
        if (canvasView.rightPanel.paintMode)
            return ;
        const update = canvasView.canvasInteraction.updatePointer(x, y, pressed);
        if (update.panned) {
            canvasView.appWindow.panX += update.panDx;
            canvasView.appWindow.panY += update.panDy;
        }
    }
    onCanvasReleased: (x, y, button, modifiers) => {
        if (canvasView.rightPanel.paintMode)
            return ;
        const release = canvasView.canvasInteraction.endRelease();
        if (release.creating) {
            const rectangle = release.rectangle;
            const w = rectangle.width / canvasView.appWindow.viewDocScale();
            const h = rectangle.height / canvasView.appWindow.viewDocScale();
            if (w >= canvasView.editorLimits.minimumBoxSize && h >= canvasView.editorLimits.minimumBoxSize)
                canvasView.editor.createTextBox(canvasView.appWindow.viewToDocumentX(rectangle.x), canvasView.appWindow.viewToDocumentY(rectangle.y), w, h);

        }
    }
    onCanvasWheel: (x, y, angleDeltaY) => {
        return canvasView.appWindow.zoomAt(x, y, angleDeltaY > 0 ? 1.1 : 1 / 1.1);
    }

    Image {
        id: pageImage
        objectName: "pageImage"

        x: canvasView.appWindow.documentToViewX(0)
        y: canvasView.appWindow.documentToViewY(0)
        width: canvasView.appWindow.pageDisplayWidth() * canvasView.appWindow.zoom
        height: canvasView.appWindow.pageDisplayHeight() * canvasView.appWindow.zoom
        source: canvasView.editor.currentPageUrl
        fillMode: Image.Stretch
        asynchronous: true
        visible: source.toString().length > 0
        onStatusChanged: {
            if (status === Image.Ready)
                canvasView.appWindow.pageBaseScale = canvasView.appWindow.fitPageScale();

        }
    }

    PaintLayer {
        id: paintBehindTextLayer
        objectName: "paintBehindTextLayer"
        x: pageImage.x
        y: pageImage.y
        width: pageImage.sourceSize.width
        height: pageImage.sourceSize.height
        scale: canvasView.appWindow.viewDocScale()
        transformOrigin: Item.TopLeft
        strokes: canvasView.editor.paintBehindText
        drawPreviewStroke: false
        visible: pageImage.visible && hasPaintContent
    }

    PaintLayer {
        id: paintBehindTextPreviewLayer
        objectName: "paintBehindTextPreviewLayer"
        x: pageImage.x
        y: pageImage.y
        width: pageImage.sourceSize.width
        height: pageImage.sourceSize.height
        scale: canvasView.appWindow.viewDocScale()
        transformOrigin: Item.TopLeft
        previewStroke: canvasView.appWindow.activePaintStroke("behind_text")
        drawPersistedStrokes: false
        visible: pageImage.visible && hasPaintContent
    }

    Image {
        id: rawOverlayImage

        x: pageImage.x
        y: pageImage.y
        width: pageImage.width
        height: pageImage.height
        source: canvasView.editor.rawPageUrl
        fillMode: Image.Stretch
        asynchronous: true
        opacity: 0.45
        visible: canvasView.editor.rawVisible && source.toString().length > 0
    }

    Rectangle {
        visible: canvasView.appWindow.dragMode === canvasView.editorInteraction.dragModeCreate
        x: Math.min(canvasView.appWindow.createStartX, canvasView.appWindow.createCurrentX)
        y: Math.min(canvasView.appWindow.createStartY, canvasView.appWindow.createCurrentY)
        width: Math.abs(canvasView.appWindow.createCurrentX - canvasView.appWindow.createStartX)
        height: Math.abs(canvasView.appWindow.createCurrentY - canvasView.appWindow.createStartY)
        color: Qt.alpha(canvasView.appWindow.palette.highlight, 0.14)
        border.color: canvasView.appWindow.palette.highlight
        border.width: 1
    }

    MouseArea {
        anchors.fill: parent
        visible: canvasView.appWindow.dragMode === canvasView.editorInteraction.dragModePathHandle && canvasView.appWindow.pathHandleInteractionActive
        enabled: visible
        z: 100
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        onPositionChanged: (mouse) => {
            canvasView.appWindow.updatePathHandleDragFromCanvas(mouse.x, mouse.y);
        }
        onReleased: canvasView.appWindow.endPathHandleDrag()
        onCanceled: canvasView.appWindow.endPathHandleDrag()
    }

    Repeater {
        model: canvasView.editor.boxesModel

        delegate: TextBoxDelegate {
            canvasItem: canvasView.canvas
            interaction: canvasView.editorInteraction
            renderSelectionUi: false
        }

    }

    PaintLayer {
        id: paintAboveTextLayer
        objectName: "paintAboveTextLayer"
        z: 30
        x: pageImage.x
        y: pageImage.y
        width: pageImage.sourceSize.width
        height: pageImage.sourceSize.height
        scale: canvasView.appWindow.viewDocScale()
        transformOrigin: Item.TopLeft
        strokes: canvasView.editor.paintAboveText
        drawPreviewStroke: false
        visible: pageImage.visible && hasPaintContent
    }

    PaintLayer {
        id: paintAboveTextPreviewLayer
        objectName: "paintAboveTextPreviewLayer"
        z: 30
        x: pageImage.x
        y: pageImage.y
        width: pageImage.sourceSize.width
        height: pageImage.sourceSize.height
        scale: canvasView.appWindow.viewDocScale()
        transformOrigin: Item.TopLeft
        previewStroke: canvasView.appWindow.activePaintStroke("above_text")
        drawPersistedStrokes: false
        visible: pageImage.visible && hasPaintContent
    }

    Repeater {
        model: canvasView.editor.boxesModel

        delegate: TextBoxDelegate {
            canvasItem: canvasView.canvas
            interaction: canvasView.editorInteraction
            renderTextContent: false
        }

    }

    MouseArea {
        id: paintInputArea
        objectName: "paintInputArea"
        x: pageImage.x
        y: pageImage.y
        width: pageImage.width
        height: pageImage.height
        visible: canvasView.rightPanel.paintMode && pageImage.visible
        enabled: visible
        z: 200
        hoverEnabled: true
        cursorShape: Qt.CrossCursor
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        function containsLocal(localX, localY) {
            return localX >= 0 && localY >= 0 && localX <= width && localY <= height;
        }
        function canvasPoint(localX, localY) {
            return paintInputArea.mapToItem(canvasView.canvas, localX, localY);
        }
        onPressed: (mouse) => {
            if (!containsLocal(mouse.x, mouse.y)) {
                mouse.accepted = false;
                return ;
            }
            canvasView.canvas.forceActiveFocus();
            const point = canvasPoint(mouse.x, mouse.y);
            canvasView.appWindow.beginPaintDrag(point.x, point.y);
            mouse.accepted = true;
        }
        onPositionChanged: (mouse) => {
            if (pressed && containsLocal(mouse.x, mouse.y)) {
                const point = canvasPoint(mouse.x, mouse.y);
                canvasView.appWindow.updatePaintDrag(point.x, point.y);
            }
            mouse.accepted = true;
        }
        onReleased: (mouse) => {
            canvasView.appWindow.endPaintDrag();
            mouse.accepted = true;
        }
        onCanceled: canvasView.appWindow.cancelPaintDrag()
    }

}
