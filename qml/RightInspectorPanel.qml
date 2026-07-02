import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: rightInspectorPanel

    property var editor: null
    property var editorLimits
    readonly property var selectedBoxData: ({
        rotation: selectedBoxState.value("boxRotation", 0),
        perspective: selectedBoxState.value("boxPerspective", false),
        outline: selectedBoxState.value("boxOutline", false),
        outlineColor: selectedBoxState.value("boxOutlineColor", "#ffffff"),
        outlineSize: selectedBoxState.value("boxOutlineSize", 2),
        blur: selectedBoxState.value("boxBlur", false),
        blurSize: selectedBoxState.value("boxBlurSize", 0),
        shadow: selectedBoxState.value("boxShadow", false),
        shadowColor: selectedBoxState.value("boxShadowColor", "#000000"),
        shadowOffsetX: selectedBoxState.value("boxShadowOffsetX", 4),
        shadowOffsetY: selectedBoxState.value("boxShadowOffsetY", 4),
        shadowBlurSize: selectedBoxState.value("boxShadowBlurSize", 0),
        gradient: selectedBoxState.value("boxGradient", false),
        gradientDirection: selectedBoxState.value("boxGradientDirection", 0),
        gradientColorA: selectedBoxState.value("boxGradientColorA", "#ffffff"),
        gradientColorB: selectedBoxState.value("boxGradientColorB", "#000000"),
        path: selectedBoxState.value("boxPath", false),
        pathMode: selectedBoxState.value("boxPathMode", 0)
    })
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    property real displayScale: 1
    property real minimumDisplayScale: 0.5
    property real maximumDisplayScale: 6

    signal colorDialogRequested(string hex, string setter)
    signal zoomRequested(real displayScale)

    z: 1
    SplitView.minimumWidth: 180
    SplitView.preferredWidth: 260
    SplitView.fillHeight: true

    SelectedBoxState {
        id: selectedBoxState
        editor: rightInspectorPanel.editor
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
