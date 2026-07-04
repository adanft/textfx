import QtQuick

Item {
    id: resizeHandles

    property var boxRef
    property var canvasItem
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
            property var rootWindow: boxRef.rootWindow
            property var editorRef: boxRef.editorRef
            readonly property var visualPosition: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height)

            objectName: "resizeHandle_" + modelData.name
            z: resizeHandles.zResizeHandles
            width: rootWindow.handleSize()
            height: width
            radius: width / 2
            color: rootWindow.palette.highlight
            x: visualPosition.x - width / 2
            y: visualPosition.y - height / 2
            visible: boxRef.selected

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                cursorShape: Qt.SizeFDiagCursor
                onPressed: (mouse) => {
                    mouse.accepted = true;
                    resizeHandle.editorRef.selectBox(resizeHandle.boxRef.boxModel.index);
                    resizeHandle.editorRef.beginInteraction();
                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y);
                    else
                        resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y);
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return ;

                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.boxRef.perspectiveActive) {
                        resizeHandle.rootWindow.updatePerspectiveDrag(point.x, point.y);
                        return ;
                    }
                    resizeHandle.rootWindow.updateResizeDrag(point.x, point.y);
                }
                onReleased: {
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.endPerspectiveDrag(true);
                    else
                        resizeHandle.rootWindow.endResizeDrag(true);
                    resizeHandle.editorRef.endInteraction();
                }
                onCanceled: {
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.endPerspectiveDrag(false);
                    else
                        resizeHandle.rootWindow.endResizeDrag(false);
                    resizeHandle.editorRef.endInteraction();
                }
            }

        }

    }

}
