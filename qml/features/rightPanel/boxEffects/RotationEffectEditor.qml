import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: rotationEffectEditor

    property var editor: null
    property var selectedBoxData: ({})

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    Label {
        text: qsTr("Rotation")
    }

    SpinBox {
        objectName: "rightInspectorRotationSpinBox"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        from: -360
        to: 360
        value: Math.round(rotationEffectEditor.selectedBoxData.rotation)
        onValueModified: rotationEffectEditor.editor.setSelectedRotation(value)
    }
}
