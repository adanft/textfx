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

            ColumnLayout {
                id: textEffectsSection

                readonly property bool sectionReady: rightInspectorPanel.editor.selectedIndex >= 0

                Layout.fillWidth: true
                Layout.minimumWidth: 0
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
                        spacing: 6

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

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    objectName: "rightInspectorOutlineEnabledCheckBox"
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBoxData.outline
                                    onClicked: rightInspectorPanel.editor.setSelectedOutlineEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Color")
                                }

                                ColorButton {
                                    objectName: "rightInspectorOutlineColorButton"
                                    swatchText: rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBoxData.outlineColor)
                                    swatchColor: swatchText
                                    onClicked: rightInspectorPanel.colorDialogRequested(swatchText, "outline")
                                }

                                Label {
                                    text: qsTr("Size")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: rightInspectorPanel.editorLimits.minimumEffectSize
                                    to: rightInspectorPanel.editorLimits.maximumEffectSize
                                    value: rightInspectorPanel.selectedBoxData.outlineSize
                                    onValueModified: rightInspectorPanel.editor.setSelectedOutlineSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBoxData.blur
                                    onClicked: rightInspectorPanel.editor.setSelectedBlurEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Size")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: rightInspectorPanel.editorLimits.minimumEffectSize
                                    to: rightInspectorPanel.editorLimits.maximumEffectSize
                                    value: rightInspectorPanel.selectedBoxData.blurSize
                                    onValueModified: rightInspectorPanel.editor.setSelectedBlurSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBoxData.shadow
                                    onClicked: rightInspectorPanel.editor.setSelectedShadowEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Color")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBoxData.shadowColor)
                                    swatchColor: swatchText
                                    onClicked: rightInspectorPanel.colorDialogRequested(swatchText, "shadow")
                                }

                                Label {
                                    text: qsTr("Offset X")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: rightInspectorPanel.editorLimits.minimumShadowOffset
                                    to: rightInspectorPanel.editorLimits.maximumShadowOffset
                                    value: rightInspectorPanel.selectedBoxData.shadowOffsetX
                                    onValueModified: rightInspectorPanel.editor.setSelectedShadowOffsetX(value)
                                }

                                Label {
                                    text: qsTr("Offset Y")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: rightInspectorPanel.editorLimits.minimumShadowOffset
                                    to: rightInspectorPanel.editorLimits.maximumShadowOffset
                                    value: rightInspectorPanel.selectedBoxData.shadowOffsetY
                                    onValueModified: rightInspectorPanel.editor.setSelectedShadowOffsetY(value)
                                }

                                Label {
                                    text: qsTr("Blur")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: rightInspectorPanel.editorLimits.minimumEffectSize
                                    to: rightInspectorPanel.editorLimits.maximumEffectSize
                                    value: rightInspectorPanel.selectedBoxData.shadowBlurSize
                                    onValueModified: rightInspectorPanel.editor.setSelectedShadowBlurSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBoxData.gradient
                                    onClicked: rightInspectorPanel.editor.setSelectedGradientEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Direction")
                                }

                                ComboBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    model: [qsTr("Vertical"), qsTr("Horizontal")]
                                    currentIndex: Math.min(rightInspectorPanel.selectedBoxData.gradientDirection, 1)
                                    onActivated: rightInspectorPanel.editor.setSelectedGradientDirection(currentIndex)
                                }

                                Label {
                                    text: qsTr("Color A")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBoxData.gradientColorA)
                                    swatchColor: swatchText
                                    onClicked: rightInspectorPanel.colorDialogRequested(swatchText, "gradientA")
                                }

                                Label {
                                    text: qsTr("Color B")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBoxData.gradientColorB)
                                    swatchColor: swatchText
                                    onClicked: rightInspectorPanel.colorDialogRequested(swatchText, "gradientB")
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    objectName: "rightInspectorPathEnabledCheckBox"
                                    text: qsTr("Path Handles")
                                    checked: rightInspectorPanel.selectedBoxData.path
                                    onClicked: rightInspectorPanel.editor.setSelectedPathEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Mode")
                                }

                                ComboBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    model: [qsTr("Straight"), qsTr("Smooth")]
                                    currentIndex: Math.min(rightInspectorPanel.selectedBoxData.pathMode, 1)
                                    onActivated: rightInspectorPanel.editor.setSelectedPathMode(currentIndex)
                                }

                                Button {
                                    objectName: "rightInspectorAddPathPointButton"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Add Path Point")
                                    onClicked: rightInspectorPanel.editor.addSelectedPathPoint()
                                }

                                Button {
                                    objectName: "rightInspectorResetPathButton"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Reset Path")
                                    onClicked: rightInspectorPanel.editor.resetSelectedPath()
                                }

                            }

                        }

                    }

                }

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
