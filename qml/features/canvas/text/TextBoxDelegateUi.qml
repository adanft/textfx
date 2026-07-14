import QtQuick
Item {
    id: delegateUi
    required property var boxRef
    required property var canvasItem
    required property var outlinedTextItem
    parent: boxRef
    anchors.fill: parent
    TapHandler {
        enabled: delegateUi.boxRef.editingSelected
        acceptedButtons: Qt.LeftButton
        acceptedModifiers: Qt.ControlModifier | Qt.ShiftModifier
        onTapped: (eventPoint) => {
            if (!delegateUi.boxRef.editingSelected)
                return ;
            delegateUi.boxRef.editorRef.endTextEdit();
            if (eventPoint.modifiers & Qt.ControlModifier)
                delegateUi.boxRef.editorRef.toggleBoxSelection(delegateUi.boxRef.boxModel.index);
            else
                delegateUi.boxRef.editorRef.selectBox(delegateUi.boxRef.boxModel.index);
        }
    }
    Canvas {
        id: perspectiveBorder
        property var boxRef: delegateUi.boxRef
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        property real margin: rootWindow.perspectiveMargin(boxRef.boxModel)
        parent: boxRef
        objectName: "perspectiveBorder"
        x: -margin
        y: -margin
        width: parent.width + margin * 2
        height: parent.height + margin * 2
        visible: boxRef.selected && boxRef.perspectiveActive
        z: boxRef.zPerspectiveBorder
        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            const nw = rootWindow.perspectiveCorner(boxRef.boxModel, "nw", boxRef.width, boxRef.height);
            const ne = rootWindow.perspectiveCorner(boxRef.boxModel, "ne", boxRef.width, boxRef.height);
            const se = rootWindow.perspectiveCorner(boxRef.boxModel, "se", boxRef.width, boxRef.height);
            const sw = rootWindow.perspectiveCorner(boxRef.boxModel, "sw", boxRef.width, boxRef.height);
            ctx.beginPath();
            ctx.moveTo(nw.x + margin, nw.y + margin);
            ctx.lineTo(ne.x + margin, ne.y + margin);
            ctx.lineTo(se.x + margin, se.y + margin);
            ctx.lineTo(sw.x + margin, sw.y + margin);
            ctx.closePath();
            ctx.lineWidth = rootWindow.selectionLineWidth();
            ctx.strokeStyle = rootWindow.palette.highlight;
            ctx.stroke();
        }
        Connections {
            function onZoomChanged() {
                perspectiveBorder.requestPaint();
            }
            function onPerspectiveRevisionChanged() {
                perspectiveBorder.requestPaint();
            }
            target: perspectiveBorder.rootWindow
        }
        Connections {
            function onDocumentChanged() {
                perspectiveBorder.requestPaint();
            }
            target: perspectiveBorder.editorRef
        }
    }
    TextBoxEditControls {
        parent: delegateUi.boxRef
        boxRef: delegateUi.boxRef
        canvasItem: delegateUi.canvasItem
        outlinedTextItem: delegateUi.outlinedTextItem
    }
    TextBoxSelectionControls {
        parent: delegateUi.boxRef
        boxRef: delegateUi.boxRef
        canvasItem: delegateUi.canvasItem
    }
    TextBoxPathControls {
        parent: delegateUi.boxRef
        boxRef: delegateUi.boxRef
        canvasItem: delegateUi.canvasItem
    }
}
