import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: perspectiveEffectEditor

    property var editor: null
    property var selectedBoxData: ({})

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    CheckBox {
        objectName: "rightInspectorPerspectiveEnabledCheckBox"
        text: qsTr("Perspective Handles")
        checked: perspectiveEffectEditor.selectedBoxData.perspective
        onClicked: perspectiveEffectEditor.editor.setSelectedPerspectiveEnabled(checked)
    }

    Button {
        objectName: "rightInspectorResetPerspectiveButton"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: qsTr("Reset Perspective")
        onClicked: perspectiveEffectEditor.editor.resetSelectedPerspective()
    }
}
