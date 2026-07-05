import QtQuick

Item {
    id: pathControls

    property var boxRef
    property var canvasItem
    readonly property var rootWindow: boxRef ? boxRef.rootWindow : null
    readonly property bool activePathDragForThisBox: rootWindow && rootWindow.activePathHandlePlane && rootWindow.activePathHandlePlane.boxRef === boxRef
    readonly property bool pathDecorationsLoaded: boxRef && ((boxRef.selected && boxRef.boxModel.path) || activePathDragForThisBox)
    readonly property bool pathGuideLoaded: boxRef && boxRef.selected && boxRef.boxModel.path && boxRef.boxModel.pathPoints && boxRef.boxModel.pathPoints.length > 1

    anchors.fill: parent
    visible: boxRef && boxRef.renderSelectionUi

    Loader {
        id: pathGuideLoader

        property var boxRef: pathControls.boxRef

        parent: pathControls.boxRef
        anchors.fill: parent
        active: pathControls.visible && pathControls.pathGuideLoaded
        sourceComponent: Component {
            TextPathGuide {
                boxRef: pathGuideLoader.boxRef
            }
        }
        z: 18
    }

    Loader {
        id: pathHandlesLoader

        property var boxRef: pathControls.boxRef
        property var canvasItem: pathControls.canvasItem

        parent: pathControls.boxRef
        anchors.fill: parent
        active: pathControls.visible && pathControls.pathDecorationsLoaded
        sourceComponent: Component {
            TextPathHandles {
                boxRef: pathHandlesLoader.boxRef
                canvasItem: pathHandlesLoader.canvasItem
            }
        }
        z: pathControls.boxRef ? pathControls.boxRef.zSelectionControls : 20
    }
}

