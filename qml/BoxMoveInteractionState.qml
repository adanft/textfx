import QtQuick

QtObject {
    id: moveState

    property real documentScale: 1
    property var activeMoveDelegate: null
    property int activeMoveIndex: -1
    property real moveStartX: 0
    property real moveStartY: 0
    property real moveStartCanvasX: 0
    property real moveStartCanvasY: 0
    property real moveX: 0
    property real moveY: 0

    function begin(delegate, index, canvasX, canvasY) {
        activeMoveDelegate = delegate;
        activeMoveIndex = index;
        moveStartX = delegate.boxModel.x;
        moveStartY = delegate.boxModel.y;
        moveStartCanvasX = canvasX;
        moveStartCanvasY = canvasY;
        moveX = moveStartX;
        moveY = moveStartY;
    }

    function update(canvasX, canvasY) {
        if (!activeMoveDelegate)
            return ;

        const scale = Math.max(0.0001, documentScale);
        moveX = moveStartX + (canvasX - moveStartCanvasX) / scale;
        moveY = moveStartY + (canvasY - moveStartCanvasY) / scale;
    }

    function delta() {
        return {
            "x": moveX - moveStartX,
            "y": moveY - moveStartY
        };
    }

    function reset() {
        activeMoveDelegate = null;
        activeMoveIndex = -1;
    }

}
