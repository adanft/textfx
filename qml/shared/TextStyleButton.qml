import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: textStyleButton

    property string accessibleLabel: ""
    property url iconSource: ""
    property int minimumButtonSize: 32

    checkable: true
    Layout.minimumWidth: minimumButtonSize
    Layout.minimumHeight: minimumButtonSize
    Layout.preferredWidth: minimumButtonSize
    Layout.preferredHeight: minimumButtonSize
    display: AbstractButton.IconOnly
    icon.source: iconSource
    icon.width: 20
    icon.height: 20
    icon.color: !enabled ? palette.mid : (checked ? palette.accent : palette.buttonText)
    ToolTip.text: accessibleLabel
    ToolTip.visible: hovered
    Accessible.name: accessibleLabel
}
