import QtQuick

QtObject {
    id: interactionState

    property int idleMode: 0
    property int panMode: 2
    property int createMode: 3
    property real pressX: 0
    property real pressY: 0
    property real pointerX: 0
    property real pointerY: 0
    property int dragMode: idleMode
    property real createStartX: 0
    property real createStartY: 0
    property real createCurrentX: 0
    property real createCurrentY: 0

    function beginPress(x, y, button, modifiers) {
        pressX = x;
        pressY = y;
        pointerX = x;
        pointerY = y;
        if (button === Qt.LeftButton && (modifiers & Qt.ControlModifier)) {
            dragMode = createMode;
            createStartX = x;
            createStartY = y;
            createCurrentX = x;
            createCurrentY = y;
            return ;
        }
        dragMode = button === Qt.LeftButton || button === Qt.MiddleButton || button === Qt.RightButton ? panMode : idleMode;
    }

    function updatePointer(x, y, pressed) {
        pointerX = x;
        pointerY = y;
        if (!pressed)
            return {
            "panned": false,
            "panDx": 0,
            "panDy": 0,
            "creating": false
        };

        const dx = x - pressX;
        const dy = y - pressY;
        const panned = dragMode === panMode;
        const creating = dragMode === createMode;
        if (creating) {
            createCurrentX = x;
            createCurrentY = y;
        }
        pressX = x;
        pressY = y;
        return {
            "panned": panned,
            "panDx": panned ? dx : 0,
            "panDy": panned ? dy : 0,
            "creating": creating
        };
    }

    function createRectangle() {
        const left = Math.min(createStartX, createCurrentX);
        const top = Math.min(createStartY, createCurrentY);
        const right = Math.max(createStartX, createCurrentX);
        const bottom = Math.max(createStartY, createCurrentY);
        return {
            "x": left,
            "y": top,
            "width": right - left,
            "height": bottom - top
        };
    }

    function endRelease() {
        const rectangle = dragMode === createMode ? createRectangle() : {
            "x": 0,
            "y": 0,
            "width": 0,
            "height": 0
        };
        const wasCreating = dragMode === createMode;
        dragMode = idleMode;
        return {
            "creating": wasCreating,
            "rectangle": rectangle
        };
    }

    function cancel() {
        dragMode = idleMode;
    }

}
