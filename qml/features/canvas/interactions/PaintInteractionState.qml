import QtQuick

QtObject {
    id: paintInteractionState

    property var editor: null
    property var paintPanel: null
    property int dragMode: 0
    property int dragModeIdle: 0
    property int dragModePaint: 0
    property var documentPoint: function(x, y) { return [x, y]; }
    property var setDragMode: function(mode) {}
    property var activePaintPoints: []
    property var activePaintPreviewPoints: []
    property string activePaintTarget: ""
    property int activePaintPageIndex: -1
    property int paintPreviewPublishRevision: 0
    readonly property int paintPreviewPublishIntervalMs: 16

    function paintPoint(x, y) {
        return documentPoint(x, y);
    }

    function strokeFor(points) {
        return {
            "target": activePaintTarget,
            "color": paintPanel.paintBrushColor,
            "size": paintPanel.paintBrushSize,
            "opacity": paintPanel.paintBrushOpacity,
            "points": points
        };
    }

    function canShowPreviewFor(target) {
        return activePaintTarget === target && editor && editor.currentPageIndex === activePaintPageIndex && paintPanel && !paintPanel.paintEraserMode && activePaintPreviewPoints.length > 0;
    }

    function activePaintStroke(target) {
        if (!canShowPreviewFor(target))
            return {};

        const stroke = strokeFor(activePaintPreviewPoints);
        return {
            "color": stroke.color,
            "size": stroke.size,
            "opacity": stroke.opacity,
            "points": stroke.points
        };
    }

    function publishActivePaintPreview() {
        activePaintPreviewPoints = activePaintPoints.slice();
        ++paintPreviewPublishRevision;
    }

    function publishPaintPreview() {
        paintPreviewPublishTimer.stop();
        if (dragMode !== dragModePaint || !paintPanel || paintPanel.paintEraserMode || activePaintPoints.length === 0)
            return ;
        publishActivePaintPreview();
    }

    function clearPaintDrag() {
        paintPreviewPublishTimer.stop();
        activePaintPoints = [];
        activePaintPreviewPoints = [];
        activePaintTarget = "";
        activePaintPageIndex = -1;
        setDragMode(dragModeIdle);
    }

    function paintDragPageMatchesOrigin() {
        return editor && activePaintPageIndex >= 0 && editor.currentPageIndex === activePaintPageIndex;
    }

    function schedulePaintPreviewPublish() {
        if (!paintPreviewPublishTimer.running)
            paintPreviewPublishTimer.start();
    }

    function beginPaintDrag(x, y) {
        if (!paintPanel || !paintPanel.paintMode || !editor)
            return false;
        editor.endTextEdit();
        setDragMode(dragModePaint);
        activePaintTarget = paintPanel.paintTarget;
        activePaintPageIndex = editor.currentPageIndex;
        const point = paintPoint(x, y);
        if (paintPanel.paintEraserMode) {
            editor.erasePaintAt(activePaintTarget, point[0], point[1], paintPanel.paintEraserSize);
        } else {
            activePaintPoints = [point];
            publishActivePaintPreview();
        }
        return true;
    }

    function updatePaintDrag(x, y) {
        if (dragMode !== dragModePaint)
            return ;
        if (!paintDragPageMatchesOrigin()) {
            cancelPaintDrag();
            return ;
        }
        const point = paintPoint(x, y);
        if (paintPanel.paintEraserMode) {
            editor.erasePaintAt(activePaintTarget, point[0], point[1], paintPanel.paintEraserSize);
            return ;
        }
        activePaintPoints.push(point);
        schedulePaintPreviewPublish();
    }

    function commitPaintStroke(stroke) {
        editor.addPaintStroke(stroke.target, stroke.color, stroke.size, stroke.opacity, stroke.points);
    }

    function endPaintDrag() {
        if (dragMode !== dragModePaint)
            return false;
        const pageMatchesOrigin = paintDragPageMatchesOrigin();
        if (pageMatchesOrigin)
            publishPaintPreview();
        if (!paintPanel.paintEraserMode && activePaintPoints.length > 0 && pageMatchesOrigin)
            commitPaintStroke(strokeFor(activePaintPoints));
        clearPaintDrag();
        return true;
    }

    function cancelPaintDrag() {
        if (dragMode !== dragModePaint)
            return false;
        clearPaintDrag();
        return true;
    }

    property Timer paintPreviewPublishTimer: Timer {
        interval: paintInteractionState.paintPreviewPublishIntervalMs
        repeat: false
        onTriggered: paintInteractionState.publishPaintPreview()
    }
}
