import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../constants/UiConstants.js" as UiConstants

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

                RotationEffectEditor {
                    editor: boxEffectsSection.editor
                    selectedBoxData: boxEffectsSection.selectedBoxData
                }

                PerspectiveEffectEditor {
                    editor: boxEffectsSection.editor
                    selectedBoxData: boxEffectsSection.selectedBoxData
                }
            }

        }

    }

}
