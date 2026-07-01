import QtQuick

QtObject {
    id: geometry

    property real documentScale: 1
    property real handleSize: 12
    property real rotateHandleDistance: 28
    property bool livePerspectiveActive: false
    property int activePerspectiveBoxIndex: -1
    property string activePerspectiveHandle: ""
    property real perspectiveStartX: 0
    property real perspectiveStartY: 0
    property real perspectiveX: 0
    property real perspectiveY: 0
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

    function perspectiveBase(name, axis) {
        if (axis === 0)
            return name === "ne" || name === "e" || name === "se" ? 1 : name === "n" || name === "s" ? 0.5 : 0;

        return name === "se" || name === "s" || name === "sw" ? 1 : name === "e" || name === "w" ? 0.5 : 0;
    }

    function usesLivePerspectiveOffset(box, name, axis) {
        if (!livePerspectiveActive || !box)
            return false;

        if (box.index !== activePerspectiveBoxIndex)
            return false;

        const handle = activePerspectiveHandle;
        if (name === handle && (handle === "nw" || handle === "ne" || handle === "se" || handle === "sw"))
            return true;

        if (handle === "n" && axis === 1)
            return name === "nw" || name === "ne" || name === "n";

        if (handle === "e" && axis === 0)
            return name === "ne" || name === "se" || name === "e";

        if (handle === "s" && axis === 1)
            return name === "sw" || name === "se" || name === "s";

        if (handle === "w" && axis === 0)
            return name === "nw" || name === "sw" || name === "w";

        return false;
    }

    function modelPerspectiveOffset(box, name, axis) {
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

    function perspectiveOffset(box, name, axis) {
        if (!usesLivePerspectiveOffset(box, name, axis))
            return modelPerspectiveOffset(box, name, axis);

        const dx = perspectiveX - perspectiveStartX;
        const dy = perspectiveY - perspectiveStartY;
        if (activePerspectiveHandle === "n")
            return name === "nw" ? perspectiveStartNwY + dy : name === "ne" ? perspectiveStartNeY + dy : (perspectiveStartNwY + perspectiveStartNeY) / 2 + dy;

        if (activePerspectiveHandle === "e")
            return name === "ne" ? perspectiveStartNeX + dx : name === "se" ? perspectiveStartSeX + dx : (perspectiveStartNeX + perspectiveStartSeX) / 2 + dx;

        if (activePerspectiveHandle === "s")
            return name === "sw" ? perspectiveStartSwY + dy : name === "se" ? perspectiveStartSeY + dy : (perspectiveStartSwY + perspectiveStartSeY) / 2 + dy;

        if (activePerspectiveHandle === "w")
            return name === "nw" ? perspectiveStartNwX + dx : name === "sw" ? perspectiveStartSwX + dx : (perspectiveStartNwX + perspectiveStartSwX) / 2 + dx;

        if (name === activePerspectiveHandle)
            return axis === 0 ? perspectiveX : perspectiveY;

        return modelPerspectiveOffset(box, name, axis);
    }

    function visualHandlePosition(box, name, width, height) {
        if (!box || !box.perspective)
            return {
            "x": perspectiveBase(name, 0) * width,
            "y": perspectiveBase(name, 1) * height
        };

        if (name === "n" || name === "e" || name === "s" || name === "w") {
            const a = name === "n" || name === "w" ? perspectiveCorner(box, "nw", width, height) : perspectiveCorner(box, "se", width, height);
            const b = name === "n" || name === "e" ? perspectiveCorner(box, "ne", width, height) : perspectiveCorner(box, "sw", width, height);
            return {
                "x": (a.x + b.x) / 2,
                "y": (a.y + b.y) / 2
            };
        }
        return perspectiveCorner(box, name, width, height);
    }

    function topMiddleVisualPoint(box, width, height) {
        if (!box || !box.perspective)
            return {
            "x": width / 2,
            "y": 0
        };

        const nw = perspectiveCorner(box, "nw", width, height);
        const ne = perspectiveCorner(box, "ne", width, height);
        return {
            "x": (nw.x + ne.x) / 2,
            "y": (nw.y + ne.y) / 2
        };
    }

    function rotateHandlePosition(box, width, height) {
        const top = topMiddleVisualPoint(box, width, height);
        if (!box || !box.perspective)
            return {
            "x": top.x,
            "y": top.y - rotateHandleDistance
        };

        const nw = perspectiveCorner(box, "nw", width, height);
        const ne = perspectiveCorner(box, "ne", width, height);
        const edgeX = ne.x - nw.x;
        const edgeY = ne.y - nw.y;
        const edgeLength = Math.max(1, Math.sqrt(edgeX * edgeX + edgeY * edgeY));
        return {
            "x": top.x + edgeY / edgeLength * rotateHandleDistance,
            "y": top.y - edgeX / edgeLength * rotateHandleDistance
        };
    }

    function perspectiveLayoutCorner(box, name, width, height) {
        return {
            "x": perspectiveBase(name, 0) * width + perspectiveOffset(box, name, 0),
            "y": perspectiveBase(name, 1) * height + perspectiveOffset(box, name, 1)
        };
    }

    function perspectiveCorner(box, name, width, height) {
        const scale = Math.max(0.0001, documentScale);
        const corner = perspectiveLayoutCorner(box, name, width / scale, height / scale);
        return {
            "x": corner.x * scale,
            "y": corner.y * scale
        };
    }

    function perspectiveVisualBounds(box, width, height) {
        const nw = perspectiveCorner(box, "nw", width, height);
        const ne = perspectiveCorner(box, "ne", width, height);
        const se = perspectiveCorner(box, "se", width, height);
        const sw = perspectiveCorner(box, "sw", width, height);
        const left = Math.min(nw.x, ne.x, se.x, sw.x) - 1;
        const top = Math.min(nw.y, ne.y, se.y, sw.y) - 1;
        const right = Math.max(nw.x, ne.x, se.x, sw.x) + 1;
        const bottom = Math.max(nw.y, ne.y, se.y, sw.y) + 1;
        return {
            "x": left,
            "y": top,
            "width": right - left,
            "height": bottom - top
        };
    }

    function pointOnSegment(point, a, b) {
        const cross = (point.y - a.y) * (b.x - a.x) - (point.x - a.x) * (b.y - a.y);
        if (Math.abs(cross) > 0.001)
            return false;

        const dot = (point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y);
        if (dot < 0)
            return false;

        const lengthSquared = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        return dot <= lengthSquared;
    }

    function pointInPerspectivePolygon(point, box, width, height) {
        const polygon = [perspectiveCorner(box, "nw", width, height), perspectiveCorner(box, "ne", width, height), perspectiveCorner(box, "se", width, height), perspectiveCorner(box, "sw", width, height)];
        let inside = false;
        for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
            const a = polygon[i];
            const b = polygon[j];
            if (pointOnSegment(point, a, b))
                return true;

            if (((a.y > point.y) !== (b.y > point.y)) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                inside = !inside;

        }
        return inside;
    }

    function perspectiveMargin(box) {
        if (!box || !box.perspective)
            return 0;

        return Math.max(Math.abs(pointValue(box.perspectiveNw, 0, 0)), Math.abs(pointValue(box.perspectiveNw, 1, 0)), Math.abs(pointValue(box.perspectiveNe, 0, 0)), Math.abs(pointValue(box.perspectiveNe, 1, 0)), Math.abs(pointValue(box.perspectiveSe, 0, 0)), Math.abs(pointValue(box.perspectiveSe, 1, 0)), Math.abs(pointValue(box.perspectiveSw, 0, 0)), Math.abs(pointValue(box.perspectiveSw, 1, 0))) * Math.max(0.0001, documentScale) + handleSize;
    }

    function perspectiveMatrix(box, width, height, renderScale, enabled) {
        if (!enabled || width <= 0 || height <= 0)
            return Qt.matrix4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

        const scale = renderScale > 0 ? renderScale : 1;
        const layoutWidth = width / scale;
        const layoutHeight = height / scale;
        const p0 = perspectiveLayoutCorner(box, "nw", layoutWidth, layoutHeight);
        const p1 = perspectiveLayoutCorner(box, "ne", layoutWidth, layoutHeight);
        const p2 = perspectiveLayoutCorner(box, "se", layoutWidth, layoutHeight);
        const p3 = perspectiveLayoutCorner(box, "sw", layoutWidth, layoutHeight);
        const dx1 = p1.x - p2.x, dy1 = p1.y - p2.y;
        const dx2 = p3.x - p2.x, dy2 = p3.y - p2.y;
        const dx3 = p0.x - p1.x + p2.x - p3.x;
        const dy3 = p0.y - p1.y + p2.y - p3.y;
        let g = 0, h = 0;
        const det = dx1 * dy2 - dx2 * dy1;
        if (Math.abs(det) > 1e-06 && (Math.abs(dx3) > 1e-06 || Math.abs(dy3) > 1e-06)) {
            g = (dx3 * dy2 - dx2 * dy3) / det;
            h = (dx1 * dy3 - dx3 * dy1) / det;
        }
        const a = (p1.x - p0.x + g * p1.x) / layoutWidth;
        const b = (p3.x - p0.x + h * p3.x) / layoutHeight;
        const d = (p1.y - p0.y + g * p1.y) / layoutWidth;
        const e = (p3.y - p0.y + h * p3.y) / layoutHeight;
        return Qt.matrix4x4(a, b, 0, p0.x * scale, d, e, 0, p0.y * scale, 0, 0, 1, 0, g / width, h / height, 0, 1);
    }

}
