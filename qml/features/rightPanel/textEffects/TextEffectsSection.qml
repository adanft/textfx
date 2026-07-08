import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: textEffectsSection

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    readonly property bool sectionReady: editor && editor.selectedIndex >= 0

    signal colorDialogRequested(string hex, string setter)

    function forwardColorDialog(hex, setter) {
        colorDialogRequested(hex, setter);
    }

    enabled: sectionReady

    Label {
        text: qsTr("Text Effects")
        font.bold: true
        enabled: textEffectsSection.sectionReady
    }

    GroupBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: textEffectsSection.sectionReady

        ColumnLayout {
            anchors.fill: parent
            spacing: UiConstants.panelSpacing

            TabBar {
                id: textEffectsTabs

                objectName: "rightInspectorTextEffectsTabs"
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                TabButton {
                    text: qsTr("Outline")
                }

                TabButton {
                    text: qsTr("Blur")
                }

                TabButton {
                    text: qsTr("Shadow")
                }

                TabButton {
                    text: qsTr("Gradient")
                }

                TabButton {
                    text: qsTr("Path")
                }

            }

            StackLayout {
                currentIndex: textEffectsTabs.currentIndex
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                OutlineEffectEditor {
                    editor: textEffectsSection.editor
                    editorLimits: textEffectsSection.editorLimits
                    selectedBoxData: textEffectsSection.selectedBoxData
                    qmlColorProvider: textEffectsSection.qmlColorProvider
                    onColorDialogRequested: (hex, setter) => textEffectsSection.forwardColorDialog(hex, setter)
                }

                BlurEffectEditor {
                    editor: textEffectsSection.editor
                    editorLimits: textEffectsSection.editorLimits
                    selectedBoxData: textEffectsSection.selectedBoxData
                }

                ShadowEffectEditor {
                    editor: textEffectsSection.editor
                    editorLimits: textEffectsSection.editorLimits
                    selectedBoxData: textEffectsSection.selectedBoxData
                    qmlColorProvider: textEffectsSection.qmlColorProvider
                    onColorDialogRequested: (hex, setter) => textEffectsSection.forwardColorDialog(hex, setter)
                }

                GradientEffectEditor {
                    editor: textEffectsSection.editor
                    selectedBoxData: textEffectsSection.selectedBoxData
                    qmlColorProvider: textEffectsSection.qmlColorProvider
                    onColorDialogRequested: (hex, setter) => textEffectsSection.forwardColorDialog(hex, setter)
                }

                PathEffectEditor {
                    editor: textEffectsSection.editor
                    selectedBoxData: textEffectsSection.selectedBoxData
                }

            }

        }

    }

}
