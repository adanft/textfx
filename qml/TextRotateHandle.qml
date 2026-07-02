import QtQuick

Rectangle {
    id: rotateHandle

    property var boxRef
    property var canvasItem
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef

    z: 20
    width: rootWindow.handleSize()
    height: width
    radius: width / 2
    color: rootWindow.palette.highlight
    x: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).x - width / 2
    y: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).y - height / 2
    visible: boxRef.selected

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        onPressed: (mouse) => {
            mouse.accepted = true;
            rotateHandle.editorRef.selectBox(rotateHandle.boxRef.boxModel.index);
            rotateHandle.editorRef.beginInteraction();
            const point = mapToItem(canvasItem, mouse.x, mouse.y);
            rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef, point.x, point.y);
        }
        onPositionChanged: (mouse) => {
            if (!pressed)
                return ;

            const point = mapToItem(canvasItem, mouse.x, mouse.y);
            rotateHandle.rootWindow.updateRotateDrag(point.x, point.y);
        }
        onReleased: {
            rotateHandle.rootWindow.endRotateDrag(true);
            rotateHandle.editorRef.endInteraction();
        }
        onCanceled: {
            rotateHandle.rootWindow.endRotateDrag(false);
            rotateHandle.editorRef.endInteraction();
        }
    }

}
