import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: layersSection

    property var editor: null
    readonly property bool sectionReady: editor && editor.layers.length > 0

    enabled: sectionReady

    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        Label {
            text: qsTr("Layers")
            font.bold: true
            enabled: layersSection.sectionReady
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: layersSection.editor.layers.length
            enabled: layersSection.sectionReady
        }

    }

    ColumnLayout {
        id: layersContent

        readonly property real minimumListHeight: 200

        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.minimumHeight: layersControls.implicitHeight + spacing + minimumListHeight
        Layout.fillHeight: true
        spacing: UiConstants.panelSpacing
        enabled: layersSection.sectionReady

        Flow {
            id: layersControls

            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: UiConstants.panelSpacing

            Button {
                objectName: "rightInspectorLayerUpButton"
                text: qsTr("Up")
                enabled: layersSection.editor.selectedIndex >= 0 && layersSection.editor.selectedIndex < layersSection.editor.boxCount - 1
                display: AbstractButton.IconOnly
                icon.source: "qrc:/qt/qml/TextFX/assets/icons/flat/arrow-up.svg"
                icon.width: UiConstants.iconSize
                icon.height: UiConstants.iconSize
                icon.color: !enabled ? palette.mid : palette.buttonText
                onClicked: layersSection.editor.moveLayer(layersSection.editor.selectedIndex + 1)
            }

            Button {
                objectName: "rightInspectorLayerDownButton"
                text: qsTr("Down")
                enabled: layersSection.editor.selectedIndex > 0
                display: AbstractButton.IconOnly
                icon.source: "qrc:/qt/qml/TextFX/assets/icons/flat/arrow-down.svg"
                icon.width: UiConstants.iconSize
                icon.height: UiConstants.iconSize
                icon.color: !enabled ? palette.mid : palette.buttonText
                onClicked: layersSection.editor.moveLayer(layersSection.editor.selectedIndex - 1)
            }

        }

        GroupBox {
            id: layersListFrame

            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: layersContent.minimumListHeight
            Layout.fillHeight: true

            ListView {
                objectName: "rightInspectorLayersListView"
                anchors.fill: parent
                clip: true
                model: layersSection.editor.layers

                delegate: ItemDelegate {
                    id: layerDelegate

                    readonly property string layerText: String(modelData.text).replace(/\s+/g, " ").trim()

                    objectName: "rightInspectorLayerDelegate"
                    width: ListView.view.width
                    text: qsTr("Layer %1 — %2").arg(modelData.index + 1).arg(layerText.length > 0 ? layerText : qsTr("Text"))
                    highlighted: modelData.index === layersSection.editor.selectedIndex
                    onClicked: layersSection.editor.selectBox(modelData.index)

                    contentItem: Label {
                        text: layerDelegate.text
                        font: layerDelegate.font
                        enabled: layerDelegate.enabled
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

        }

    }

}
