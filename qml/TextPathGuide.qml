import QtQuick

Canvas {
    id: pathGuide

    property var boxRef
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef
    property int pathMode: boxRef.boxModel.pathMode
    property var pathPoints: boxRef.boxModel.pathPoints
    readonly property int pathModeStraight: 0
    readonly property int pathModeSmooth: 1
    readonly property int zPathGuide: 18

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
    z: zPathGuide
    onPaint: {
        const ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        const points = pathPoints;
        if (!points || points.length < 2)
            return ;

        const guidePoints = pathMode === pathModeSmooth ? smoothGuidePoints(points) : points.map((point) => {
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
