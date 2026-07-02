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
