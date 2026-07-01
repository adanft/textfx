import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: rightInspectorPanel

    property var editor: null
    property var editorLimits
    property var selectedBoxProvider: function() {
        return null;
    }
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)

    function selectedBox() {
        return selectedBoxProvider ? selectedBoxProvider() : null;
    }

    z: 1
    SplitView.minimumWidth: 180
    SplitView.preferredWidth: 260
    SplitView.fillHeight: true

    ScrollView {
        id: rightPanelScroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: rightPanelScroll.availableWidth
            spacing: 8

            ColumnLayout {
                id: navigationSection

                readonly property bool sectionReady: rightInspectorPanel.editor.pageCount > 0

                Layout.fillWidth: true
                Layout.minimumWidth: 0

                Label {
                    text: qsTr("Navigation")
                    font.bold: true
                    enabled: navigationSection.sectionReady
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    enabled: navigationSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label {
                            text: rightInspectorPanel.editor.currentPageName.length > 0 ? rightInspectorPanel.editor.currentPageName : qsTr("No page")
                            font.bold: true
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6

                            Button {
                                text: qsTr("Previous")
                                enabled: rightInspectorPanel.editor.canGoPrevious
                                onClicked: rightInspectorPanel.editor.previousPage()
                            }

                            Button {
                                objectName: "rightInspectorNextPageButton"
                                text: qsTr("Next")
                                enabled: rightInspectorPanel.editor.canGoNext
                                onClicked: rightInspectorPanel.editor.nextPage()
                            }

                        }

                        ComboBox {
                            id: pageSelect

                            objectName: "rightInspectorPageSelect"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: rightInspectorPanel.editor.pageLabels
                            currentIndex: rightInspectorPanel.editor.currentPageIndex
                            enabled: rightInspectorPanel.editor.pageCount > 0
                            onActivated: (index) => {
                                return rightInspectorPanel.editor.goToPage(index);
                            }

                            contentItem: Label {
                                text: pageSelect.displayText
                                font: pageSelect.font
                                enabled: pageSelect.enabled
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                        }

                        Label {
                            text: rightInspectorPanel.editor.pageCount > 0 ? qsTr("Page %1 of %2").arg(rightInspectorPanel.editor.currentPageIndex + 1).arg(rightInspectorPanel.editor.pageCount) : qsTr("Page 0 of 0")
                            color: rightInspectorPanel.palette.mid
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                        }

                        CheckBox {
                            objectName: "rightInspectorRawVisibleCheckBox"
                            text: qsTr("Show Raw")
                            checked: rightInspectorPanel.editor.rawVisible
                            onToggled: rightInspectorPanel.editor.rawVisible = checked
                        }

                    }

                }

            }

            ColumnLayout {
                id: boxEffectsSection

                readonly property bool sectionReady: rightInspectorPanel.editor.selectedIndex >= 0

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                enabled: sectionReady

                Label {
                    text: qsTr("Box Effects")
                    font.bold: true
                    enabled: boxEffectsSection.sectionReady
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    enabled: boxEffectsSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        TabBar {
                            id: boxEffectsTabs

                            objectName: "rightInspectorBoxEffectsTabs"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0

                            TabButton {
                                text: qsTr("Rotation")
                            }

                            TabButton {
                                text: qsTr("Perspective")
                            }

                        }

                        StackLayout {
                            currentIndex: boxEffectsTabs.currentIndex
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                Label {
                                    text: qsTr("Rotation")
                                }

                                SpinBox {
                                    objectName: "rightInspectorRotationSpinBox"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    from: -360
                                    to: 360
                                    value: rightInspectorPanel.selectedBox() ? Math.round(rightInspectorPanel.selectedBox().rotation) : 0
                                    onValueModified: rightInspectorPanel.editor.setSelectedRotation(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    objectName: "rightInspectorPerspectiveEnabledCheckBox"
                                    text: qsTr("Perspective Handles")
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().perspective : false
                                    onClicked: rightInspectorPanel.editor.setSelectedPerspectiveEnabled(checked)
                                }

                                Button {
                                    objectName: "rightInspectorResetPerspectiveButton"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Reset Perspective")
                                    onClicked: rightInspectorPanel.editor.resetSelectedPerspective()
                                }

                            }

                        }

                    }

                }

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
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().outline : false
                                    onClicked: rightInspectorPanel.editor.setSelectedOutlineEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Color")
                                }

                                ColorButton {
                                    objectName: "rightInspectorOutlineColorButton"
                                    swatchText: rightInspectorPanel.selectedBox() ? rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBox().outlineColor) : "#ffffff"
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
                                    value: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().outlineSize : 2
                                    onValueModified: rightInspectorPanel.editor.setSelectedOutlineSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().blur : false
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
                                    value: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().blurSize : 0
                                    onValueModified: rightInspectorPanel.editor.setSelectedBlurSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().shadow : false
                                    onClicked: rightInspectorPanel.editor.setSelectedShadowEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Color")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.selectedBox() ? rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBox().shadowColor) : "#000000"
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
                                    value: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().shadowOffsetX : 4
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
                                    value: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().shadowOffsetY : 4
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
                                    value: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().shadowBlurSize : 0
                                    onValueModified: rightInspectorPanel.editor.setSelectedShadowBlurSize(value)
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0

                                CheckBox {
                                    text: qsTr("Enabled")
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().gradient : false
                                    onClicked: rightInspectorPanel.editor.setSelectedGradientEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Direction")
                                }

                                ComboBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    model: [qsTr("Vertical"), qsTr("Horizontal")]
                                    currentIndex: rightInspectorPanel.selectedBox() ? Math.min(rightInspectorPanel.selectedBox().gradientDirection, 1) : 0
                                    onActivated: rightInspectorPanel.editor.setSelectedGradientDirection(currentIndex)
                                }

                                Label {
                                    text: qsTr("Color A")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.selectedBox() ? rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBox().gradientColorA) : "#ffffff"
                                    swatchColor: swatchText
                                    onClicked: rightInspectorPanel.colorDialogRequested(swatchText, "gradientA")
                                }

                                Label {
                                    text: qsTr("Color B")
                                }

                                ColorButton {
                                    swatchText: rightInspectorPanel.selectedBox() ? rightInspectorPanel.qmlColorProvider(rightInspectorPanel.selectedBox().gradientColorB) : "#000000"
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
                                    checked: rightInspectorPanel.selectedBox() ? rightInspectorPanel.selectedBox().path : false
                                    onClicked: rightInspectorPanel.editor.setSelectedPathEnabled(checked)
                                }

                                Label {
                                    text: qsTr("Mode")
                                }

                                ComboBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    model: [qsTr("Straight"), qsTr("Smooth")]
                                    currentIndex: rightInspectorPanel.selectedBox() ? Math.min(rightInspectorPanel.selectedBox().pathMode, 1) : 0
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

            ColumnLayout {
                id: layersSection

                readonly property bool sectionReady: rightInspectorPanel.editor.layers.length > 0

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.fillHeight: true
                enabled: sectionReady

                Label {
                    text: qsTr("Layers")
                    font.bold: true
                    enabled: layersSection.sectionReady
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.fillHeight: true
                    enabled: layersSection.sectionReady

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Flow {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6

                            Button {
                                objectName: "rightInspectorLayerUpButton"
                                text: qsTr("Up")
                                enabled: rightInspectorPanel.editor.selectedIndex >= 0 && rightInspectorPanel.editor.selectedIndex < rightInspectorPanel.editor.boxes.length - 1
                                onClicked: rightInspectorPanel.editor.moveLayer(rightInspectorPanel.editor.selectedIndex + 1)
                            }

                            Button {
                                objectName: "rightInspectorLayerDownButton"
                                text: qsTr("Down")
                                enabled: rightInspectorPanel.editor.selectedIndex > 0
                                onClicked: rightInspectorPanel.editor.moveLayer(rightInspectorPanel.editor.selectedIndex - 1)
                            }

                        }

                        ListView {
                            objectName: "rightInspectorLayersListView"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: 160
                            Layout.fillHeight: true
                            clip: true
                            model: rightInspectorPanel.editor.layers

                            delegate: ItemDelegate {
                                id: layerDelegate

                                objectName: "rightInspectorLayerDelegate"
                                width: ListView.view.width
                                text: qsTr("Layer %1 — %2").arg(modelData.index + 1).arg(modelData.text.length > 0 ? modelData.text : qsTr("Text"))
                                highlighted: modelData.index === rightInspectorPanel.editor.selectedIndex
                                onClicked: rightInspectorPanel.editor.selectBox(modelData.index)

                                contentItem: Label {
                                    text: layerDelegate.text
                                    font: layerDelegate.font
                                    enabled: layerDelegate.enabled
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }

                            }

                        }

                    }

                }

            }

        }

    }

}
