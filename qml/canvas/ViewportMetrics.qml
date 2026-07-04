import QtQuick

QtObject {
    id: viewportMetrics

    property real zoom: 1
    property real pageBaseScale: 1
    property real panX: 0
    property real panY: 0
    property real canvasWidth: 0
    property real canvasHeight: 0
    property real pageSourceWidth: 0
    property real pageSourceHeight: 0
    readonly property real minimumZoom: 0.5
    readonly property real maximumZoom: 6

    function fitPageScale() {
        if (pageSourceWidth <= 0 || pageSourceHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0)
            return 1;

        return Math.min(canvasWidth / pageSourceWidth, canvasHeight / pageSourceHeight);
    }

    function pageScale() {
        return pageBaseScale;
    }

    function pageDisplayWidth() {
        return pageSourceWidth * pageScale();
    }

    function pageDisplayHeight() {
        return pageSourceHeight * pageScale();
    }

    function pageLeft() {
        return (canvasWidth - pageDisplayWidth()) / 2;
    }

    function pageTop() {
        return (canvasHeight - pageDisplayHeight()) / 2;
    }

    function viewDocScale() {
        return pageScale() * zoom;
    }

    function livePreviewScale() {
        return Math.min(1, viewDocScale());
    }

    function documentToViewLength(value) {
        return value * viewDocScale();
    }

    function documentToViewX(x) {
        return panX + (pageLeft() + x * pageScale()) * zoom;
    }

    function documentToViewY(y) {
        return panY + (pageTop() + y * pageScale()) * zoom;
    }

    function viewToDocumentX(x) {
        return (x - panX) / viewDocScale() - pageLeft() / pageScale();
    }

    function viewToDocumentY(y) {
        return (y - panY) / viewDocScale() - pageTop() / pageScale();
    }

    function zoomStateAt(x, y, factor) {
        const oldZoom = zoom;
        const nextZoom = Math.max(minimumZoom, Math.min(maximumZoom, oldZoom * factor));
        if (nextZoom === oldZoom)
            return {
            "zoom": oldZoom,
            "panX": panX,
            "panY": panY,
            "changed": false
        };

        return {
            "zoom": nextZoom,
            "panX": x - (x - panX) * (nextZoom / oldZoom),
            "panY": y - (y - panY) * (nextZoom / oldZoom),
            "changed": true
        };
    }

    function resetState() {
        return {
            "zoom": 1,
            "panX": 0,
            "panY": 0
        };
    }

}
