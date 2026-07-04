import QtQuick

Item {
    id: pathHandlePlane

    property var boxRef
    property var canvasItem
    property var rootWindow: boxRef.rootWindow
    readonly property int zPathHandles: 20
    readonly property real handleViewSize: Math.max(1, rootWindow.documentToViewLength(10))
    readonly property real minimumTouchMargin: 8
    readonly property color handleColor: "#ffb000"
    readonly property color handleBorderColor: "#202020"

    anchors.fill: parent
    z: zPathHandles


    Repeater {
        model: pathHandlePlane.boxRef.boxModel.pathPoints

        delegate: Rectangle {
            id: pathHandle

            property var pathPlane: parent
            property var boxRef: pathPlane.boxRef
            property var canvasItem: pathPlane.canvasItem
            property var rootWindow: pathPlane.rootWindow
            property var editorRef: boxRef.editorRef

            objectName: "pathHandle"
            width: pathPlane.handleViewSize
            height: width
            color: pathPlane.handleColor
            border.color: pathPlane.handleBorderColor
            visible: boxRef.selected && boxRef.boxModel.path
            x: rootWindow.pointValue(modelData, 0, 0.5) * pathPlane.width - width / 2
            y: rootWindow.pointValue(modelData, 1, 0.5) * pathPlane.height - height / 2

            MouseArea {
                anchors.fill: parent
                anchors.margins: -Math.max(pathHandle.pathPlane.minimumTouchMargin, pathHandle.width)
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                onPressed: (mouse) => {
                    mouse.accepted = true;
                    pathHandle.editorRef.selectBox(pathHandle.boxRef.boxModel.index);
                    const canvasPoint = mapToItem(canvasItem, mouse.x, mouse.y);
                    const startX = rootWindow.pointValue(modelData, 0, 0.5) * pathHandle.pathPlane.width;
                    const startY = rootWindow.pointValue(modelData, 1, 0.5) * pathHandle.pathPlane.height;
                    pathHandle.rootWindow.beginPathHandleDrag(pathHandle.pathPlane, index, startX, startY, canvasPoint.x, canvasPoint.y);
                }
                onPositionChanged: (mouse) => {
                    if (!pressed || !(mouse.buttons & Qt.LeftButton))
                        return ;

                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    pathHandle.rootWindow.updatePathHandleDragFromCanvas(point.x, point.y);
                }
                onReleased: pathHandle.rootWindow.endPathHandleDrag()
                onCanceled: pathHandle.rootWindow.endPathHandleDrag()
            }

        }

    }

    transform: Matrix4x4 {
        matrix: pathHandlePlane.rootWindow.perspectiveMatrix(pathHandlePlane.boxRef.boxModel, pathHandlePlane.width, pathHandlePlane.height, pathHandlePlane.rootWindow.viewDocScale(), pathHandlePlane.boxRef.perspectiveActive)
    }

}
