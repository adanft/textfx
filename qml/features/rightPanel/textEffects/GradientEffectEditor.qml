import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../shared"

ColumnLayout {
    id: gradientEffectEditor

    property var editor: null
    property var selectedBoxData: ({})
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    readonly property int gradientHorizontal: 1

    signal colorDialogRequested(string hex, string setter)

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    CheckBox {
        text: qsTr("Enabled")
        checked: gradientEffectEditor.selectedBoxData.gradient
        onClicked: gradientEffectEditor.editor.setSelectedGradientEnabled(checked)
    }

    Label {
        text: qsTr("Direction")
    }

    ComboBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        model: [qsTr("Vertical"), qsTr("Horizontal")]
        currentIndex: Math.min(gradientEffectEditor.selectedBoxData.gradientDirection, gradientEffectEditor.gradientHorizontal)
        onActivated: gradientEffectEditor.editor.setSelectedGradientDirection(currentIndex)
    }

    Label {
        text: qsTr("Color A")
    }

    ColorButton {
        swatchText: gradientEffectEditor.qmlColorProvider(gradientEffectEditor.selectedBoxData.gradientColorA)
        swatchColor: swatchText
        onClicked: gradientEffectEditor.colorDialogRequested(swatchText, "gradientA")
    }

    Label {
        text: qsTr("Color B")
    }

    ColorButton {
        swatchText: gradientEffectEditor.qmlColorProvider(gradientEffectEditor.selectedBoxData.gradientColorB)
        swatchColor: swatchText
        onClicked: gradientEffectEditor.colorDialogRequested(swatchText, "gradientB")
    }
}
