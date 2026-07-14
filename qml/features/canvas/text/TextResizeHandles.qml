import QtQuick

Item {
    id: resizeHandles

    property var boxRef
    property var canvasItem
    required property real boxOriginX
    required property real boxOriginY
    readonly property int zResizeHandles: 20

    anchors.fill: parent
    z: zResizeHandles


    Repeater {
        model: [{
            "name": "nw"
        }, {
            "name": "n"
        }, {
            "name": "ne"
        }, {
            "name": "e"
        }, {
            "name": "se"
        }, {
            "name": "s"
        }, {
            "name": "sw"
        }, {
            "name": "w"
        }]

        delegate: Rectangle {
            id: resizeHandle

            property var boxRef: resizeHandles.boxRef
            property var canvasItem: resizeHandles.canvasItem
            property var rootWindow: boxRef ? boxRef.rootWindow : null
            property var editorRef: boxRef ? boxRef.editorRef : null
            readonly property var boxModel: boxRef ? boxRef.boxModel : null
            readonly property bool handleReady: rootWindow && editorRef && boxModel
            property bool dragStarted: false
            property bool dragPerspective: false
            property var dragRootWindow: null
            property var dragEditorRef: null
            readonly property var visualPosition: handleReady ? rootWindow.visualHandlePosition(boxModel, modelData.name, boxRef.width, boxRef.height) : Qt.point(0, 0)

            function finishDrag(commit) {
                if (!dragStarted)
                    return ;
                if (dragRootWindow) {
                    if (dragPerspective)
                        dragRootWindow.endPerspectiveDrag(commit);
                    else
                        dragRootWindow.endResizeDrag(commit);
                }
                if (dragEditorRef)
                    dragEditorRef.endInteraction();
                dragStarted = false;
                dragPerspective = false;
                dragRootWindow = null;
                dragEditorRef = null;
            }

            objectName: "resizeHandle_" + modelData.name
            z: resizeHandles.zResizeHandles
            width: handleReady ? rootWindow.resizeHandleSize : 0
            height: width
            radius: width / 2
            color: handleReady ? rootWindow.palette.highlight : "transparent"
            x: boxOriginX + visualPosition.x - width / 2
            y: boxOriginY + visualPosition.y - height / 2
            visible: handleReady && boxRef.selected

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                cursorShape: Qt.SizeFDiagCursor
                onPressed: (mouse) => {
                    mouse.accepted = true;
                    if (!resizeHandle.handleReady)
                        return ;
                    const pressedBoxRef = resizeHandle.boxRef;
                    const pressedBoxModel = resizeHandle.boxModel;
                    const pressedRootWindow = resizeHandle.rootWindow;
                    const pressedEditorRef = resizeHandle.editorRef;
                    resizeHandle.dragStarted = true;
                    resizeHandle.dragPerspective = pressedBoxRef.perspectiveActive;
                    resizeHandle.dragRootWindow = pressedRootWindow;
                    resizeHandle.dragEditorRef = pressedEditorRef;
                    pressedEditorRef.selectBox(pressedBoxModel.index);
                    pressedEditorRef.beginInteraction();
                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.dragPerspective)
                        pressedRootWindow.beginPerspectiveDrag(pressedBoxRef, modelData.name, point.x, point.y);
                    else
                        pressedRootWindow.beginResizeDrag(pressedBoxRef, modelData.name, point.x, point.y);
                }
                onPositionChanged: (mouse) => {
                    if (!pressed || !resizeHandle.dragStarted || !resizeHandle.dragRootWindow)
                        return ;

                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.dragPerspective) {
                        resizeHandle.dragRootWindow.updatePerspectiveDrag(point.x, point.y);
                        return ;
                    }
                    resizeHandle.dragRootWindow.updateResizeDrag(point.x, point.y);
                }
                onReleased: resizeHandle.finishDrag(true)
                onCanceled: resizeHandle.finishDrag(false)
            }

        }

    }

}
