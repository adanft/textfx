import QtQuick
import QtQuick.Controls

TextArea {
    id: boxTextArea

    property var boxRef
    property var outlinedTextItem
    property bool selectionUiVisible: true
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef
    property real editLineSpacing: boxRef.boxModel.lineSpacing
    property bool editLayoutAligned: false
    property real editLayoutTopPadding: 0
    property real editLayoutLeftPadding: 0
    property real editLayoutRightPadding: 0
    property real editLayoutTabStopDistance: 0
    property real editLayoutPaintOffsetX: 0
    property real editLayoutPaintOffsetY: 0
    property int editLayoutRefreshGeneration: 0
    readonly property int zEditOverlay: 1

    function editHorizontalAlignment(alignment) {
        return alignment === boxRef.textAlignCenter ? TextEdit.AlignHCenter : alignment === boxRef.textAlignRight ? TextEdit.AlignRight : TextEdit.AlignLeft;
    }

    function refreshEditLayoutMetrics() {
        ++editLayoutRefreshGeneration;
        const renderer = outlinedTextItem;
        const aligned = renderer ? renderer.editLayoutMetricsValid : false;
        editLayoutAligned = aligned;
        editLayoutTopPadding = aligned ? renderer.editLayoutTopPadding : 0;
        editLayoutLeftPadding = aligned ? renderer.editLayoutLeftPadding : 0;
        editLayoutRightPadding = aligned ? renderer.editLayoutRightPadding : 0;
        editLayoutTabStopDistance = renderer ? renderer.editLayoutTabStopDistance : 0;
        editLayoutPaintOffsetX = aligned ? renderer.editLayoutPaintOffsetX : 0;
        editLayoutPaintOffsetY = aligned ? renderer.editLayoutPaintOffsetY : 0;
    }

    function scheduleEditLayoutMetricsRefresh() {
        const renderer = outlinedTextItem;
        const generation = ++editLayoutRefreshGeneration;
        Qt.callLater(function() {
            if (generation === editLayoutRefreshGeneration && renderer === outlinedTextItem)
                refreshEditLayoutMetrics();
        });
    }

    function focusForEdit() {
        if (outlinedTextItem && boxRef.selected && editorRef.editingText && !activeFocus)
            forceActiveFocus();

    }

    function forceEditFocus() {
        forceActiveFocus();
    }

    function applyLineSpacing() {
        editorRef.applyTextLineSpacing(textDocument, editLineSpacing);
    }

    property bool syncingTextFromModel: false
    property bool userInputSyncPending: false
    property string livePreviewText: boxRef.modelPreviewText()

    function modelText() {
        return boxRef.modelPreviewText();
    }

    function setLivePreviewText(nextText) {
        livePreviewText = nextText;
    }

    function syncTextFromModel() {
        const nextText = modelText();
        if (activeFocus && userInputSyncPending) {
            setLivePreviewText(text);
            userInputSyncPending = false;
            applyLineSpacing();
            return ;
        }

        userInputSyncPending = false;

        if (text === nextText) {
            setLivePreviewText(text);
            return ;
        }

        const oldCursor = cursorPosition;
        const oldSelectionStart = selectionStart;
        const oldSelectionEnd = selectionEnd;
        syncingTextFromModel = true;
        text = nextText;
        setLivePreviewText(nextText);
        cursorPosition = Math.min(oldCursor, text.length);
        select(Math.min(oldSelectionStart, text.length), Math.min(oldSelectionEnd, text.length));
        syncingTextFromModel = false;
        applyLineSpacing();
    }

    objectName: selectionUiVisible ? "boxTextArea" : "boxTextAreaInactive"
    z: zEditOverlay
    x: editLayoutPaintOffsetX * rootWindow.viewDocScale()
    y: editLayoutPaintOffsetY * rootWindow.viewDocScale()
    width: boxRef.visualDocW
    height: boxRef.visualDocH
    transformOrigin: Item.TopLeft
    scale: rootWindow.viewDocScale()
    clip: true
    visible: selectionUiVisible && boxRef.selected && editorRef.editingText
    text: ""
    color: "transparent"
    selectedTextColor: "transparent"
    selectionColor: Qt.alpha(rootWindow.palette.highlight, 0.35)
    placeholderTextColor: "transparent"
    font.family: boxRef.boxModel.resolvedFontFamily
    font.pixelSize: Math.round(Math.max(1, boxRef.boxModel.fontSize))
    font.bold: boxRef.boxModel.bold
    font.weight: boxRef.boxModel.bold ? Font.Bold : Font.Normal
    font.italic: boxRef.boxModel.italic
    font.letterSpacing: boxRef.boxModel.letterSpacing
    horizontalAlignment: editHorizontalAlignment(boxRef.boxModel.alignment)
    padding: 0
    topPadding: editLayoutTopPadding
    leftPadding: editLayoutLeftPadding
    rightPadding: editLayoutRightPadding
    bottomPadding: 0
    background: null
    wrapMode: TextEdit.WordWrap
    tabStopDistance: editLayoutTabStopDistance
    selectByMouse: boxRef.selected && editorRef.editingText
    readOnly: !(boxRef.selected && editorRef.editingText)
    Component.onCompleted: {
        syncTextFromModel();
        applyLineSpacing();
        scheduleEditLayoutMetricsRefresh();
        Qt.callLater(focusForEdit);
    }
    onVisibleChanged: {
        if (visible) {
            syncTextFromModel();
            applyLineSpacing();
            scheduleEditLayoutMetricsRefresh();
            Qt.callLater(focusForEdit);
        }
    }
    onOutlinedTextItemChanged: {
        editLayoutAligned = false;
        editLayoutTopPadding = editLayoutLeftPadding = editLayoutRightPadding = 0;
        editLayoutTabStopDistance = editLayoutPaintOffsetX = editLayoutPaintOffsetY = 0;
        scheduleEditLayoutMetricsRefresh();
        if (outlinedTextItem && visible)
            Qt.callLater(focusForEdit);
    }
    onEditLineSpacingChanged: applyLineSpacing()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) {
            rootWindow.handleEscape();
            event.accepted = true;
        }
    }
    onActiveFocusChanged: {
        if (editorRef) {
            activeFocus ? editorRef.beginTextEdit() : editorRef.endTextEdit();
        }
    }
    onTextChanged: {
        setLivePreviewText(text);
        if (activeFocus && editorRef && !syncingTextFromModel) {
            userInputSyncPending = true;
            editorRef.updateSelectedText(text);
            refreshEditLayoutMetrics();
        }

    }

    Connections {
        target: boxTextArea.outlinedTextItem
        function onEditLayoutMetricsChanged() {
            boxTextArea.scheduleEditLayoutMetricsRefresh();
        }
    }

    Connections {
        function onSelectedBoxChanged() {
            if (boxTextArea.visible)
                boxTextArea.syncTextFromModel();
        }

        target: boxTextArea.editorRef
    }

    cursorDelegate: Rectangle {
        width: Math.max(1, 2 / boxTextArea.rootWindow.viewDocScale())
        color: boxTextArea.rootWindow.palette.highlight

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: boxTextArea.visible && boxTextArea.activeFocus

            NumberAnimation {
                to: 0
                duration: 450
            }

            NumberAnimation {
                to: 1
                duration: 450
            }

        }

    }

}
