import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: paintSection

    property alias paintMode: paintModeCheck.checked
    property alias eraserMode: eraserModeCheck.checked
    property alias paintTarget: targetCombo.currentValue
    property string brushColor: "ff0000ff"
    property alias brushSize: brushSizeSpin.value
    property alias brushOpacity: opacitySlider.value
    property alias eraserSize: eraserSizeSpin.value
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)

    spacing: 6

    CheckBox {
        id: paintModeCheck
        objectName: "paintModeCheck"
        text: qsTr("Paint mode")
        Layout.fillWidth: true
    }

    CheckBox {
        id: eraserModeCheck
        objectName: "eraserModeCheck"
        text: qsTr("Eraser")
        enabled: paintModeCheck.checked
        Layout.fillWidth: true
    }

    ComboBox {
        id: targetCombo
        objectName: "paintTargetCombo"
        textRole: "text"
        valueRole: "value"
        model: [{ text: qsTr("Behind text"), value: "behind_text" }, { text: qsTr("Above text"), value: "above_text" }]
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: qsTr("Color")
        }

        ColorButton {
            objectName: "paintColorButton"
            swatchText: paintSection.qmlColorProvider(paintSection.brushColor)
            swatchColor: swatchText
            enabled: !eraserModeCheck.checked
            onClicked: paintSection.colorDialogRequested(swatchText, "paint")
        }
    }

    RowLayout {
        Label { text: qsTr("Brush") }
        SpinBox {
            id: brushSizeSpin
            objectName: "paintBrushSizeSpin"
            from: 1
            to: 128
            value: 12
            enabled: !eraserModeCheck.checked
            Layout.fillWidth: true
        }
    }

    RowLayout {
        Label { text: qsTr("Opacity") }
        Slider {
            id: opacitySlider
            objectName: "paintOpacitySlider"
            from: 0.05
            to: 1
            value: 1
            enabled: !eraserModeCheck.checked
            Layout.fillWidth: true
        }
    }

    RowLayout {
        Label { text: qsTr("Eraser") }
        SpinBox {
            id: eraserSizeSpin
            objectName: "paintEraserSizeSpin"
            from: 1
            to: 256
            value: 12
            Layout.fillWidth: true
        }
    }
}
