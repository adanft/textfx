import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: textStyleButton

    property string accessibleLabel: ""
    property int buttonSize: 24

    checkable: true
    width: buttonSize
    height: buttonSize
    implicitWidth: buttonSize
    implicitHeight: buttonSize
    Layout.preferredWidth: buttonSize
    Layout.preferredHeight: buttonSize
    font.family: "Symbols Nerd Font"
    font.pixelSize: 18
    ToolTip.text: accessibleLabel
    ToolTip.visible: hovered
    Accessible.name: accessibleLabel
}
