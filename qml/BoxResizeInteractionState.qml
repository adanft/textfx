import QtQuick

QtObject {
    id: resizeState

    property real documentScale: 1
    property real minimumBoxSize: 12
    property var activeResizeDelegate: null
    property string activeResizeHandle: ""
    property real resizeStartX: 0
    property real resizeStartY: 0
    property real resizeStartW: 0
    property real resizeStartH: 0
    property real resizeStartRotation: 0
    property real resizeStartCanvasX: 0
    property real resizeStartCanvasY: 0
    property real resizeX: 0
    property real resizeY: 0
    property real resizeW: 0
    property real resizeH: 0

    function rotatePoint(x, y, radians) {
        const c = Math.cos(radians);
        const s = Math.sin(radians);
        return {
            "x": x * c - y * s,
            "y": x * s + y * c
        };
    }

    function begin(delegate, handle, canvasX, canvasY) {
        activeResizeDelegate = delegate;
        activeResizeHandle = handle;
        resizeStartX = delegate.boxModel.x;
        resizeStartY = delegate.boxModel.y;
        resizeStartW = delegate.boxModel.w;
        resizeStartH = delegate.boxModel.h;
        resizeStartRotation = delegate.boxModel.rotation * Math.PI / 180;
        resizeStartCanvasX = canvasX;
        resizeStartCanvasY = canvasY;
        resizeX = resizeStartX;
        resizeY = resizeStartY;
        resizeW = resizeStartW;
        resizeH = resizeStartH;
    }

    function update(canvasX, canvasY) {
        if (!activeResizeDelegate)
            return ;

        const scale = Math.max(0.0001, documentScale);
        const delta = rotatePoint((canvasX - resizeStartCanvasX) / scale, (canvasY - resizeStartCanvasY) / scale, -resizeStartRotation);
        const pivotX = resizeStartW / 2;
        const pivotY = resizeStartH / 2;
        let left = 0;
        let top = 0;
        let right = resizeStartW;
        let bottom = resizeStartH;
        if (activeResizeHandle.indexOf("w") >= 0)
            left = Math.min(delta.x, right - minimumBoxSize);
        else if (activeResizeHandle.indexOf("e") >= 0)
            right = Math.max(resizeStartW + delta.x, left + minimumBoxSize);
        if (activeResizeHandle.indexOf("n") >= 0)
            top = Math.min(delta.y, bottom - minimumBoxSize);
        else if (activeResizeHandle.indexOf("s") >= 0)
            bottom = Math.max(resizeStartH + delta.y, top + minimumBoxSize);
        resizeW = right - left;
        resizeH = bottom - top;
        const newPivotX = resizeW / 2;
        const newPivotY = resizeH / 2;
        const moved = rotatePoint(left - pivotX + newPivotX, top - pivotY + newPivotY, resizeStartRotation);
        resizeX = resizeStartX + pivotX + moved.x - newPivotX;
        resizeY = resizeStartY + pivotY + moved.y - newPivotY;
    }

    function bounds() {
        return {
            "x": resizeX,
            "y": resizeY,
            "width": resizeW,
            "height": resizeH
        };
    }

    function reset() {
        activeResizeDelegate = null;
        activeResizeHandle = "";
    }

}
