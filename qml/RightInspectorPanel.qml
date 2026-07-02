import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: rightInspectorPanel

    property var editor: null
    property var editorLimits
    property int selectedBoxRevision: 0
    readonly property var selectedBoxData: ({
        rotation: selectedBoxValue("boxRotation", 0),
        perspective: selectedBoxValue("boxPerspective", false),
        outline: selectedBoxValue("boxOutline", false),
        outlineColor: selectedBoxValue("boxOutlineColor", "#ffffff"),
        outlineSize: selectedBoxValue("boxOutlineSize", 2),
        blur: selectedBoxValue("boxBlur", false),
        blurSize: selectedBoxValue("boxBlurSize", 0),
        shadow: selectedBoxValue("boxShadow", false),
        shadowColor: selectedBoxValue("boxShadowColor", "#000000"),
        shadowOffsetX: selectedBoxValue("boxShadowOffsetX", 4),
        shadowOffsetY: selectedBoxValue("boxShadowOffsetY", 4),
        shadowBlurSize: selectedBoxValue("boxShadowBlurSize", 0),
        gradient: selectedBoxValue("boxGradient", false),
        gradientDirection: selectedBoxValue("boxGradientDirection", 0),
        gradientColorA: selectedBoxValue("boxGradientColorA", "#ffffff"),
        gradientColorB: selectedBoxValue("boxGradientColorB", "#000000"),
        path: selectedBoxValue("boxPath", false),
        pathMode: selectedBoxValue("boxPathMode", 0)
    })
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    property real displayScale: 1
    property real minimumDisplayScale: 0.5
    property real maximumDisplayScale: 6

    signal colorDialogRequested(string hex, string setter)
    signal zoomRequested(real displayScale)

    function selectedBoxValue(roleName, fallback) {
        selectedBoxRevision;
        if (!editor || editor.selectedIndex < 0)
            return fallback;

        const value = editor.boxRole(editor.selectedIndex, roleName);
        return value === undefined || value === null ? fallback : value;
    }

    z: 1
    SplitView.minimumWidth: 180
    SplitView.preferredWidth: 260
    SplitView.fillHeight: true

    Connections {
        target: rightInspectorPanel.editor ? rightInspectorPanel.editor.boxesModel : null
        function onDataChanged() { rightInspectorPanel.selectedBoxRevision += 1; }
        function onModelReset() { rightInspectorPanel.selectedBoxRevision += 1; }
        function onRowsInserted() { rightInspectorPanel.selectedBoxRevision += 1; }
        function onRowsRemoved() { rightInspectorPanel.selectedBoxRevision += 1; }
    }

    ScrollView {
        id: rightPanelScroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: rightPanelScroll.availableWidth
            // Keep the layout at least viewport-tall so Layers can consume spare vertical space.
            height: Math.max(implicitHeight, rightPanelScroll.availableHeight)
            spacing: 8

            NavigationSection {
                editor: rightInspectorPanel.editor
                displayScale: rightInspectorPanel.displayScale
                minimumDisplayScale: rightInspectorPanel.minimumDisplayScale
                maximumDisplayScale: rightInspectorPanel.maximumDisplayScale
                onZoomRequested: (displayScale) => {
                    rightInspectorPanel.zoomRequested(displayScale);
                }
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            BoxEffectsSection {
                editor: rightInspectorPanel.editor
                selectedBoxData: rightInspectorPanel.selectedBoxData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            TextEffectsSection {
                editor: rightInspectorPanel.editor
                editorLimits: rightInspectorPanel.editorLimits
                selectedBoxData: rightInspectorPanel.selectedBoxData
                qmlColorProvider: rightInspectorPanel.qmlColorProvider
                onColorDialogRequested: (hex, setter) => {
                    rightInspectorPanel.colorDialogRequested(hex, setter);
                }
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            LayersSection {
                editor: rightInspectorPanel.editor
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.fillHeight: true

            }

        }

    }

}
