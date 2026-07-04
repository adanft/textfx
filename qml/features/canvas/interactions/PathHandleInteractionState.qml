import QtQuick

QtObject {
    id: pathHandleState

    property int activePathHandleIndex: -1
    property var activePathHandlePlane: null
    property bool activePathHandlePerspective: false
    property bool pathHandleInteractionActive: false
    property bool pathHandleDragPressed: false
    property real activePathBoxWidth: 1
    property real activePathBoxHeight: 1
    property real activePathBoxRotation: 0
    property real pathHandlePressCanvasX: 0
    property real pathHandlePressCanvasY: 0
    property real pathHandlePressLocalX: 0
    property real pathHandlePressLocalY: 0
    property real pathHandleStartX: 0
    property real pathHandleStartY: 0

    function begin(pathPlane, index, startX, startY, canvasItem, pressCanvasX, pressCanvasY) {
        activePathHandlePlane = pathPlane;
        activePathBoxWidth = pathPlane ? pathPlane.width : 1;
        activePathBoxHeight = pathPlane ? pathPlane.height : 1;
        activePathHandlePerspective = !!(pathPlane && pathPlane.boxRef && pathPlane.boxRef.perspectiveActive);
        activePathBoxRotation = pathPlane && pathPlane.boxRef ? pathPlane.boxRef.rotation : 0;
        activePathHandleIndex = index;
        pathHandlePressCanvasX = pressCanvasX;
        pathHandlePressCanvasY = pressCanvasY;
        const pressLocal = pathPlane ? pathPlane.mapFromItem(canvasItem, pressCanvasX, pressCanvasY) : Qt.point(pressCanvasX, pressCanvasY);
        pathHandlePressLocalX = pressLocal.x;
        pathHandlePressLocalY = pressLocal.y;
        pathHandleStartX = startX;
        pathHandleStartY = startY;
        pathHandleDragPressed = true;
        pathHandleInteractionActive = true;
    }

    function update(canvasItem, canvasX, canvasY, leftMouseButtonDown) {
        if (!pathHandleInteractionActive || !pathHandleDragPressed || activePathHandleIndex < 0)
            return {
            "changed": false,
            "ended": false
        };

        if (!leftMouseButtonDown)
            return {
            "changed": false,
            "ended": true
        };

        let localDx = 0;
        let localDy = 0;
        if (activePathHandlePerspective) {
            const local = activePathHandlePlane ? activePathHandlePlane.mapFromItem(canvasItem, canvasX, canvasY) : Qt.point(canvasX, canvasY);
            localDx = local.x - pathHandlePressLocalX;
            localDy = local.y - pathHandlePressLocalY;
        } else {
            const radians = -activePathBoxRotation * Math.PI / 180;
            const dx = canvasX - pathHandlePressCanvasX;
            const dy = canvasY - pathHandlePressCanvasY;
            localDx = dx * Math.cos(radians) - dy * Math.sin(radians);
            localDy = dx * Math.sin(radians) + dy * Math.cos(radians);
        }
        return {
            "changed": true,
            "ended": false,
            "index": activePathHandleIndex,
            "x": (pathHandleStartX + localDx) / activePathBoxWidth,
            "y": (pathHandleStartY + localDy) / activePathBoxHeight
        };
    }

    function reset() {
        const wasActive = pathHandleInteractionActive;
        pathHandleDragPressed = false;
        pathHandleInteractionActive = false;
        activePathHandlePlane = null;
        activePathHandlePerspective = false;
        activePathHandleIndex = -1;
        return wasActive;
    }

}
