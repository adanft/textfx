import QtQuick
import TextFX.Ui 1.0

Item {
    id: boxTextPerspective

    required property var boxRef
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef
    property alias outlinedTextItem: boxOutlinedText
    readonly property bool overflow: boxOutlinedText.overflow

    z: boxRef.zTextContent
    anchors.fill: parent
    clip: true

    OutlinedTextItem {
        id: boxOutlinedText

        property var boxRef: boxTextPerspective.boxRef
        property var rootWindow: boxTextPerspective.rootWindow
        property var editorRef: boxTextPerspective.editorRef

        objectName: "boxOutlinedText"
        width: boxRef.visualDocW * rootWindow.livePreviewScale()
        height: boxRef.visualDocH * rootWindow.livePreviewScale()
        transformOrigin: Item.TopLeft
        scale: rootWindow.viewDocScale() / rootWindow.livePreviewScale()
        text: boxRef.modelPreviewText()
        color: rootWindow.qmlColor(boxRef.boxModel.color)
        fontFamily: boxRef.boxModel.fontFamily
        pixelSize: Math.max(1, boxRef.boxModel.fontSize)
        bold: boxRef.boxModel.bold
        italic: boxRef.boxModel.italic
        letterSpacing: boxRef.boxModel.letterSpacing
        lineSpacing: boxRef.boxModel.lineSpacing
        horizontalAlignment: boxRef.textHorizontalAlignment(boxRef.boxModel.alignment)
        outlineColor: rootWindow.qmlColor(boxRef.boxModel.outlineColor)
        outlineSize: boxRef.boxModel.outline && !boxRef.boxModel.outlineLayersSet && boxRef.boxModel.outlineSize > 0 ? boxRef.boxModel.outlineSize : 0
        outlineLayers: boxRef.boxModel.outlineLayers
        blurSize: boxRef.boxModel.blur && boxRef.boxModel.blurSize > 0 ? boxRef.boxModel.blurSize : 0
        shadowEnabled: boxRef.boxModel.shadow
        shadowColor: rootWindow.qmlColor(boxRef.boxModel.shadowColor)
        shadowOffsetX: boxRef.boxModel.shadowOffsetX
        shadowOffsetY: boxRef.boxModel.shadowOffsetY
        shadowBlurSize: boxRef.boxModel.shadow && boxRef.boxModel.shadowBlurSize > 0 ? boxRef.boxModel.shadowBlurSize : 0
        gradientEnabled: boxRef.boxModel.gradient
        gradientDirection: boxRef.boxModel.gradientDirection
        gradientColorA: rootWindow.qmlColor(boxRef.boxModel.gradientColorA)
        gradientColorB: rootWindow.qmlColor(boxRef.boxModel.gradientColorB)
        pathEnabled: boxRef.boxModel.path && !boxRef.editingSelected
        pathMode: boxRef.boxModel.pathMode
        pathPoints: boxRef.boxModel.pathPoints
        renderScale: rootWindow.livePreviewScale()
    }

    transform: Matrix4x4 {
        matrix: boxTextPerspective.rootWindow.perspectiveMatrix(boxTextPerspective.boxRef.boxModel, boxTextPerspective.width, boxTextPerspective.height, boxTextPerspective.rootWindow.viewDocScale(), boxTextPerspective.boxRef.perspectiveActive)
    }

}
