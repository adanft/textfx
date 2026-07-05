import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: boxEffectsSection

    property var editor: null
    property var selectedBoxData: ({})
    readonly property bool sectionReady: editor && editor.selectedIndex >= 0

    enabled: sectionReady

    Label {
        text: qsTr("Box Effects")
        font.bold: true
        enabled: boxEffectsSection.sectionReady
    }

    GroupBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: boxEffectsSection.sectionReady

        ColumnLayout {
            anchors.fill: parent
            spacing: UiConstants.panelSpacing

            TabBar {
                id: boxEffectsTabs

                objectName: "rightInspectorBoxEffectsTabs"
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                TabButton {
                    text: qsTr("Rotation")
                }

                TabButton {
                    text: qsTr("Perspective")
                }

            }

            StackLayout {
                currentIndex: boxEffectsTabs.currentIndex
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                ColumnLayout {
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
                        value: Math.round(boxEffectsSection.selectedBoxData.rotation)
                        onValueModified: boxEffectsSection.editor.setSelectedRotation(value)
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    CheckBox {
                        objectName: "rightInspectorPerspectiveEnabledCheckBox"
                        text: qsTr("Perspective Handles")
                        checked: boxEffectsSection.selectedBoxData.perspective
                        onClicked: boxEffectsSection.editor.setSelectedPerspectiveEnabled(checked)
                    }

                    Button {
                        objectName: "rightInspectorResetPerspectiveButton"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Reset Perspective")
                        onClicked: boxEffectsSection.editor.resetSelectedPerspective()
                    }

                }

            }

        }

    }

}
