import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../shared"

ColumnLayout {
    id: shadowEffectEditor

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    CheckBox {
        text: qsTr("Enabled")
        checked: shadowEffectEditor.selectedBoxData.shadow
        onClicked: shadowEffectEditor.editor.setSelectedShadowEnabled(checked)
    }

    Label {
        text: qsTr("Color")
    }

    ColorButton {
        swatchText: shadowEffectEditor.qmlColorProvider(shadowEffectEditor.selectedBoxData.shadowColor)
        swatchColor: swatchText
        onClicked: shadowEffectEditor.colorDialogRequested(swatchText, "shadow")
    }

    Label {
        text: qsTr("Offset X")
    }

    SpinBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        from: shadowEffectEditor.editorLimits.minimumShadowOffset
        to: shadowEffectEditor.editorLimits.maximumShadowOffset
        value: shadowEffectEditor.selectedBoxData.shadowOffsetX
        onValueModified: shadowEffectEditor.editor.setSelectedShadowOffsetX(value)
    }

    Label {
        text: qsTr("Offset Y")
    }

    SpinBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        from: shadowEffectEditor.editorLimits.minimumShadowOffset
        to: shadowEffectEditor.editorLimits.maximumShadowOffset
        value: shadowEffectEditor.selectedBoxData.shadowOffsetY
        onValueModified: shadowEffectEditor.editor.setSelectedShadowOffsetY(value)
    }

    Label {
        text: qsTr("Blur")
    }

    SpinBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        from: shadowEffectEditor.editorLimits.minimumEffectSize
        to: shadowEffectEditor.editorLimits.maximumEffectSize
        value: shadowEffectEditor.selectedBoxData.shadowBlurSize
        onValueModified: shadowEffectEditor.editor.setSelectedShadowBlurSize(value)
    }
}
