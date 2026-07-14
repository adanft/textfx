pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: selectionControls

    required property var boxRef
    required property var canvasItem
    required property real boxOriginX
    required property real boxOriginY

    objectName: "textBoxSelectionControls"
    anchors.fill: parent
    visible: boxRef && boxRef.renderSelectionUi
    z: boxRef ? boxRef.zSelectionControls : 20

    TextResizeHandles {
        boxRef: selectionControls.boxRef
        canvasItem: selectionControls.canvasItem
        boxOriginX: selectionControls.boxOriginX
        boxOriginY: selectionControls.boxOriginY
        visible: selectionControls.boxRef && selectionControls.boxRef.renderSelectionUi
    }

    Loader {
        id: rotateHandleLoader

        property var boxRef: selectionControls.boxRef
        property var canvasItem: selectionControls.canvasItem

        active: selectionControls.boxRef && selectionControls.boxRef.renderSelectionUi && selectionControls.boxRef.rotateDecorationsLoaded
        sourceComponent: Component {
            TextRotateHandle {
                boxRef: rotateHandleLoader.boxRef
                canvasItem: rotateHandleLoader.canvasItem
                boxOriginX: selectionControls.boxOriginX
                boxOriginY: selectionControls.boxOriginY
            }
        }
        z: selectionControls.boxRef ? selectionControls.boxRef.zSelectionControls : 20
    }
}
