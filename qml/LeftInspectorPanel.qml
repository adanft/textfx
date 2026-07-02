import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: leftInspectorPanel

    property var editor: null
    property var editorLimits
    property int selectedBoxRevision: 0
    readonly property var selectedBoxData: ({
        fontFamily: selectedBoxValue("boxFontFamily", ""),
        fontSize: selectedBoxValue("boxFontSize", 16),
        lineSpacing: selectedBoxValue("boxLineSpacing", 0),
        letterSpacing: selectedBoxValue("boxLetterSpacing", 0),
        color: selectedBoxValue("boxColor", "#000000"),
        bold: selectedBoxValue("boxBold", false),
        italic: selectedBoxValue("boxItalic", false),
        uppercase: selectedBoxValue("boxUppercase", false),
        lowercase: selectedBoxValue("boxLowercase", false),
        alignment: selectedBoxValue("boxAlignment", 0)
    })
    property var fontFamilyOptionsProvider: function(selected) {
        return selected ? [selected] : [];
    }
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)
    signal textInputFocusChanged(bool active)

    function selectedBoxValue(roleName, fallback) {
        selectedBoxRevision;
        if (!editor || editor.selectedIndex < 0)
            return fallback;

        const value = editor.boxRole(editor.selectedIndex, roleName);
        return value === undefined || value === null ? fallback : value;
    }

    z: 1
    SplitView.minimumWidth: 240
    SplitView.preferredWidth: 280
    SplitView.fillHeight: true

    Connections {
        target: leftInspectorPanel.editor ? leftInspectorPanel.editor.boxesModel : null
        function onDataChanged() { leftInspectorPanel.selectedBoxRevision += 1; }
        function onModelReset() { leftInspectorPanel.selectedBoxRevision += 1; }
        function onRowsInserted() { leftInspectorPanel.selectedBoxRevision += 1; }
        function onRowsRemoved() { leftInspectorPanel.selectedBoxRevision += 1; }
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

                Label {
                    text: qsTr("Text Properties")
                    font.bold: true
                    enabled: textPropertiesSection.sectionReady
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    enabled: textPropertiesSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label {
                            text: qsTr("Font Family")
                        }

                        ComboBox {
                            id: fontFamilyCombo

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: leftInspectorPanel.fontFamilyOptionsProvider(leftInspectorPanel.selectedBoxData.fontFamily)
                            currentIndex: Math.max(0, indexOfValue(leftInspectorPanel.selectedBoxData.fontFamily))
                            onActivated: leftInspectorPanel.editor.setSelectedFontFamily(currentText)

                            contentItem: Label {
                                text: fontFamilyCombo.displayText
                                font: fontFamilyCombo.font
                                enabled: fontFamilyCombo.enabled
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                Label {
                                    text: qsTr("Size")
                                }

                                SpinBox {
                                    objectName: "leftInspectorFontSizeSpinBox"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: leftInspectorPanel.editorLimits.minimumFontSize
                                    to: leftInspectorPanel.editorLimits.maximumFontSize
                                    value: leftInspectorPanel.selectedBoxData.fontSize
                                    onValueModified: leftInspectorPanel.editor.setSelectedFontSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                Label {
                                    text: qsTr("Leading")
                                }

                                SpinBox {
                                    objectName: "leftInspectorLineSpacingSpinBox"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: leftInspectorPanel.editorLimits.minimumTextSpacing
                                    to: leftInspectorPanel.editorLimits.maximumTextSpacing
                                    value: leftInspectorPanel.selectedBoxData.lineSpacing
                                    onValueModified: leftInspectorPanel.editor.setSelectedLineSpacing(value)
                                }

                            }

                        }

                        Label {
                            text: qsTr("Tracking")
                        }

                        SpinBox {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            from: leftInspectorPanel.editorLimits.minimumTextSpacing
                            to: leftInspectorPanel.editorLimits.maximumTextSpacing
                            value: leftInspectorPanel.selectedBoxData.letterSpacing
                            onValueModified: leftInspectorPanel.editor.setSelectedLetterSpacing(value)
                        }

                        Label {
                            text: qsTr("Text Color")
                        }

                        ColorButton {
                            objectName: "leftInspectorTextColorButton"
                            swatchText: leftInspectorPanel.qmlColorProvider(leftInspectorPanel.selectedBoxData.color)
                            swatchColor: swatchText
                            onClicked: leftInspectorPanel.colorDialogRequested(swatchText, "text")
                        }

                        Label {
                            text: qsTr("Style")
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6

                            TextStyleButton {
                                objectName: "leftInspectorBoldButton"
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/bold.svg"
                                accessibleLabel: qsTr("Bold")
                                checked: leftInspectorPanel.selectedBoxData.bold
                                onClicked: leftInspectorPanel.editor.setSelectedBold(checked)
                            }

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/italic.svg"
                                accessibleLabel: qsTr("Italic")
                                checked: leftInspectorPanel.selectedBoxData.italic
                                onClicked: leftInspectorPanel.editor.setSelectedItalic(checked)
                            }

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/uppercase.svg"
                                accessibleLabel: qsTr("Uppercase")
                                checked: leftInspectorPanel.selectedBoxData.uppercase
                                onClicked: leftInspectorPanel.editor.setSelectedUppercase(checked)
                            }

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/lowercase.svg"
                                accessibleLabel: qsTr("Lowercase")
                                checked: leftInspectorPanel.selectedBoxData.lowercase
                                onClicked: leftInspectorPanel.editor.setSelectedLowercase(checked)
                            }

                        }

                        Label {
                            text: qsTr("Paragraph")
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-left.svg"
                                accessibleLabel: qsTr("Align Left")
                                checked: leftInspectorPanel.selectedBoxData.alignment === 0
                                onClicked: leftInspectorPanel.editor.setSelectedAlignment(0)
                            }

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-center.svg"
                                accessibleLabel: qsTr("Align Center")
                                checked: leftInspectorPanel.selectedBoxData.alignment === 1
                                onClicked: leftInspectorPanel.editor.setSelectedAlignment(1)
                            }

                            TextStyleButton {
                                iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-right.svg"
                                accessibleLabel: qsTr("Align Right")
                                checked: leftInspectorPanel.selectedBoxData.alignment === 2
                                onClicked: leftInspectorPanel.editor.setSelectedAlignment(2)
                            }

                        }

                    }

                }

            }

            TextPresetsSection {
                editor: leftInspectorPanel.editor
                onTextInputFocusChanged: (active) => {
                    leftInspectorPanel.textInputFocusChanged(active);
                }
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            PageTextsSection {
                editor: leftInspectorPanel.editor
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.fillHeight: true
            }

        }

    }

}
