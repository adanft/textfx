import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../constants/UiConstants.js" as UiConstants

Button {
    id: colorButton

    property color swatchColor: UiConstants.defaultBlack
    property string swatchText: "#000000"

    Layout.fillWidth: true

    contentItem: RowLayout {
        spacing: UiConstants.inlineSpacing

        Rectangle {
            width: UiConstants.colorSwatchWidth
            height: UiConstants.colorSwatchHeight
            radius: UiConstants.colorSwatchRadius
            color: colorButton.enabled ? colorButton.swatchColor : colorButton.palette.mid
            border.color: colorButton.palette.mid
        }

        Label {
            text: colorButton.swatchText
            enabled: colorButton.enabled
            Layout.fillWidth: true
        }

    }

}
