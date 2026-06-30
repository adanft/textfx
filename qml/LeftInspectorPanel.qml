import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: leftInspectorPanel

    property var editor: null
    property var editorLimits
    property var selectedBoxProvider: function() { return null }
    property var fontFamilyOptionsProvider: function(selected) { return selected ? [selected] : [] }
    property var qmlColorProvider: function(hex) { return hex }

    signal colorDialogRequested(string hex, string setter)
    signal textInputFocusChanged(bool active)

    z: 1
    SplitView.minimumWidth: 240
    SplitView.preferredWidth: 280
    SplitView.fillHeight: true

    function selectedBox() {
        return selectedBoxProvider ? selectedBoxProvider() : null
    }

    ScrollView {
        id: leftPanelScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: leftPanelScroll.availableWidth
            // Keep the layout at least viewport-tall so Page Texts can consume spare vertical space.
            height: Math.max(implicitHeight, leftPanelScroll.availableHeight)
            spacing: 8

             ColumnLayout {
                id: textPropertiesSection

                readonly property bool sectionReady: leftInspectorPanel.editor.selectedIndex >= 0

                 Layout.fillWidth: true
                 Layout.minimumWidth: 0
                enabled: sectionReady

                Label { text: qsTr("Text Properties"); font.bold: true; enabled: textPropertiesSection.sectionReady }

                 GroupBox {
                     Layout.fillWidth: true
                     Layout.minimumWidth: 0
                     enabled: textPropertiesSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label { text: qsTr("Font Family") }
                        ComboBox {
                            id: fontFamilyCombo
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: leftInspectorPanel.fontFamilyOptionsProvider(leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().fontFamily : "")
                            currentIndex: Math.max(0, indexOfValue(leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().fontFamily : ""))
                            contentItem: Label {
                                text: fontFamilyCombo.displayText
                                font: fontFamilyCombo.font
                                enabled: fontFamilyCombo.enabled
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            onActivated: leftInspectorPanel.editor.setSelectedFontFamily(currentText)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 8
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Label { text: qsTr("Size") }
                        SpinBox { objectName: "leftInspectorFontSizeSpinBox"; Layout.fillWidth: true; Layout.minimumWidth: 0; from: leftInspectorPanel.editorLimits.minimumFontSize; to: leftInspectorPanel.editorLimits.maximumFontSize; value: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().fontSize : 16; onValueModified: leftInspectorPanel.editor.setSelectedFontSize(value) }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Label { text: qsTr("Leading") }
                                SpinBox { objectName: "leftInspectorLineSpacingSpinBox"; Layout.fillWidth: true; Layout.minimumWidth: 0; from: leftInspectorPanel.editorLimits.minimumTextSpacing; to: leftInspectorPanel.editorLimits.maximumTextSpacing; value: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().lineSpacing : 0; onValueModified: leftInspectorPanel.editor.setSelectedLineSpacing(value) }
                            }
                        }

                        Label { text: qsTr("Tracking") }
                        SpinBox { Layout.fillWidth: true; Layout.minimumWidth: 0; from: leftInspectorPanel.editorLimits.minimumTextSpacing; to: leftInspectorPanel.editorLimits.maximumTextSpacing; value: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().letterSpacing : 0; onValueModified: leftInspectorPanel.editor.setSelectedLetterSpacing(value) }

                        Label { text: qsTr("Text Color") }
                        ColorButton { objectName: "leftInspectorTextColorButton"; swatchText: leftInspectorPanel.selectedBox() ? leftInspectorPanel.qmlColorProvider(leftInspectorPanel.selectedBox().color) : "#000000"; swatchColor: swatchText; onClicked: leftInspectorPanel.colorDialogRequested(swatchText, "text") }

                        Label { text: qsTr("Style") }
                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6
                            TextStyleButton { objectName: "leftInspectorBoldButton"; text: String.fromCodePoint(0xf0264); accessibleLabel: qsTr("Bold"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().bold : false; onClicked: leftInspectorPanel.editor.setSelectedBold(checked) }
                            TextStyleButton { text: String.fromCodePoint(0xf0277); accessibleLabel: qsTr("Italic"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().italic : false; onClicked: leftInspectorPanel.editor.setSelectedItalic(checked) }
                            TextStyleButton { text: String.fromCodePoint(0xf0b36); accessibleLabel: qsTr("Uppercase"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().uppercase : false; onClicked: leftInspectorPanel.editor.setSelectedUppercase(checked) }
                        }

                        Label { text: qsTr("Paragraph") }
                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6
                            TextStyleButton { text: String.fromCodePoint(0xf0262); accessibleLabel: qsTr("Align Left"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().alignment === 0 : false; onClicked: leftInspectorPanel.editor.setSelectedAlignment(0) }
                            TextStyleButton { text: String.fromCodePoint(0xf0260); accessibleLabel: qsTr("Align Center"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().alignment === 1 : false; onClicked: leftInspectorPanel.editor.setSelectedAlignment(1) }
                            TextStyleButton { text: String.fromCodePoint(0xf0263); accessibleLabel: qsTr("Align Right"); checked: leftInspectorPanel.selectedBox() ? leftInspectorPanel.selectedBox().alignment === 2 : false; onClicked: leftInspectorPanel.editor.setSelectedAlignment(2) }
                        }
                    }
                }
            }

             ColumnLayout {
                id: textPresetsSection

                readonly property bool sectionReady: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0

                 Layout.fillWidth: true
                 Layout.minimumWidth: 0
                enabled: sectionReady

                Label { text: qsTr("Text Presets"); font.bold: true; enabled: textPresetsSection.sectionReady }

                 GroupBox {
                     Layout.fillWidth: true
                     Layout.minimumWidth: 0
                     enabled: textPresetsSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            ComboBox {
                                id: presetSelect
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: leftInspectorPanel.editor.presets
                                textRole: "name"
                                currentIndex: leftInspectorPanel.editor.selectedPresetIndex >= 0 ? leftInspectorPanel.editor.selectedPresetIndex : (leftInspectorPanel.editor.presets.length > 0 ? 0 : -1)
                                contentItem: Label {
                                    text: presetSelect.displayText
                                    font: presetSelect.font
                                    enabled: presetSelect.enabled
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onActivated: index => leftInspectorPanel.editor.selectPreset(index)
                            }
                            Button { text: qsTr("Apply"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0 && leftInspectorPanel.editor.selectedPresetIndex >= 0; onClicked: leftInspectorPanel.editor.applySelectedPreset() }
                        }
                        TextField { id: presetNameField; Layout.fillWidth: true; Layout.minimumWidth: 0; placeholderText: qsTr("Preset name"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0; onActiveFocusChanged: leftInspectorPanel.textInputFocusChanged(activeFocus) }
                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6
                            Button { text: qsTr("Create"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0 && presetNameField.text.trim().length > 0; onClicked: { leftInspectorPanel.editor.addPreset(presetNameField.text); presetNameField.text = "" } }
                            Button { text: qsTr("Update"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0 && leftInspectorPanel.editor.selectedPresetIndex >= 0; onClicked: leftInspectorPanel.editor.updateSelectedPreset() }
                            Button { text: qsTr("Rename"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0 && leftInspectorPanel.editor.selectedPresetIndex >= 0 && presetNameField.text.trim().length > 0 && !(leftInspectorPanel.editor.presets[leftInspectorPanel.editor.selectedPresetIndex] || {}).isDefault; onClicked: { leftInspectorPanel.editor.renameSelectedPreset(presetNameField.text); presetNameField.text = "" } }
                            Button { text: qsTr("Delete"); enabled: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.selectedIndex >= 0 && leftInspectorPanel.editor.selectedPresetIndex >= 0 && !(leftInspectorPanel.editor.presets[leftInspectorPanel.editor.selectedPresetIndex] || {}).isDefault; onClicked: leftInspectorPanel.editor.deleteSelectedPreset() }
                        }
                    }
                }
            }

             ColumnLayout {
                id: pageTextsSection

                readonly property bool sectionReady: leftInspectorPanel.editor.hasProject && leftInspectorPanel.editor.pageTexts.length > 0

                 Layout.fillWidth: true
                 Layout.minimumWidth: 0
                 Layout.fillHeight: true
                enabled: sectionReady

                 RowLayout {
                     Layout.fillWidth: true
                     Layout.minimumWidth: 0
                     Label { text: qsTr("Page Texts"); font.bold: true; enabled: pageTextsSection.sectionReady }
                     Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; text: leftInspectorPanel.editor.pageTexts.length; enabled: pageTextsSection.sectionReady }
                 }

                 Frame {
                    id: pageTextsFrame

                    readonly property real minimumListHeight: 200

                     Layout.fillWidth: true
                       Layout.minimumWidth: 0
                     Layout.minimumHeight: minimumListHeight + topPadding + bottomPadding
                       Layout.fillHeight: true
                      enabled: pageTextsSection.sectionReady
                      background: Rectangle { color: pageTextsFrame.palette.base; border.color: pageTextsFrame.palette.mid; border.width: 1 }

                         ListView {
                          id: pageTextsList
                          objectName: "leftInspectorPageTextsList"

                         anchors.fill: parent
                         Layout.fillWidth: true
                         Layout.fillHeight: true
                        Layout.preferredHeight: pageTextsFrame.minimumListHeight
                          clip: true
                          model: leftInspectorPanel.editor.pageTexts
                         delegate: ItemDelegate {
                              id: pageTextDelegate
                              objectName: "leftInspectorPageTextDelegate"
                             width: ListView.view.width
                             hoverEnabled: true
                             text: modelData
                             contentItem: Label {
                                 text: pageTextDelegate.text
                                 font: pageTextDelegate.font
                                 enabled: pageTextDelegate.enabled
                                 elide: Text.ElideRight
                                 verticalAlignment: Text.AlignVCenter
                             }
                             background: Rectangle { color: pageTextDelegate.down || pageTextDelegate.hovered ? Qt.alpha(pageTextDelegate.palette.highlight, pageTextDelegate.down ? 0.24 : 0.14) : "transparent" }
                             onClicked: leftInspectorPanel.editor.applyPageText(index)
                         }
                     }
                 }
            }
        }
    }
}
