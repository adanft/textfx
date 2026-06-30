import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: colorButton

    property color swatchColor: "#000000"
    property string swatchText: "#000000"

    Layout.fillWidth: true
    contentItem: RowLayout {
        spacing: 8

        Rectangle { width: 22; height: 16; radius: 3; color: colorButton.enabled ? colorButton.swatchColor : colorButton.palette.mid; border.color: colorButton.palette.mid }
        Label { text: colorButton.swatchText; enabled: colorButton.enabled; Layout.fillWidth: true }
    }
}
