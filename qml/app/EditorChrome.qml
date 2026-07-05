import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "../features/menus"
import "../commands"

Item {
    id: editorChrome

    property var editor: null
    property var hostPalette
    property real hostWidth: 0
    property real hostHeight: 0
    property real zoomCenterX: 0
    property real zoomCenterY: 0
    property bool pageNavigationEnabled: true
    property bool sidePanelTextInputFocused: false
    property alias menuBar: chromeMenuBar
    property string colorDialogSetter: ""
    property var paintBrushColorTarget: null
    readonly property bool editingText: editor ? editor.editingText : false
    readonly property bool hasSelectedBox: editor ? editor.selectedIndex >= 0 : false
    readonly property bool boxActionsAvailable: hasSelectedBox && !editingText
    readonly property bool projectAvailable: editor ? editor.hasProject : false

    signal zoomAtRequested(real x, real y, real factor)
    signal resetZoomRequested()
    signal escapeRequested()

    function toast(message) {
        toastLabel.text = message;
        toastPopup.open();
        toastTimer.restart();
    }

    function openColorDialog(hex, setter) {
        colorDialog.selectedColor = hex;
        colorDialogSetter = setter;
        colorDialog.open();
    }

    function openProjectPicker() {
        openProjectDialog.open();
    }

    function openNewProjectPicker() {
        newProjectDialog.open();
    }

    function qmlColor(hex) {
        const raw = String(hex || "000000ff");
        const value = raw.startsWith("#") ? raw.slice(1) : raw;
        return "#" + value.slice(0, 6);
    }

    function dialogColorHex(colorValue) {
        const value = String(colorValue);
        if (/^#[0-9a-fA-F]{8}$/.test(value))
            return "#" + value.slice(3);

        return qmlColor(value);
    }

    function applyColorDialogSelection(hex) {
        if (!editor)
            return ;

        if (colorDialogSetter === "text")
            editor.setSelectedTextColor(hex);
        else if (colorDialogSetter === "outline")
            editor.setSelectedOutlineColor(hex);
        else if (colorDialogSetter.startsWith("outlineLayer:"))
            editor.setSelectedOutlineLayerColor(Number(colorDialogSetter.split(":")[1]), hex);
        else if (colorDialogSetter === "shadow")
            editor.setSelectedShadowColor(hex);
        else if (colorDialogSetter === "gradientA")
            editor.setSelectedGradientColorA(hex);
        else if (colorDialogSetter === "gradientB")
            editor.setSelectedGradientColorB(hex);
        else if (colorDialogSetter === "paint" && paintBrushColorTarget)
            paintBrushColorTarget.paintBrushColor = hex;
    }

    width: 0
    height: 0

    Connections {
        function onNotificationChanged() {
            if (editorChrome.editor.notification.length > 0)
                editorChrome.toast(editorChrome.editor.notification);

        }

        target: editorChrome.editor
    }

    EditorCommands {
        id: editorCommands

        editor: editorChrome.editor
        pageNavigationEnabled: editorChrome.pageNavigationEnabled
        sidePanelTextInputFocused: editorChrome.sidePanelTextInputFocused
        zoomCenterX: editorChrome.zoomCenterX
        zoomCenterY: editorChrome.zoomCenterY
        onOpenProjectRequested: openProjectDialog.open()
        onNewProjectRequested: newProjectDialog.open()
        onZoomAtRequested: (x, y, factor) => editorChrome.zoomAtRequested(x, y, factor)
        onResetZoomRequested: editorChrome.resetZoomRequested()
        onEscapeRequested: editorChrome.escapeRequested()
    }

    MenuBar {
        id: chromeMenuBar

        FileMenu {
            newAction: editorCommands.newAction
            openAction: editorCommands.openAction
            saveAction: editorCommands.saveAction
            saveAllAction: editorCommands.saveAllAction
            quitAction: editorCommands.quitAction
        }

        EditMenu {
            copyAction: editorCommands.copyAction
            pasteAction: editorCommands.pasteAction
            deleteAction: editorCommands.deleteAction
            duplicateAction: editorCommands.duplicateAction
        }

        ViewMenu {
            zoomInAction: editorCommands.zoomInAction
            zoomOutAction: editorCommands.zoomOutAction
            resetZoomAction: editorCommands.resetZoomAction
            rawOverlayAction: editorCommands.rawOverlayAction
        }
    }


    Popup {
        id: toastPopup

        x: Math.max(24, (editorChrome.hostWidth - width) / 2)
        y: editorChrome.hostHeight - height - 24
        modal: false
        closePolicy: Popup.NoAutoClose

        Label {
            id: toastLabel

            padding: 12
            color: editorChrome.hostPalette.highlightedText
        }

        background: Rectangle {
            color: editorChrome.hostPalette.highlight
            radius: 6
        }

    }

    Timer {
        id: toastTimer

        interval: 2200
        onTriggered: toastPopup.close()
    }

    FolderDialog {
        id: openProjectDialog

        title: qsTr("Open Project")
        onAccepted: editorChrome.editor.openProjectUrl(selectedFolder)
    }

    FolderDialog {
        id: newProjectDialog

        title: qsTr("New Project")
        onAccepted: editorChrome.editor.newProjectUrl(selectedFolder)
    }

    ColorDialog {
        id: colorDialog
        objectName: "colorDialog"

        onAccepted: editorChrome.applyColorDialogSelection(editorChrome.dialogColorHex(selectedColor))
    }

}
