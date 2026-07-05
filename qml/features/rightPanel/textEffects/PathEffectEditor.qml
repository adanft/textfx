import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: pathEffectEditor

    property var editor: null
    property var selectedBoxData: ({})
    readonly property int pathModeSmooth: 1

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    CheckBox {
        objectName: "rightInspectorPathEnabledCheckBox"
        text: qsTr("Path Handles")
        checked: pathEffectEditor.selectedBoxData.path
        onClicked: pathEffectEditor.editor.setSelectedPathEnabled(checked)
    }

    Label {
        text: qsTr("Mode")
    }

    ComboBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        model: [qsTr("Straight"), qsTr("Smooth")]
        currentIndex: Math.min(pathEffectEditor.selectedBoxData.pathMode, pathEffectEditor.pathModeSmooth)
        onActivated: pathEffectEditor.editor.setSelectedPathMode(currentIndex)
    }

    Button {
        objectName: "rightInspectorAddPathPointButton"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: qsTr("Add Path Point")
        onClicked: pathEffectEditor.editor.addSelectedPathPoint()
    }

    Button {
        objectName: "rightInspectorResetPathButton"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: qsTr("Reset Path")
        onClicked: pathEffectEditor.editor.resetSelectedPath()
    }
}
