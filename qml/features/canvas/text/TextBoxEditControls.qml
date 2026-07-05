import QtQuick

Item {
    id: editControls

    required property var boxRef
    required property var canvasItem
    required property var outlinedTextItem

    objectName: "textBoxEditControls"
    anchors.fill: parent
    z: boxRef.zTextContent

    TextEditOverlay {
        id: boxTextOverlay

        boxRef: editControls.boxRef
        outlinedTextItem: editControls.outlinedTextItem
        selectionUiVisible: editControls.boxRef.renderSelectionUi
    }

    TextBoxMoveArea {
        objectName: "textBoxMoveArea"
        boxRef: editControls.boxRef
        canvasItem: editControls.canvasItem
        editOverlay: boxTextOverlay
        visible: editControls.boxRef.renderSelectionUi
    }
}
