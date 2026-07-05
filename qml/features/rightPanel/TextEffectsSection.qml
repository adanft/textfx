import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants
import "../../shared"

ColumnLayout {
    id: textEffectsSection

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})
    property var qmlColorProvider: function(hex) {
        return hex;
    }
    readonly property bool sectionReady: editor && editor.selectedIndex >= 0
    readonly property int gradientVertical: 0
    readonly property int gradientHorizontal: 1
    readonly property int pathModeStraight: 0
    readonly property int pathModeSmooth: 1

    signal colorDialogRequested(string hex, string setter)

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
            spacing: UiConstants.panelSpacing

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

                    Button {
                        objectName: "rightInspectorAddOutlineLayerButton"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Add outline layer")
                        onClicked: textEffectsSection.editor.addSelectedOutlineLayer()
                    }

                    Repeater {
                        model: {
                            const layers = textEffectsSection.selectedBoxData.outlineLayers || [];
                            return layers.length > 0 ? layers : [{
                                "synthetic": true,
                                "enabled": textEffectsSection.selectedBoxData.outline,
                                "color": textEffectsSection.selectedBoxData.outlineColor,
                                "size": textEffectsSection.selectedBoxData.outlineSize
                            }];
                        }

                        delegate: Frame {
                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true

                                    CheckBox {
                                        objectName: index === 0 ? "rightInspectorOutlineEnabledCheckBox" : "rightInspectorOutlineLayerEnabledCheckBox"
                                        text: qsTr("Layer %1").arg(index + 1)
                                        checked: modelData.enabled
                                        onClicked: index === 0 ? textEffectsSection.editor.setSelectedOutlineEnabled(checked) : textEffectsSection.editor.setSelectedOutlineLayerEnabled(index, checked)
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    Button {
                                        objectName: "rightInspectorRemoveOutlineLayerButton"
                                        text: qsTr("Remove")
                                        visible: !modelData.synthetic
                                        onClicked: textEffectsSection.editor.removeSelectedOutlineLayer(index)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: qsTr("Color")
                                    }

                                    ColorButton {
                                        objectName: index === 0 ? "rightInspectorOutlineColorButton" : "rightInspectorOutlineLayerColorButton"
                                        swatchText: textEffectsSection.qmlColorProvider(modelData.color)
                                        swatchColor: swatchText
                                        onClicked: textEffectsSection.colorDialogRequested(swatchText, index === 0 ? "outline" : "outlineLayer:" + index)
                                    }
                                }

                                Label {
                                    text: qsTr("Size")
                                }

                                SpinBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: textEffectsSection.editorLimits.minimumEffectSize
                                    to: textEffectsSection.editorLimits.maximumEffectSize
                                    value: modelData.size
                                    onValueModified: index === 0 ? textEffectsSection.editor.setSelectedOutlineSize(value) : textEffectsSection.editor.setSelectedOutlineLayerSize(index, value)
                                }
                            }
                        }
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    CheckBox {
                        text: qsTr("Enabled")
                        checked: textEffectsSection.selectedBoxData.blur
                        onClicked: textEffectsSection.editor.setSelectedBlurEnabled(checked)
                    }

                    Label {
                        text: qsTr("Size")
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        from: textEffectsSection.editorLimits.minimumEffectSize
                        to: textEffectsSection.editorLimits.maximumEffectSize
                        value: textEffectsSection.selectedBoxData.blurSize
                        onValueModified: textEffectsSection.editor.setSelectedBlurSize(value)
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    CheckBox {
                        text: qsTr("Enabled")
                        checked: textEffectsSection.selectedBoxData.shadow
                        onClicked: textEffectsSection.editor.setSelectedShadowEnabled(checked)
                    }

                    Label {
                        text: qsTr("Color")
                    }

                    ColorButton {
                        swatchText: textEffectsSection.qmlColorProvider(textEffectsSection.selectedBoxData.shadowColor)
                        swatchColor: swatchText
                        onClicked: textEffectsSection.colorDialogRequested(swatchText, "shadow")
                    }

                    Label {
                        text: qsTr("Offset X")
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        from: textEffectsSection.editorLimits.minimumShadowOffset
                        to: textEffectsSection.editorLimits.maximumShadowOffset
                        value: textEffectsSection.selectedBoxData.shadowOffsetX
                        onValueModified: textEffectsSection.editor.setSelectedShadowOffsetX(value)
                    }

                    Label {
                        text: qsTr("Offset Y")
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        from: textEffectsSection.editorLimits.minimumShadowOffset
                        to: textEffectsSection.editorLimits.maximumShadowOffset
                        value: textEffectsSection.selectedBoxData.shadowOffsetY
                        onValueModified: textEffectsSection.editor.setSelectedShadowOffsetY(value)
                    }

                    Label {
                        text: qsTr("Blur")
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        from: textEffectsSection.editorLimits.minimumEffectSize
                        to: textEffectsSection.editorLimits.maximumEffectSize
                        value: textEffectsSection.selectedBoxData.shadowBlurSize
                        onValueModified: textEffectsSection.editor.setSelectedShadowBlurSize(value)
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    CheckBox {
                        text: qsTr("Enabled")
                        checked: textEffectsSection.selectedBoxData.gradient
                        onClicked: textEffectsSection.editor.setSelectedGradientEnabled(checked)
                    }

                    Label {
                        text: qsTr("Direction")
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: [qsTr("Vertical"), qsTr("Horizontal")]
                        currentIndex: Math.min(textEffectsSection.selectedBoxData.gradientDirection, textEffectsSection.gradientHorizontal)
                        onActivated: textEffectsSection.editor.setSelectedGradientDirection(currentIndex)
                    }

                    Label {
                        text: qsTr("Color A")
                    }

                    ColorButton {
                        swatchText: textEffectsSection.qmlColorProvider(textEffectsSection.selectedBoxData.gradientColorA)
                        swatchColor: swatchText
                        onClicked: textEffectsSection.colorDialogRequested(swatchText, "gradientA")
                    }

                    Label {
                        text: qsTr("Color B")
                    }

                    ColorButton {
                        swatchText: textEffectsSection.qmlColorProvider(textEffectsSection.selectedBoxData.gradientColorB)
                        swatchColor: swatchText
                        onClicked: textEffectsSection.colorDialogRequested(swatchText, "gradientB")
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0

                    CheckBox {
                        objectName: "rightInspectorPathEnabledCheckBox"
                        text: qsTr("Path Handles")
                        checked: textEffectsSection.selectedBoxData.path
                        onClicked: textEffectsSection.editor.setSelectedPathEnabled(checked)
                    }

                    Label {
                        text: qsTr("Mode")
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: [qsTr("Straight"), qsTr("Smooth")]
                        currentIndex: Math.min(textEffectsSection.selectedBoxData.pathMode, textEffectsSection.pathModeSmooth)
                        onActivated: textEffectsSection.editor.setSelectedPathMode(currentIndex)
                    }

                    Button {
                        objectName: "rightInspectorAddPathPointButton"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Add Path Point")
                        onClicked: textEffectsSection.editor.addSelectedPathPoint()
                    }

                    Button {
                        objectName: "rightInspectorResetPathButton"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Reset Path")
                        onClicked: textEffectsSection.editor.resetSelectedPath()
                    }

                }

            }

        }

    }

}
