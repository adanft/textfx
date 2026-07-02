import QtQuick

MouseArea {
    id: moveArea

    property var boxRef
    property var canvasItem
    property var editOverlay
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef
    readonly property var visualBounds: boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds(boxRef.boxModel, boxRef.width, boxRef.height) : ({
        "x": 0,
        "y": 0,
        "width": boxRef.width,
        "height": boxRef.height
    })
    readonly property int zBehindEditOverlay: -1
    readonly property int zMoveArea: 10

    z: boxRef.selected && editorRef.editingText ? zBehindEditOverlay : zMoveArea
    x: visualBounds.x
    y: visualBounds.y
    width: visualBounds.width
    height: visualBounds.height
    acceptedButtons: Qt.LeftButton
    preventStealing: true
    enabled: !(boxRef.selected && editorRef.editingText)
    onPressed: (mouse) => {
        const local = mapToItem(boxRef, mouse.x, mouse.y);
        if (boxRef.perspectiveActive && !rootWindow.pointInPerspectivePolygon(local, boxRef.boxModel, boxRef.width, boxRef.height)) {
            mouse.accepted = false;
            return ;
        }
        mouse.accepted = true;
        canvasItem.forceActiveFocus();
        editorRef.selectBox(boxRef.boxModel.index);
        editorRef.beginInteraction();
        const point = mapToItem(canvasItem, mouse.x, mouse.y);
        rootWindow.beginMoveDrag(boxRef, boxRef.boxModel.index, point.x, point.y);
    }
    onPositionChanged: (mouse) => {
        if (!pressed || boxRef.boxModel.index !== editorRef.selectedIndex)
            return ;

        const point = mapToItem(canvasItem, mouse.x, mouse.y);
        rootWindow.updateMoveDrag(point.x, point.y);
    }
    onReleased: {
        rootWindow.endMoveDrag(true);
        editorRef.endInteraction();
    }
    onCanceled: {
        rootWindow.endMoveDrag(false);
        editorRef.endInteraction();
    }
    onDoubleClicked: (mouse) => {
        editorRef.selectBox(boxRef.boxModel.index);
        editorRef.beginTextEdit();
        editOverlay.forceEditFocus();
    }
}
