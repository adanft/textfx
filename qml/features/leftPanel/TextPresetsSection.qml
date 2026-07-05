import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: textPresetsSection

    property var editor: null
    readonly property bool sectionReady: textPresetsSection.editor && textPresetsSection.editor.hasProject && textPresetsSection.editor.selectedIndex >= 0
    readonly property int presetCount: editor && editor.presets ? editor.presets.length : 0
    readonly property int selectedPresetIndex: editor ? editor.selectedPresetIndex : -1
    readonly property bool hasSelectedPreset: selectedPresetIndex >= 0 && selectedPresetIndex < presetCount

    signal textInputFocusChanged(bool active)

    enabled: sectionReady

    Label {
        text: qsTr("Text Presets")
        font.bold: true
        enabled: textPresetsSection.sectionReady
    }

    GroupBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: textPresetsSection.sectionReady

        ColumnLayout {
            anchors.fill: parent
            spacing: UiConstants.panelSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                ComboBox {
                    id: presetSelect

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    model: textPresetsSection.editor ? textPresetsSection.editor.presets : []
                    textRole: "name"
                    currentIndex: textPresetsSection.hasSelectedPreset ? textPresetsSection.selectedPresetIndex : (textPresetsSection.presetCount > 0 ? 0 : -1)
                    onActivated: (index) => {
                        return textPresetsSection.editor.selectPreset(index);
                    }

                    contentItem: Label {
                        text: presetSelect.displayText
                        font: presetSelect.font
                        enabled: presetSelect.enabled
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                }

                Button {
                    text: qsTr("Apply")
                    enabled: textPresetsSection.sectionReady && textPresetsSection.hasSelectedPreset
                    onClicked: textPresetsSection.editor.applySelectedPreset()
                }

            }

            TextField {
                id: presetNameField

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                placeholderText: qsTr("Preset name")
                enabled: textPresetsSection.sectionReady
                onActiveFocusChanged: textPresetsSection.textInputFocusChanged(activeFocus)
            }

            Flow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: UiConstants.panelSpacing

                Button {
                    text: qsTr("Create")
                    enabled: textPresetsSection.sectionReady && presetNameField.text.trim().length > 0
                    onClicked: {
                        textPresetsSection.editor.addPreset(presetNameField.text);
                        presetNameField.text = "";
                    }
                }

                Button {
                    text: qsTr("Update")
                    enabled: textPresetsSection.sectionReady && textPresetsSection.hasSelectedPreset
                    onClicked: textPresetsSection.editor.updateSelectedPreset()
                }

                Button {
                    text: qsTr("Rename")
                    enabled: textPresetsSection.sectionReady && textPresetsSection.hasSelectedPreset && presetNameField.text.trim().length > 0
                    onClicked: {
                        textPresetsSection.editor.renameSelectedPreset(presetNameField.text);
                        presetNameField.text = "";
                    }
                }

                Button {
                    text: qsTr("Delete")
                    enabled: textPresetsSection.sectionReady && textPresetsSection.hasSelectedPreset
                    onClicked: textPresetsSection.editor.deleteSelectedPreset()
                }

            }

        }

    }

}
