pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: selectionControls

    required property var boxRef
    required property var canvasItem

    objectName: "textBoxSelectionControls"
    anchors.fill: parent
    visible: boxRef && boxRef.renderSelectionUi
    z: boxRef ? boxRef.zSelectionControls : 20

    TextResizeHandles {
        boxRef: selectionControls.boxRef
        canvasItem: selectionControls.canvasItem
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
            }
        }
        z: selectionControls.boxRef ? selectionControls.boxRef.zSelectionControls : 20
    }
}
