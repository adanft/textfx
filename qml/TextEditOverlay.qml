import QtQuick
import QtQuick.Controls

TextArea {
    id: boxTextArea

    property var boxRef
    property var outlinedTextItem
    property var rootWindow: boxRef.rootWindow
    property var editorRef: boxRef.editorRef
    property real editLineSpacing: boxRef.boxModel.lineSpacing
    readonly property bool editLayoutAligned: outlinedTextItem.editLayoutMetricsValid
    readonly property real editLayoutTopPadding: editLayoutAligned ? outlinedTextItem.editLayoutTopPadding : 0
    readonly property real editLayoutLeftPadding: editLayoutAligned ? outlinedTextItem.editLayoutLeftPadding : 0
    readonly property real editLayoutRightPadding: editLayoutAligned ? outlinedTextItem.editLayoutRightPadding : 0
    readonly property real editLayoutTabStopDistance: outlinedTextItem.editLayoutTabStopDistance
    readonly property real editLayoutPaintOffsetX: editLayoutAligned ? outlinedTextItem.editLayoutPaintOffsetX : 0
    readonly property real editLayoutPaintOffsetY: editLayoutAligned ? outlinedTextItem.editLayoutPaintOffsetY : 0

    function focusForEdit() {
        if (boxRef.selected && editorRef.editingText && !activeFocus)
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

    objectName: "boxTextArea"
    z: 1
    x: editLayoutPaintOffsetX * rootWindow.viewDocScale()
    y: editLayoutPaintOffsetY * rootWindow.viewDocScale()
    width: boxRef.visualDocW
    height: boxRef.visualDocH
    transformOrigin: Item.TopLeft
    scale: rootWindow.viewDocScale()
    clip: true
    visible: boxRef.selected && editorRef.editingText
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
    horizontalAlignment: boxRef.boxModel.alignment === 1 ? TextEdit.AlignHCenter : boxRef.boxModel.alignment === 2 ? TextEdit.AlignRight : TextEdit.AlignLeft
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
        Qt.callLater(focusForEdit);
    }
    onVisibleChanged: {
        if (visible) {
            syncTextFromModel();
            applyLineSpacing();
            Qt.callLater(focusForEdit);
        }
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
        applyLineSpacing();
        if (activeFocus && editorRef && !syncingTextFromModel) {
            userInputSyncPending = true;
            editorRef.updateSelectedText(text);
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
