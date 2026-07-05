import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../shared"

ColumnLayout {
    id: outlineEffectEditor

    property var editor: null
    property var editorLimits
    property var selectedBoxData: ({})
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)

    Layout.fillWidth: true
    Layout.minimumWidth: 0

    Button {
        objectName: "rightInspectorAddOutlineLayerButton"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: qsTr("Add outline layer")
        onClicked: outlineEffectEditor.editor.addSelectedOutlineLayer()
    }

    Repeater {
        model: {
            const layers = outlineEffectEditor.selectedBoxData.outlineLayers || [];
            return layers.length > 0 ? layers : [{
                "synthetic": true,
                "enabled": outlineEffectEditor.selectedBoxData.outline,
                "color": outlineEffectEditor.selectedBoxData.outlineColor,
                "size": outlineEffectEditor.selectedBoxData.outlineSize
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
                        onClicked: index === 0 ? outlineEffectEditor.editor.setSelectedOutlineEnabled(checked) : outlineEffectEditor.editor.setSelectedOutlineLayerEnabled(index, checked)
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        objectName: "rightInspectorRemoveOutlineLayerButton"
                        text: qsTr("Remove")
                        visible: !modelData.synthetic
                        onClicked: outlineEffectEditor.editor.removeSelectedOutlineLayer(index)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Color")
                    }

                    ColorButton {
                        objectName: index === 0 ? "rightInspectorOutlineColorButton" : "rightInspectorOutlineLayerColorButton"
                        swatchText: outlineEffectEditor.qmlColorProvider(modelData.color)
                        swatchColor: swatchText
                        onClicked: outlineEffectEditor.colorDialogRequested(swatchText, index === 0 ? "outline" : "outlineLayer:" + index)
                    }
                }

                Label {
                    text: qsTr("Size")
                }

                SpinBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    from: outlineEffectEditor.editorLimits.minimumEffectSize
                    to: outlineEffectEditor.editorLimits.maximumEffectSize
                    value: modelData.size
                    onValueModified: index === 0 ? outlineEffectEditor.editor.setSelectedOutlineSize(value) : outlineEffectEditor.editor.setSelectedOutlineLayerSize(index, value)
                }
            }
        }
    }
}
