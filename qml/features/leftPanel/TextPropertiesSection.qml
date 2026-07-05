import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants
import "../../shared"

ColumnLayout {
    id: textPropertiesSection

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})
    property var fontFamilyOptionsProvider: function(selected) {
        return selected ? [selected] : [];
    }
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    readonly property bool sectionReady: textPropertiesSection.editor && textPropertiesSection.editor.selectedIndex >= 0
    readonly property int textAlignLeft: 0
    readonly property int textAlignCenter: 1
    readonly property int textAlignRight: 2

    signal colorDialogRequested(string hex, string setter)

    enabled: sectionReady

    Layout.fillWidth: true
    Layout.minimumWidth: 0

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
            spacing: UiConstants.panelSpacing

            Label {
                text: qsTr("Font Family")
            }

            ComboBox {
                id: fontFamilyCombo

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: textPropertiesSection.fontFamilyOptionsProvider(textPropertiesSection.selectedBoxData.fontFamily)
                currentIndex: Math.max(0, indexOfValue(textPropertiesSection.selectedBoxData.fontFamily))
                onActivated: textPropertiesSection.editor.setSelectedFontFamily(currentText)

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
                spacing: UiConstants.inlineSpacing

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
                        from: textPropertiesSection.editorLimits.minimumFontSize
                        to: textPropertiesSection.editorLimits.maximumFontSize
                        value: textPropertiesSection.selectedBoxData.fontSize
                        onValueModified: textPropertiesSection.editor.setSelectedFontSize(value)
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
                        from: textPropertiesSection.editorLimits.minimumTextSpacing
                        to: textPropertiesSection.editorLimits.maximumTextSpacing
                        value: textPropertiesSection.selectedBoxData.lineSpacing
                        onValueModified: textPropertiesSection.editor.setSelectedLineSpacing(value)
                    }

                }

            }

            Label {
                text: qsTr("Tracking")
            }

            SpinBox {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                from: textPropertiesSection.editorLimits.minimumTextSpacing
                to: textPropertiesSection.editorLimits.maximumTextSpacing
                value: textPropertiesSection.selectedBoxData.letterSpacing
                onValueModified: textPropertiesSection.editor.setSelectedLetterSpacing(value)
            }

            Label {
                text: qsTr("Text Color")
            }

            ColorButton {
                objectName: "leftInspectorTextColorButton"
                swatchText: textPropertiesSection.qmlColorProvider(textPropertiesSection.selectedBoxData.color)
                swatchColor: swatchText
                onClicked: textPropertiesSection.colorDialogRequested(swatchText, "text")
            }

            Label {
                text: qsTr("Style")
            }

            Flow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: UiConstants.panelSpacing

                TextStyleButton {
                    objectName: "leftInspectorBoldButton"
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/bold.svg"
                    accessibleLabel: qsTr("Bold")
                    checked: textPropertiesSection.selectedBoxData.bold
                    onClicked: textPropertiesSection.editor.setSelectedBold(checked)
                }

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/italic.svg"
                    accessibleLabel: qsTr("Italic")
                    checked: textPropertiesSection.selectedBoxData.italic
                    onClicked: textPropertiesSection.editor.setSelectedItalic(checked)
                }

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/uppercase.svg"
                    accessibleLabel: qsTr("Uppercase")
                    checked: textPropertiesSection.selectedBoxData.uppercase
                    onClicked: textPropertiesSection.editor.setSelectedUppercase(checked)
                }

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/lowercase.svg"
                    accessibleLabel: qsTr("Lowercase")
                    checked: textPropertiesSection.selectedBoxData.lowercase
                    onClicked: textPropertiesSection.editor.setSelectedLowercase(checked)
                }

            }

            Label {
                text: qsTr("Paragraph")
            }

            Flow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: UiConstants.panelSpacing

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-left.svg"
                    accessibleLabel: qsTr("Align Left")
                    checked: textPropertiesSection.selectedBoxData.alignment === textPropertiesSection.textAlignLeft
                    onClicked: textPropertiesSection.editor.setSelectedAlignment(textPropertiesSection.textAlignLeft)
                }

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-center.svg"
                    accessibleLabel: qsTr("Align Center")
                    checked: textPropertiesSection.selectedBoxData.alignment === textPropertiesSection.textAlignCenter
                    onClicked: textPropertiesSection.editor.setSelectedAlignment(textPropertiesSection.textAlignCenter)
                }

                TextStyleButton {
                    iconSource: "qrc:/qt/qml/TextFX/assets/icons/flat/align-right.svg"
                    accessibleLabel: qsTr("Align Right")
                    checked: textPropertiesSection.selectedBoxData.alignment === textPropertiesSection.textAlignRight
                    onClicked: textPropertiesSection.editor.setSelectedAlignment(textPropertiesSection.textAlignRight)
                }

            }

        }

    }

}
