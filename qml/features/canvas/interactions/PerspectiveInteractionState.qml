import QtQuick

QtObject {
    id: perspectiveState

    property real documentScale: 1
    property var activePerspectiveDelegate: null
    property int activePerspectiveBoxIndex: -1
    property string activePerspectiveHandle: ""
    property real perspectiveStartCanvasX: 0
    property real perspectiveStartCanvasY: 0
    property real perspectiveStartX: 0
    property real perspectiveStartY: 0
    property real perspectiveX: 0
    property real perspectiveY: 0
    property int perspectiveRevision: 0
    property real perspectiveStartNwX: 0
    property real perspectiveStartNwY: 0
    property real perspectiveStartNeX: 0
    property real perspectiveStartNeY: 0
    property real perspectiveStartSeX: 0
    property real perspectiveStartSeY: 0
    property real perspectiveStartSwX: 0
    property real perspectiveStartSwY: 0

    function pointValue(point, index, fallback) {
        return point && point.length > index ? point[index] : fallback;
    }

    function modelPerspectiveOffset(box, name, axis) {
        if (!box)
            return 0;

        if (name === "nw")
            return pointValue(box.perspectiveNw, axis, 0);

        if (name === "ne")
            return pointValue(box.perspectiveNe, axis, 0);

        if (name === "se")
            return pointValue(box.perspectiveSe, axis, 0);

        if (name === "sw")
            return pointValue(box.perspectiveSw, axis, 0);

        if (name === "n")
            return (pointValue(box.perspectiveNw, axis, 0) + pointValue(box.perspectiveNe, axis, 0)) / 2;

        if (name === "e")
            return (pointValue(box.perspectiveNe, axis, 0) + pointValue(box.perspectiveSe, axis, 0)) / 2;

        if (name === "s")
            return (pointValue(box.perspectiveSw, axis, 0) + pointValue(box.perspectiveSe, axis, 0)) / 2;

        if (name === "w")
            return (pointValue(box.perspectiveNw, axis, 0) + pointValue(box.perspectiveSw, axis, 0)) / 2;

        return 0;
    }

    function begin(delegate, handle, canvasX, canvasY) {
        activePerspectiveDelegate = delegate;
        activePerspectiveHandle = handle;
        activePerspectiveBoxIndex = delegate && delegate.boxModel ? delegate.boxModel.index : -1;
        perspectiveStartCanvasX = canvasX;
        perspectiveStartCanvasY = canvasY;
        const box = delegate ? delegate.boxModel : null;
        perspectiveStartNwX = modelPerspectiveOffset(box, "nw", 0);
        perspectiveStartNwY = modelPerspectiveOffset(box, "nw", 1);
        perspectiveStartNeX = modelPerspectiveOffset(box, "ne", 0);
        perspectiveStartNeY = modelPerspectiveOffset(box, "ne", 1);
        perspectiveStartSeX = modelPerspectiveOffset(box, "se", 0);
        perspectiveStartSeY = modelPerspectiveOffset(box, "se", 1);
        perspectiveStartSwX = modelPerspectiveOffset(box, "sw", 0);
        perspectiveStartSwY = modelPerspectiveOffset(box, "sw", 1);
        perspectiveStartX = modelPerspectiveOffset(box, handle, 0);
        perspectiveStartY = modelPerspectiveOffset(box, handle, 1);
        perspectiveX = perspectiveStartX;
        perspectiveY = perspectiveStartY;
        ++perspectiveRevision;
    }

    function update(canvasItem, canvasX, canvasY) {
        if (!activePerspectiveDelegate)
            return ;

        const startLocal = activePerspectiveDelegate.mapFromItem(canvasItem, perspectiveStartCanvasX, perspectiveStartCanvasY);
        const local = activePerspectiveDelegate.mapFromItem(canvasItem, canvasX, canvasY);
        const scale = Math.max(0.0001, documentScale);
        const dx = (local.x - startLocal.x) / scale;
        const dy = (local.y - startLocal.y) / scale;
        perspectiveX = perspectiveStartX + (activePerspectiveHandle === "n" || activePerspectiveHandle === "s" ? 0 : dx);
        perspectiveY = perspectiveStartY + (activePerspectiveHandle === "e" || activePerspectiveHandle === "w" ? 0 : dy);
        ++perspectiveRevision;
    }

    function commitHandles() {
        const dx = perspectiveX - perspectiveStartX;
        const dy = perspectiveY - perspectiveStartY;
        if (activePerspectiveHandle === "n")
            return [{
            "name": "nw",
            "x": perspectiveStartNwX,
            "y": perspectiveStartNwY + dy
        }, {
            "name": "ne",
            "x": perspectiveStartNeX,
            "y": perspectiveStartNeY + dy
        }];

        if (activePerspectiveHandle === "e")
            return [{
            "name": "ne",
            "x": perspectiveStartNeX + dx,
            "y": perspectiveStartNeY
        }, {
            "name": "se",
            "x": perspectiveStartSeX + dx,
            "y": perspectiveStartSeY
        }];

        if (activePerspectiveHandle === "s")
            return [{
            "name": "sw",
            "x": perspectiveStartSwX,
            "y": perspectiveStartSwY + dy
        }, {
            "name": "se",
            "x": perspectiveStartSeX,
            "y": perspectiveStartSeY + dy
        }];

        if (activePerspectiveHandle === "w")
            return [{
            "name": "nw",
            "x": perspectiveStartNwX + dx,
            "y": perspectiveStartNwY
        }, {
            "name": "sw",
            "x": perspectiveStartSwX + dx,
            "y": perspectiveStartSwY
        }];

        return [{
            "name": activePerspectiveHandle,
            "x": perspectiveX,
            "y": perspectiveY
        }];
    }

    function reset() {
        activePerspectiveDelegate = null;
        activePerspectiveBoxIndex = -1;
        activePerspectiveHandle = "";
        ++perspectiveRevision;
    }

}
