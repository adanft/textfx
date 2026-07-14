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
    readonly property real fitScale: pageSourceWidth <= 0 || pageSourceHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 ? 1 : Math.min(canvasWidth / pageSourceWidth, canvasHeight / pageSourceHeight)
    readonly property real documentScale: pageBaseScale * zoom
    readonly property real pageWidth: pageSourceWidth * pageBaseScale
    readonly property real pageHeight: pageSourceHeight * pageBaseScale
    readonly property real pageLeftOffset: (canvasWidth - pageWidth) / 2
    readonly property real pageTopOffset: (canvasHeight - pageHeight) / 2
    readonly property real documentViewOriginX: panX + pageLeftOffset * zoom
    readonly property real documentViewOriginY: panY + pageTopOffset * zoom

    function fitPageScale() {
        return fitScale;
    }

    function pageScale() {
        return pageBaseScale;
    }

    function pageDisplayWidth() {
        return pageWidth;
    }

    function pageDisplayHeight() {
        return pageHeight;
    }

    function pageLeft() {
        return pageLeftOffset;
    }

    function pageTop() {
        return pageTopOffset;
    }

    function viewDocScale() {
        return documentScale;
    }

    function livePreviewScale() {
        return Math.min(1, viewDocScale());
    }

    function documentToViewLength(value) {
        return value * viewDocScale();
    }

    function documentToViewX(x) {
        return documentViewOriginX + x * documentScale;
    }

    function documentToViewY(y) {
        return documentViewOriginY + y * documentScale;
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
