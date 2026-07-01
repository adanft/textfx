import QtQuick
import QtQuick.Controls
import TextFX.Ui 1.0

Rectangle {
    id: boxDelegate

    required property var modelData
    required property var canvasItem
    required property var interaction
    readonly property var rootWindow: ApplicationWindow.window
    readonly property var editorRef: rootWindow ? rootWindow.editor : null
    property bool selected: editorRef && modelData.index === editorRef.selectedIndex
    property bool editingSelected: selected && editorRef.editingText
    property var boxModel: modelData
    property bool perspectiveActive: boxModel.perspective && !editingSelected
    property bool moveActive: rootWindow.dragMode === interaction.dragModeMove && rootWindow.activeMoveIndex === modelData.index
    property bool resizeActive: rootWindow.dragMode === interaction.dragModeResize && rootWindow.activeResizeDelegate === boxDelegate
    property real visualDocX: moveActive ? rootWindow.moveX : resizeActive ? rootWindow.resizeX : modelData.x
    property real visualDocY: moveActive ? rootWindow.moveY : resizeActive ? rootWindow.resizeY : modelData.y
    property real visualDocW: resizeActive ? rootWindow.resizeW : modelData.w
    property real visualDocH: resizeActive ? rootWindow.resizeH : modelData.h
    readonly property bool textOverflow: boxOutlinedText.overflow

    objectName: "textBoxDelegate"
    x: rootWindow.documentToViewX(visualDocX)
    y: rootWindow.documentToViewY(visualDocY)
    width: visualDocW * rootWindow.viewDocScale()
    height: visualDocH * rootWindow.viewDocScale()
    color: "transparent"
    border.width: perspectiveActive ? 0 : selected ? rootWindow.selectionLineWidth() : Math.max(1, rootWindow.documentToViewLength(1))
    border.color: textOverflow ? Qt.rgba(1, 0, 0, 1) : editingSelected ? Qt.rgba(1, 0.84, 0, 1) : selected ? rootWindow.palette.highlight : rootWindow.palette.mid
    rotation: modelData.rotation

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
            text: boxRef.selected && editorRef.editingText ? boxTextArea.text : (modelData.uppercase ? String(modelData.text).toUpperCase() : modelData.text)
            color: rootWindow.qmlColor(modelData.color)
            fontFamily: modelData.fontFamily
            pixelSize: Math.max(1, modelData.fontSize)
            bold: modelData.bold
            italic: modelData.italic
            letterSpacing: modelData.letterSpacing
            lineSpacing: modelData.lineSpacing
            horizontalAlignment: modelData.alignment === 1 ? Text.AlignHCenter : modelData.alignment === 2 ? Text.AlignRight : Text.AlignLeft
            outlineColor: rootWindow.qmlColor(modelData.outlineColor)
            outlineSize: modelData.outline && modelData.outlineSize > 0 ? modelData.outlineSize : 0
            blurSize: modelData.blur && modelData.blurSize > 0 ? modelData.blurSize : 0
            shadowEnabled: modelData.shadow
            shadowColor: rootWindow.qmlColor(modelData.shadowColor)
            shadowOffsetX: modelData.shadowOffsetX
            shadowOffsetY: modelData.shadowOffsetY
            shadowBlurSize: modelData.shadow && modelData.shadowBlurSize > 0 ? modelData.shadowBlurSize : 0
            gradientEnabled: modelData.gradient
            gradientDirection: modelData.gradientDirection
            gradientColorA: rootWindow.qmlColor(modelData.gradientColorA)
            gradientColorB: rootWindow.qmlColor(modelData.gradientColorB)
            pathEnabled: modelData.path && !boxRef.editingSelected
            pathMode: modelData.pathMode
            pathPoints: modelData.pathPoints
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
        property real editLineSpacing: modelData.lineSpacing
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
        text: modelData.uppercase ? String(modelData.text).toUpperCase() : modelData.text
        color: "transparent"
        selectedTextColor: "transparent"
        selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)
        placeholderTextColor: "transparent"
        font.family: modelData.resolvedFontFamily
        font.pixelSize: Math.round(Math.max(1, modelData.fontSize))
        font.bold: modelData.bold
        font.weight: modelData.bold ? Font.Bold : Font.Normal
        font.italic: modelData.italic
        font.letterSpacing: modelData.letterSpacing
        horizontalAlignment: modelData.alignment === 1 ? TextEdit.AlignHCenter : modelData.alignment === 2 ? TextEdit.AlignRight : TextEdit.AlignLeft
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
            applyLineSpacing();
            Qt.callLater(focusForEdit);
        }
        onVisibleChanged: {
            if (visible) {
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
            applyLineSpacing();
            if (activeFocus && editorRef)
                editorRef.updateSelectedText(text);

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
