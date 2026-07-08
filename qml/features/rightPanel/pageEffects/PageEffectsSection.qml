import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: pageEffectsSection

    property alias paintMode: paintSection.paintMode
    property alias paintEraserMode: paintSection.eraserMode
    property alias paintTarget: paintSection.paintTarget
    property alias paintBrushColor: paintSection.brushColor
    property alias paintBrushSize: paintSection.brushSize
    property alias paintBrushOpacity: paintSection.brushOpacity
    property alias paintEraserSize: paintSection.eraserSize
    property var qmlColorProvider: function(hex) {
        return hex;
    }

    signal colorDialogRequested(string hex, string setter)

    Label {
        text: qsTr("Page Effects")
        font.bold: true
    }

    GroupBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: UiConstants.panelSpacing

            TabBar {
                id: pageEffectsTabs

                objectName: "rightInspectorPageEffectsTabs"
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                TabButton {
                    text: qsTr("Paint")
                }
            }

            StackLayout {
                currentIndex: pageEffectsTabs.currentIndex
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                PaintSection {
                    id: paintSection

                    objectName: "paintSection"
                    qmlColorProvider: pageEffectsSection.qmlColorProvider
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    onColorDialogRequested: (hex, setter) => {
                        pageEffectsSection.colorDialogRequested(hex, setter);
                    }
                }
            }
        }
    }
}
