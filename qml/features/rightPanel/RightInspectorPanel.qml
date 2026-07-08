import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants
import "../../shared"
import "boxEffects"
import "navigation"
import "pageEffects"
import "textEffects"
import "layers"

Pane {
    id: rightInspectorPanel

    property var editor: null
    property var editorLimits
    readonly property var selectedBoxData: ({
        rotation: selectedBoxState.value("boxRotation", 0),
        perspective: selectedBoxState.effectValue("perspective", "enabled", "boxPerspective", false),
        outline: selectedBoxState.effectValue("outline", "enabled", "boxOutline", false),
        outlineColor: selectedBoxState.effectValue("outline", "color", "boxOutlineColor", UiConstants.defaultWhite),
        outlineSize: selectedBoxState.effectValue("outline", "size", "boxOutlineSize", 2),
        outlineLayers: selectedBoxState.effectValue("outline", "layers", "boxOutlineLayers", []),
        blur: selectedBoxState.effectValue("blur", "enabled", "boxBlur", false),
        blurSize: selectedBoxState.effectValue("blur", "size", "boxBlurSize", 0),
        shadow: selectedBoxState.effectValue("shadow", "enabled", "boxShadow", false),
        shadowColor: selectedBoxState.effectValue("shadow", "color", "boxShadowColor", UiConstants.defaultBlack),
        shadowOffsetX: selectedBoxState.effectValue("shadow", "offsetX", "boxShadowOffsetX", 4),
        shadowOffsetY: selectedBoxState.effectValue("shadow", "offsetY", "boxShadowOffsetY", 4),
        shadowBlurSize: selectedBoxState.effectValue("shadow", "blurSize", "boxShadowBlurSize", 0),
        gradient: selectedBoxState.effectValue("gradient", "enabled", "boxGradient", false),
        gradientDirection: selectedBoxState.effectValue("gradient", "direction", "boxGradientDirection", 0),
        gradientColorA: selectedBoxState.effectValue("gradient", "colorA", "boxGradientColorA", UiConstants.defaultWhite),
        gradientColorB: selectedBoxState.effectValue("gradient", "colorB", "boxGradientColorB", UiConstants.defaultBlack),
        path: selectedBoxState.effectValue("path", "enabled", "boxPath", false),
        pathMode: selectedBoxState.effectValue("path", "mode", "boxPathMode", 0)
    })
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    property real displayScale: 1
    property real minimumDisplayScale: 0.5
    property real maximumDisplayScale: 6
    property bool pageNavigationEnabled: true
    property alias paintMode: pageEffectsSection.paintMode
    property alias paintEraserMode: pageEffectsSection.paintEraserMode
    property alias paintTarget: pageEffectsSection.paintTarget
    property alias paintBrushColor: pageEffectsSection.paintBrushColor
    property alias paintBrushSize: pageEffectsSection.paintBrushSize
    property alias paintBrushOpacity: pageEffectsSection.paintBrushOpacity
    property alias paintEraserSize: pageEffectsSection.paintEraserSize

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

        objectName: "rightPanelScroll"

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: rightPanelScroll.availableWidth
            // Keep the layout at least viewport-tall so Layers can consume spare vertical space.
            height: Math.max(implicitHeight, rightPanelScroll.availableHeight)
            spacing: UiConstants.inlineSpacing

            NavigationSection {
                editor: rightInspectorPanel.editor
                displayScale: rightInspectorPanel.displayScale
                minimumDisplayScale: rightInspectorPanel.minimumDisplayScale
                maximumDisplayScale: rightInspectorPanel.maximumDisplayScale
                pageNavigationEnabled: rightInspectorPanel.pageNavigationEnabled
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

            PageEffectsSection {
                id: pageEffectsSection
                objectName: "pageEffectsSection"
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
