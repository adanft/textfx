import QtQuick

QtObject {
    id: rotateState

    property var activeRotateDelegate: null
    property real rotateStartRotation: 0
    property real rotateStartAngle: 0
    property real rotateDegrees: 0

    function angleDegrees(cx, cy, x, y) {
        return Math.atan2(y - cy, x - cx) * 180 / Math.PI
    }

    function angleDeltaDegrees(from, to) {
        let delta = to - from
        while (delta > 180) delta -= 360
        while (delta < -180) delta += 360
        return delta
    }

    function begin(delegate, documentX, documentY) {
        activeRotateDelegate = delegate
        const box = delegate.boxModel
        const cx = box.x + box.w / 2
        const cy = box.y + box.h / 2
        rotateStartRotation = box.rotation
        rotateDegrees = rotateStartRotation
        rotateStartAngle = angleDegrees(cx, cy, documentX, documentY)
    }

    function update(documentX, documentY) {
        if (!activeRotateDelegate)
            return
        const box = activeRotateDelegate.boxModel
        const cx = box.x + box.w / 2
        const cy = box.y + box.h / 2
        const angle = angleDegrees(cx, cy, documentX, documentY)
        rotateDegrees = rotateStartRotation + angleDeltaDegrees(rotateStartAngle, angle)
        activeRotateDelegate.rotation = rotateDegrees
    }

    function cancelPreview() {
        if (activeRotateDelegate)
            activeRotateDelegate.rotation = rotateStartRotation
    }

    function reset() {
        activeRotateDelegate = null
    }
}
