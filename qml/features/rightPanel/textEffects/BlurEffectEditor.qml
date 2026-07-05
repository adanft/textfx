import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: blurEffectEditor

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    CheckBox {
        text: qsTr("Enabled")
        checked: blurEffectEditor.selectedBoxData.blur
        onClicked: blurEffectEditor.editor.setSelectedBlurEnabled(checked)
    }

    Label {
        text: qsTr("Size")
    }

    SpinBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        from: blurEffectEditor.editorLimits.minimumEffectSize
        to: blurEffectEditor.editorLimits.maximumEffectSize
        value: blurEffectEditor.selectedBoxData.blurSize
        onValueModified: blurEffectEditor.editor.setSelectedBlurSize(value)
    }
}
