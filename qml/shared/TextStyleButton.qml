import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../constants/UiConstants.js" as UiConstants

Button {
    id: textStyleButton

    property string accessibleLabel: ""
    property url iconSource: ""
    property int minimumButtonSize: UiConstants.styleButtonSize

    checkable: true
    Layout.minimumWidth: minimumButtonSize
    Layout.minimumHeight: minimumButtonSize
    Layout.preferredWidth: minimumButtonSize
    Layout.preferredHeight: minimumButtonSize
    display: AbstractButton.IconOnly
    icon.source: iconSource
    icon.width: UiConstants.iconSize
    icon.height: UiConstants.iconSize
    icon.color: !enabled ? palette.mid : (checked ? palette.accent : palette.buttonText)
    ToolTip.text: accessibleLabel
    ToolTip.visible: hovered
    Accessible.name: accessibleLabel
}
