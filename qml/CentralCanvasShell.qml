import QtQuick
import QtQuick.Controls

Item {
    id: canvasShell

    property var hostPalette
    property bool editingText: false
    property alias canvasItem: canvas
    default property alias canvasContent: canvas.data

    signal escapePressed()
    signal copyPressed()
    signal pastePressed()
    signal deletePressed()
    signal canvasPressed(real x, real y, int button, int modifiers)
    signal canvasPositionChanged(real x, real y, bool pressed)
    signal canvasReleased(real x, real y, int button, int modifiers)
    signal canvasWheel(real x, real y, real angleDeltaY)

    z: 0
    SplitView.minimumWidth: 240
    SplitView.fillWidth: true
    SplitView.fillHeight: true

    Rectangle {
        id: canvas

        objectName: "centralCanvas"
        z: 0
        x: -canvasShell.x
        width: canvasShell.SplitView.view ? canvasShell.SplitView.view.width : canvasShell.width
        height: canvasShell.height
        color: canvasShell.hostPalette.base
        clip: true
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                canvasShell.escapePressed();
                event.accepted = true;
                return ;
            }
            if (canvasShell.editingText)
                return ;

            if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_C) {
                canvasShell.copyPressed();
                event.accepted = true;
                return ;
            }
            if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_V) {
                canvasShell.pastePressed();
                event.accepted = true;
                return ;
            }

            if (event.key === Qt.Key_Delete) {
                canvasShell.deletePressed();
                event.accepted = true;
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            onPressed: (mouse) => {
                canvas.forceActiveFocus();
                canvasShell.canvasPressed(mouse.x, mouse.y, mouse.button, mouse.modifiers);
            }
            onPositionChanged: (mouse) => {
                return canvasShell.canvasPositionChanged(mouse.x, mouse.y, pressed);
            }
            onReleased: (mouse) => {
                return canvasShell.canvasReleased(mouse.x, mouse.y, mouse.button, mouse.modifiers);
            }
            onWheel: (wheel) => {
                canvasShell.canvasWheel(wheel.x, wheel.y, wheel.angleDelta.y);
                wheel.accepted = true;
            }
        }

    }

}
