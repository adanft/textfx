import QtQuick
import "../../utils/ColorUtils.js" as ColorUtils

Canvas {
    id: paintLayer

    property var strokes: []
    property var previewStroke: ({})
    property bool drawPersistedStrokes: true
    property bool drawPreviewStroke: true
    readonly property bool hasPaintContent: (drawPersistedStrokes && strokes.length > 0) || (drawPreviewStroke && hasDrawableStroke(previewStroke))
    readonly property int liveStrokeCount: (drawPersistedStrokes ? strokes.length : 0) + (drawPreviewStroke && hasDrawableStroke(previewStroke) ? 1 : 0)
    property int lastPaintedStrokeCount: 0
    property int paintRevision: 0

    function hasDrawableStroke(stroke) {
        return !!stroke && !!stroke.points && stroke.points.length > 0;
    }

    function drawStroke(ctx, stroke) {
        if (!hasDrawableStroke(stroke))
            return false;

        const points = stroke.points || [];
        ctx.strokeStyle = ColorUtils.colorWithOpacity(stroke.color, stroke.opacity);
        ctx.lineWidth = Math.max(1, stroke.size);
        ctx.beginPath();
        ctx.moveTo(points[0][0], points[0][1]);
        for (let pointIndex = 1; pointIndex < points.length; ++pointIndex)
            ctx.lineTo(points[pointIndex][0], points[pointIndex][1]);
        if (points.length === 1)
            ctx.lineTo(points[0][0] + 0.1, points[0][1]);
        ctx.stroke();
        return true;
    }


    function schedulePaint() {
        ++paintRevision;
        requestPaint();
    }

    onStrokesChanged: {
        if (drawPersistedStrokes)
            schedulePaint();
    }
    onPreviewStrokeChanged: {
        if (drawPreviewStroke)
            schedulePaint();
    }
    onDrawPersistedStrokesChanged: schedulePaint()
    onDrawPreviewStrokeChanged: schedulePaint()
    onWidthChanged: schedulePaint()
    onHeightChanged: schedulePaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        let paintedCount = 0;
        if (drawPersistedStrokes) {
            for (let i = 0; i < strokes.length; ++i) {
                const stroke = strokes[i];
                if (drawStroke(ctx, stroke))
                    ++paintedCount;
            }
        }
        if (drawPreviewStroke && drawStroke(ctx, previewStroke))
            ++paintedCount;
        lastPaintedStrokeCount = paintedCount;
        ++paintRevision;
    }
}
