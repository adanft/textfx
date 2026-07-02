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
    required property var canvasItem
    required property var interaction
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
        alignment: boxAlignment,
        outline: boxOutline,
        outlineColor: boxOutlineColor,
        outlineSize: boxOutlineSize,
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
    readonly property bool textOverflow: boxOutlinedText.overflow

    function modelPreviewText() {
        return boxModel.uppercase ? String(boxModel.text).toUpperCase() : boxModel.text;
    }

    objectName: "textBoxDelegate"
    x: rootWindow.documentToViewX(visualDocX)
    y: rootWindow.documentToViewY(visualDocY)
    width: visualDocW * rootWindow.viewDocScale()
    height: visualDocH * rootWindow.viewDocScale()
    color: "transparent"
    border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))
    border.color: textOverflow ? Qt.rgba(1, 0, 0, 1) : editingSelected ? Qt.rgba(1, 0.84, 0, 1) : selected ? rootWindow.palette.highlight : rootWindow.palette.mid
    rotation: boxRotation

    Canvas {
        id: perspectiveBorder

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        property real margin: rootWindow.perspectiveMargin(boxRef.boxModel)

        x: -margin
        y: -margin
        width: parent.width + margin * 2
        height: parent.height + margin * 2
        visible: boxRef.selected && boxRef.perspectiveActive
        z: 19
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

    Item {
        id: boxTextPerspective

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef

        z: 1
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
            horizontalAlignment: boxRef.boxModel.alignment === 1 ? Text.AlignHCenter : boxRef.boxModel.alignment === 2 ? Text.AlignRight : Text.AlignLeft
            outlineColor: rootWindow.qmlColor(boxRef.boxModel.outlineColor)
            outlineSize: boxRef.boxModel.outline && boxRef.boxModel.outlineSize > 0 ? boxRef.boxModel.outlineSize : 0
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

    Canvas {
        id: pathGuide

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        property int pathMode: boxRef.boxModel.pathMode
        property var pathPoints: boxRef.boxModel.pathPoints

        objectName: "pathGuide"
        function guidePoint(point) {
            return Qt.point(rootWindow.pointValue(point, 0, 0.5) * width, rootWindow.pointValue(point, 1, 0.5) * height);
        }

        function guideLineWidth() {
            return Math.max(2, rootWindow.selectionLineWidth());
        }

        function smoothGuidePoints(points) {
            let result = points.map((point) => {
                return guidePoint(point);
            });
            for (let pass = 0; pass < 3 && result.length >= 3; ++pass) {
                const next = [result[0]];
                for (let i = 0; i + 1 < result.length; ++i) {
                    const from = result[i];
                    const to = result[i + 1];
                    next.push(Qt.point(from.x + (to.x - from.x) * 0.25, from.y + (to.y - from.y) * 0.25));
                    next.push(Qt.point(from.x + (to.x - from.x) * 0.75, from.y + (to.y - from.y) * 0.75));
                }
                next.push(result[result.length - 1]);
                result = next;
            }
            return result;
        }

        anchors.fill: parent
        visible: boxRef.selected && boxRef.boxModel.path && boxRef.boxModel.pathPoints.length > 1
        z: 18
        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            const points = pathPoints;
            if (!points || points.length < 2)
                return ;

            const guidePoints = pathMode === 1 ? smoothGuidePoints(points) : points.map((point) => {
                return guidePoint(point);
            });
            const lineWidth = guideLineWidth();
            ctx.beginPath();
            ctx.moveTo(guidePoints[0].x, guidePoints[0].y);
            for (let i = 1; i < guidePoints.length; ++i) ctx.lineTo(guidePoints[i].x, guidePoints[i].y)
            ctx.lineWidth = lineWidth;
            ctx.strokeStyle = "#ffb000";
            ctx.stroke();
        }
        onVisibleChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPathModeChanged: requestPaint()
        onPathPointsChanged: requestPaint()

        Connections {
            function onZoomChanged() {
                pathGuide.requestPaint();
            }

            target: pathGuide.rootWindow
        }

        Connections {
            function onDocumentChanged() {
                pathGuide.requestPaint();
            }

            target: pathGuide.editorRef
        }

        transform: Matrix4x4 {
            matrix: pathGuide.rootWindow.perspectiveMatrix(pathGuide.boxRef.boxModel, pathGuide.width, pathGuide.height, pathGuide.rootWindow.viewDocScale(), pathGuide.boxRef.perspectiveActive)
        }

    }

    TextArea {
        id: boxTextArea

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        property real editLineSpacing: boxRef.boxModel.lineSpacing
        readonly property bool editLayoutAligned: boxOutlinedText.editLayoutMetricsValid
        readonly property real editLayoutTopPadding: editLayoutAligned ? boxOutlinedText.editLayoutTopPadding : 0
        readonly property real editLayoutLeftPadding: editLayoutAligned ? boxOutlinedText.editLayoutLeftPadding : 0
        readonly property real editLayoutRightPadding: editLayoutAligned ? boxOutlinedText.editLayoutRightPadding : 0
        readonly property real editLayoutTabStopDistance: boxOutlinedText.editLayoutTabStopDistance
        readonly property real editLayoutPaintOffsetX: editLayoutAligned ? boxOutlinedText.editLayoutPaintOffsetX : 0
        readonly property real editLayoutPaintOffsetY: editLayoutAligned ? boxOutlinedText.editLayoutPaintOffsetY : 0

        function focusForEdit() {
            if (boxRef.selected && editorRef.editingText && !activeFocus)
                forceActiveFocus();

        }

        function applyLineSpacing() {
            editorRef.applyTextLineSpacing(textDocument, editLineSpacing);
        }

        property bool syncingTextFromModel: false
        property bool userInputSyncPending: false
        property string livePreviewText: boxRef.modelPreviewText()
        property string pendingUserInputText: ""

        function modelText() {
            return boxRef.modelPreviewText();
        }

        function setLivePreviewText(nextText) {
            livePreviewText = nextText;
        }

        function syncTextFromModel() {
            const nextText = modelText();
            if (activeFocus && userInputSyncPending) {
                setLivePreviewText(text);
                userInputSyncPending = false;
                pendingUserInputText = "";
                applyLineSpacing();
                return ;
            }

            userInputSyncPending = false;
            pendingUserInputText = "";

            if (text === nextText) {
                setLivePreviewText(text);
                return ;
            }

            const oldCursor = cursorPosition;
            const oldSelectionStart = selectionStart;
            const oldSelectionEnd = selectionEnd;
            syncingTextFromModel = true;
            text = nextText;
            setLivePreviewText(nextText);
            cursorPosition = Math.min(oldCursor, text.length);
            select(Math.min(oldSelectionStart, text.length), Math.min(oldSelectionEnd, text.length));
            syncingTextFromModel = false;
            applyLineSpacing();
        }

        objectName: "boxTextArea"
        z: 1
        x: editLayoutPaintOffsetX * rootWindow.viewDocScale()
        y: editLayoutPaintOffsetY * rootWindow.viewDocScale()
        width: boxRef.visualDocW
        height: boxRef.visualDocH
        transformOrigin: Item.TopLeft
        scale: rootWindow.viewDocScale()
        clip: true
        visible: boxRef.selected && editorRef.editingText
        text: ""
        color: "transparent"
        selectedTextColor: "transparent"
        selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)
        placeholderTextColor: "transparent"
        font.family: boxRef.boxModel.resolvedFontFamily
        font.pixelSize: Math.round(Math.max(1, boxRef.boxModel.fontSize))
        font.bold: boxRef.boxModel.bold
        font.weight: boxRef.boxModel.bold ? Font.Bold : Font.Normal
        font.italic: boxRef.boxModel.italic
        font.letterSpacing: boxRef.boxModel.letterSpacing
        horizontalAlignment: boxRef.boxModel.alignment === 1 ? TextEdit.AlignHCenter : boxRef.boxModel.alignment === 2 ? TextEdit.AlignRight : TextEdit.AlignLeft
        padding: 0
        topPadding: editLayoutTopPadding
        leftPadding: editLayoutLeftPadding
        rightPadding: editLayoutRightPadding
        bottomPadding: 0
        background: null
        wrapMode: TextEdit.WordWrap
        tabStopDistance: editLayoutTabStopDistance
        selectByMouse: boxRef.selected && editorRef.editingText
        readOnly: !(boxRef.selected && editorRef.editingText)
        Component.onCompleted: {
            syncTextFromModel();
            applyLineSpacing();
            Qt.callLater(focusForEdit);
        }
        onVisibleChanged: {
            if (visible) {
                syncTextFromModel();
                applyLineSpacing();
                Qt.callLater(focusForEdit);
            }
        }
        onEditLineSpacingChanged: applyLineSpacing()
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                rootWindow.handleEscape();
                event.accepted = true;
            }
        }
        onActiveFocusChanged: {
            if (editorRef) {
                activeFocus ? editorRef.beginTextEdit() : editorRef.endTextEdit();
            }
        }
        onTextChanged: {
            setLivePreviewText(text);
            applyLineSpacing();
            if (activeFocus && editorRef && !syncingTextFromModel) {
                userInputSyncPending = true;
                pendingUserInputText = text;
                editorRef.updateSelectedText(text);
            }

        }

        Connections {
            function onSelectedBoxChanged() {
                if (boxTextArea.visible)
                    boxTextArea.syncTextFromModel();
            }

            target: boxTextArea.editorRef
        }

        cursorDelegate: Rectangle {
            width: Math.max(1, 2 / boxTextArea.rootWindow.viewDocScale())
            color: boxTextArea.rootWindow.palette.highlight

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: boxTextArea.visible && boxTextArea.activeFocus

                NumberAnimation {
                    to: 0
                    duration: 450
                }

                NumberAnimation {
                    to: 1
                    duration: 450
                }

            }

        }

    }

    MouseArea {
        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef
        readonly property var visualBounds: boxRef.perspectiveActive ? rootWindow.perspectiveVisualBounds(boxRef.boxModel, boxRef.width, boxRef.height) : ({
            "x": 0,
            "y": 0,
            "width": boxRef.width,
            "height": boxRef.height
        })

        z: boxRef.selected && editorRef.editingText ? -1 : 10
        x: visualBounds.x
        y: visualBounds.y
        width: visualBounds.width
        height: visualBounds.height
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        enabled: !(boxRef.selected && editorRef.editingText)
        onPressed: (mouse) => {
            const local = mapToItem(boxRef, mouse.x, mouse.y);
            if (boxRef.perspectiveActive && !rootWindow.pointInPerspectivePolygon(local, boxRef.boxModel, boxRef.width, boxRef.height)) {
                mouse.accepted = false;
                return ;
            }
            mouse.accepted = true;
            canvasItem.forceActiveFocus();
            editorRef.selectBox(boxRef.boxModel.index);
            editorRef.beginInteraction();
            const point = mapToItem(canvasItem, mouse.x, mouse.y);
            rootWindow.beginMoveDrag(boxRef, boxRef.boxModel.index, point.x, point.y);
        }
        onPositionChanged: (mouse) => {
            if (!pressed || boxRef.boxModel.index !== editorRef.selectedIndex)
                return ;

            const point = mapToItem(canvasItem, mouse.x, mouse.y);
            rootWindow.updateMoveDrag(point.x, point.y);
        }
        onReleased: {
            rootWindow.endMoveDrag(true);
            editorRef.endInteraction();
        }
        onCanceled: {
            rootWindow.endMoveDrag(false);
            editorRef.endInteraction();
        }
        onDoubleClicked: (mouse) => {
            editorRef.selectBox(boxRef.boxModel.index);
            editorRef.beginTextEdit();
            boxTextArea.forceActiveFocus();
        }
    }

    Repeater {
        model: [{
            "name": "nw"
        }, {
            "name": "n"
        }, {
            "name": "ne"
        }, {
            "name": "e"
        }, {
            "name": "se"
        }, {
            "name": "s"
        }, {
            "name": "sw"
        }, {
            "name": "w"
        }]

        delegate: Rectangle {
            id: resizeHandle

            property var boxRef: parent
            property var rootWindow: boxRef.rootWindow
            property var editorRef: boxRef.editorRef

            objectName: "resizeHandle_" + modelData.name
            z: 20
            width: rootWindow.handleSize()
            height: width
            radius: width / 2
            color: rootWindow.palette.highlight
            x: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).x - width / 2
            y: rootWindow.visualHandlePosition(boxRef.boxModel, modelData.name, boxRef.width, boxRef.height).y - height / 2
            visible: boxRef.selected

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                preventStealing: true
                cursorShape: Qt.SizeFDiagCursor
                onPressed: (mouse) => {
                    mouse.accepted = true;
                    resizeHandle.editorRef.selectBox(resizeHandle.boxRef.boxModel.index);
                    resizeHandle.editorRef.beginInteraction();
                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.beginPerspectiveDrag(resizeHandle.boxRef, modelData.name, point.x, point.y);
                    else
                        resizeHandle.rootWindow.beginResizeDrag(resizeHandle.boxRef, modelData.name, point.x, point.y);
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return ;

                    const point = mapToItem(canvasItem, mouse.x, mouse.y);
                    if (resizeHandle.boxRef.perspectiveActive) {
                        resizeHandle.rootWindow.updatePerspectiveDrag(point.x, point.y);
                        return ;
                    }
                    resizeHandle.rootWindow.updateResizeDrag(point.x, point.y);
                }
                onReleased: {
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.endPerspectiveDrag(true);
                    else
                        resizeHandle.rootWindow.endResizeDrag(true);
                    resizeHandle.editorRef.endInteraction();
                }
                onCanceled: {
                    if (resizeHandle.boxRef.perspectiveActive)
                        resizeHandle.rootWindow.endPerspectiveDrag(false);
                    else
                        resizeHandle.rootWindow.endResizeDrag(false);
                    resizeHandle.editorRef.endInteraction();
                }
            }

        }

    }

    Rectangle {
        id: rotateHandle

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow
        property var editorRef: boxRef.editorRef

        z: 20
        width: rootWindow.handleSize()
        height: width
        radius: width / 2
        color: rootWindow.palette.highlight
        x: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).x - width / 2
        y: rootWindow.rotateHandlePosition(boxRef.boxModel, boxRef.width, boxRef.height).y - height / 2
        visible: boxRef.selected

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            onPressed: (mouse) => {
                mouse.accepted = true;
                rotateHandle.editorRef.selectBox(rotateHandle.boxRef.boxModel.index);
                rotateHandle.editorRef.beginInteraction();
                const point = mapToItem(canvasItem, mouse.x, mouse.y);
                rotateHandle.rootWindow.beginRotateDrag(rotateHandle.boxRef, point.x, point.y);
            }
            onPositionChanged: (mouse) => {
                if (!pressed)
                    return ;

                const point = mapToItem(canvasItem, mouse.x, mouse.y);
                rotateHandle.rootWindow.updateRotateDrag(point.x, point.y);
            }
            onReleased: {
                rotateHandle.rootWindow.endRotateDrag(true);
                rotateHandle.editorRef.endInteraction();
            }
            onCanceled: {
                rotateHandle.rootWindow.endRotateDrag(false);
                rotateHandle.editorRef.endInteraction();
            }
        }

    }

    Item {
        id: pathHandlePlane

        property var boxRef: parent
        property var rootWindow: boxRef.rootWindow

        anchors.fill: parent
        z: 20

        Repeater {
            model: pathHandlePlane.boxRef.boxModel.pathPoints

            delegate: Rectangle {
                id: pathHandle

                property var pathPlane: parent
                property var boxRef: pathPlane.boxRef
                property var rootWindow: pathPlane.rootWindow
                property var editorRef: boxRef.editorRef

                objectName: "pathHandle"
                width: Math.max(1, rootWindow.documentToViewLength(10))
                height: width
                color: "#ffb000"
                border.color: "#202020"
                visible: boxRef.selected && boxRef.boxModel.path
                x: rootWindow.pointValue(modelData, 0, 0.5) * pathPlane.width - width / 2
                y: rootWindow.pointValue(modelData, 1, 0.5) * pathPlane.height - height / 2

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Math.max(8, pathHandle.width)
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    onPressed: (mouse) => {
                        mouse.accepted = true;
                        pathHandle.editorRef.selectBox(pathHandle.boxRef.boxModel.index);
                        const canvasPoint = mapToItem(canvasItem, mouse.x, mouse.y);
                        const startX = rootWindow.pointValue(modelData, 0, 0.5) * pathHandle.pathPlane.width;
                        const startY = rootWindow.pointValue(modelData, 1, 0.5) * pathHandle.pathPlane.height;
                        pathHandle.rootWindow.beginPathHandleDrag(pathHandle.pathPlane, index, startX, startY, canvasPoint.x, canvasPoint.y);
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed || !(mouse.buttons & Qt.LeftButton))
                            return ;

                        const point = mapToItem(canvasItem, mouse.x, mouse.y);
                        pathHandle.rootWindow.updatePathHandleDragFromCanvas(point.x, point.y);
                    }
                    onReleased: pathHandle.rootWindow.endPathHandleDrag()
                    onCanceled: pathHandle.rootWindow.endPathHandleDrag()
                }

            }

        }

        transform: Matrix4x4 {
            matrix: pathHandlePlane.rootWindow.perspectiveMatrix(pathHandlePlane.boxRef.boxModel, pathHandlePlane.width, pathHandlePlane.height, pathHandlePlane.rootWindow.viewDocScale(), pathHandlePlane.boxRef.perspectiveActive)
        }

    }

}
