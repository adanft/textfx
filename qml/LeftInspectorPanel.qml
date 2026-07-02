import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: leftInspectorPanel

    property var editor: null
    property var editorLimits
    readonly property var selectedBoxData: ({
        fontFamily: selectedBoxState.value("boxFontFamily", ""),
        fontSize: selectedBoxState.value("boxFontSize", 16),
        lineSpacing: selectedBoxState.value("boxLineSpacing", 0),
        letterSpacing: selectedBoxState.value("boxLetterSpacing", 0),
        color: selectedBoxState.value("boxColor", "#000000"),
        bold: selectedBoxState.value("boxBold", false),
        italic: selectedBoxState.value("boxItalic", false),
        uppercase: selectedBoxState.value("boxUppercase", false),
        lowercase: selectedBoxState.value("boxLowercase", false),
        alignment: selectedBoxState.value("boxAlignment", 0)
    })
    property var fontFamilyOptionsProvider: function(selected) {
        return selected ? [selected] : [];
    }
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)
    signal textInputFocusChanged(bool active)

    z: 1
    SplitView.minimumWidth: 240
    SplitView.preferredWidth: 280
    SplitView.fillHeight: true

    SelectedBoxState {
        id: selectedBoxState
        editor: leftInspectorPanel.editor
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

            TextPropertiesSection {
                editor: leftInspectorPanel.editor
                editorLimits: leftInspectorPanel.editorLimits
                selectedBoxData: leftInspectorPanel.selectedBoxData
                fontFamilyOptionsProvider: leftInspectorPanel.fontFamilyOptionsProvider
                qmlColorProvider: leftInspectorPanel.qmlColorProvider
                onColorDialogRequested: (hex, setter) => {
                    leftInspectorPanel.colorDialogRequested(hex, setter)
                }
                Layout.fillWidth: true
                Layout.minimumWidth: 0
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
