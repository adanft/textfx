import QtQuick
import QtQuick.Controls
import TextFX.Ui 1.0

Rectangle {
    id: boxDelegate

    required property int boxIndex
    required property string boxText
    required property real boxX
    required property real boxY
    required property real boxWidth
    required property real boxHeight
    required property real boxRotation
    required property string boxFontFamily
    required property string boxResolvedFontFamily
    required property int boxFontSize
    required property string boxColor
    required property real boxLineSpacing
    required property real boxLetterSpacing
    required property bool boxBold
    required property bool boxItalic
    required property bool boxUppercase
    required property bool boxLowercase
    required property int boxAlignment
    required property bool boxOutline
    required property string boxOutlineColor
    required property int boxOutlineSize
    required property bool boxBlur
    required property int boxBlurSize
    required property bool boxShadow
    required property string boxShadowColor
    required property real boxShadowOffsetX
    required property real boxShadowOffsetY
    required property int boxShadowBlurSize
    required property bool boxGradient
    required property int boxGradientDirection
    required property string boxGradientColorA
    required property string boxGradientColorB
    required property bool boxPerspective
    required property var boxPerspectiveNw
    required property var boxPerspectiveNe
    required property var boxPerspectiveSe
    required property var boxPerspectiveSw
    required property bool boxPath
    required property int boxPathMode
    required property var boxPathPoints
    required property var boxEffects
    required property var canvasItem
    required property var interaction
    property bool renderTextContent: true
    property bool renderSelectionUi: true
    readonly property var rootWindow: ApplicationWindow.window
    readonly property var editorRef: rootWindow ? rootWindow.editor : null
    property bool selected: editorRef && boxIndex === editorRef.selectedIndex
    property bool editingSelected: selected && editorRef.editingText
    readonly property var boxModel: ({
        index: boxIndex,
        text: boxText,
        x: boxX,
        y: boxY,
        w: boxWidth,
        h: boxHeight,
        rotation: boxRotation,
        fontFamily: boxFontFamily,
        resolvedFontFamily: boxResolvedFontFamily,
        fontSize: boxFontSize,
        color: boxColor,
        lineSpacing: boxLineSpacing,
        letterSpacing: boxLetterSpacing,
        bold: boxBold,
        italic: boxItalic,
        uppercase: boxUppercase,
        lowercase: boxLowercase,
        alignment: boxAlignment,
        outline: boxOutline,
        outlineColor: boxOutlineColor,
        outlineSize: boxOutlineSize,
        outlineLayers: boxEffects && boxEffects.outline && boxEffects.outline.layers ? boxEffects.outline.layers : [],
        outlineLayersSet: boxEffects && boxEffects.outline ? boxEffects.outline.layersSet === true : false,
        blur: boxBlur,
        blurSize: boxBlurSize,
        shadow: boxShadow,
        shadowColor: boxShadowColor,
        shadowOffsetX: boxShadowOffsetX,
        shadowOffsetY: boxShadowOffsetY,
        shadowBlurSize: boxShadowBlurSize,
        gradient: boxGradient,
        gradientDirection: boxGradientDirection,
        gradientColorA: boxGradientColorA,
        gradientColorB: boxGradientColorB,
        perspective: boxPerspective,
        perspectiveNw: boxPerspectiveNw,
        perspectiveNe: boxPerspectiveNe,
        perspectiveSe: boxPerspectiveSe,
        perspectiveSw: boxPerspectiveSw,
        path: boxPath,
        pathMode: boxPathMode,
        pathPoints: boxPathPoints
    })
    property bool perspectiveActive: boxModel.perspective && !editingSelected
    property real visualDocX: boxModel.x
    property real visualDocY: boxModel.y
    property real visualDocW: boxModel.w
    property real visualDocH: boxModel.h
    readonly property bool textOverflow: textContent.overflow
    readonly property int zTextContent: 1
    readonly property int zPerspectiveBorder: 19
    readonly property int zSelectionControls: 20
    readonly property bool rotateDecorationsLoaded: selected || (rootWindow && rootWindow.activeRotateDelegate === boxDelegate)

    function modelPreviewText() {
        return boxModel.uppercase ? String(boxModel.text).toUpperCase() : boxModel.lowercase ? String(boxModel.text).toLowerCase() : boxModel.text;
    }

    function textHorizontalAlignment(alignment) {
        return alignment === textAlignCenter ? Text.AlignHCenter : alignment === textAlignRight ? Text.AlignRight : Text.AlignLeft;
    }

    readonly property int textAlignLeft: 0
    readonly property int textAlignCenter: 1
    readonly property int textAlignRight: 2

    objectName: renderSelectionUi ? "textBoxDelegate" : "textBoxContentDelegate"
    z: renderSelectionUi ? 40 : 0
    x: rootWindow.documentToViewX(visualDocX)
    y: rootWindow.documentToViewY(visualDocY)
    width: visualDocW * rootWindow.viewDocScale()
    height: visualDocH * rootWindow.viewDocScale()
    color: "transparent"
    border.width: !renderSelectionUi || perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))
    border.color: textOverflow ? Qt.rgba(1, 0, 0, 1) : editingSelected ? Qt.rgba(1, 0.84, 0, 1) : selected ? rootWindow.palette.highlight : rootWindow.palette.mid
    rotation: boxRotation

    Canvas {
        id: perspectiveBorder

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        property real margin: rootWindow.perspectiveMargin(boxRef.boxModel)

        objectName: "perspectiveBorder"
        x: -margin
        y: -margin
        width: parent.width + margin * 2
        height: parent.height + margin * 2
        visible: boxRef.renderSelectionUi && boxRef.selected && boxRef.perspectiveActive
        z: boxRef.zPerspectiveBorder
        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            const nw = rootWindow.perspectiveCorner(boxRef.boxModel, "nw", boxRef.width, boxRef.height);
            const ne = rootWindow.perspectiveCorner(boxRef.boxModel, "ne", boxRef.width, boxRef.height);
            const se = rootWindow.perspectiveCorner(boxRef.boxModel, "se", boxRef.width, boxRef.height);
            const sw = rootWindow.perspectiveCorner(boxRef.boxModel, "sw", boxRef.width, boxRef.height);
            ctx.beginPath();
            ctx.moveTo(nw.x + margin, nw.y + margin);
            ctx.lineTo(ne.x + margin, ne.y + margin);
            ctx.lineTo(se.x + margin, se.y + margin);
            ctx.lineTo(sw.x + margin, sw.y + margin);
            ctx.closePath();
            ctx.lineWidth = rootWindow.selectionLineWidth();
            ctx.strokeStyle = rootWindow.palette.highlight;
            ctx.stroke();
        }

        Connections {
            function onZoomChanged() {
                perspectiveBorder.requestPaint();
            }

            function onPerspectiveRevisionChanged() {
                perspectiveBorder.requestPaint();
            }

            target: perspectiveBorder.rootWindow
        }

        Connections {
            function onDocumentChanged() {
                perspectiveBorder.requestPaint();
            }

            target: perspectiveBorder.editorRef
        }

    }

    TextBoxContent {
        id: textContent

        boxRef: boxDelegate
    }

    TextBoxEditControls {
        boxRef: boxDelegate
        canvasItem: boxDelegate.canvasItem
        outlinedTextItem: textContent.outlinedTextItem
    }

    TextResizeHandles {
        boxRef: boxDelegate
        canvasItem: boxDelegate.canvasItem
        visible: boxDelegate.renderSelectionUi
    }

    Loader {
        id: rotateHandleLoader

        property var boxRef: boxDelegate
        property var canvasItem: boxDelegate.canvasItem

        active: boxDelegate.renderSelectionUi && boxDelegate.rotateDecorationsLoaded
        sourceComponent: Component {
            TextRotateHandle {
                boxRef: rotateHandleLoader.boxRef
                canvasItem: rotateHandleLoader.canvasItem
            }
        }
        z: boxDelegate.zSelectionControls
    }

    TextBoxPathControls {
        boxRef: boxDelegate
        canvasItem: boxDelegate.canvasItem
    }


}
