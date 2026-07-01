import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Item {
    id: editorChrome

    property var editor: null
    property var hostPalette
    property real hostWidth: 0
    property real hostHeight: 0
    property real zoomCenterX: 0
    property real zoomCenterY: 0
    property bool sidePanelTextInputFocused: false
    property alias menuBar: chromeMenuBar
    property string colorDialogSetter: ""

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
        else if (colorDialogSetter === "shadow")
            editor.setSelectedShadowColor(hex);
        else if (colorDialogSetter === "gradientA")
            editor.setSelectedGradientColorA(hex);
        else if (colorDialogSetter === "gradientB")
            editor.setSelectedGradientColorB(hex);
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

    Action {
        id: openAction

        text: qsTr("Open")
        shortcut: StandardKey.Open
        enabled: editorChrome.editor && editorChrome.editor.actionEnabled("open")
        onTriggered: openProjectDialog.open()
    }

    Action {
        id: saveAction

        text: qsTr("Save")
        shortcut: StandardKey.Save
        enabled: editorChrome.editor && editorChrome.editor.hasProject
        onTriggered: editorChrome.editor.save()
    }

    Action {
        id: saveAllAction

        text: qsTr("Save All")
        shortcut: "Ctrl+Shift+S"
        enabled: editorChrome.editor && editorChrome.editor.hasProject
        onTriggered: editorChrome.editor.saveAll()
    }

    Action {
        id: previousPageAction

        text: qsTr("Previous")
        shortcut: "PgUp"
        enabled: editorChrome.editor && editorChrome.editor.canGoPrevious
        onTriggered: editorChrome.editor.previousPage()
    }

    Action {
        id: nextPageAction

        text: qsTr("Next")
        shortcut: "PgDown"
        enabled: editorChrome.editor && editorChrome.editor.canGoNext
        onTriggered: editorChrome.editor.nextPage()
    }

    Action {
        id: copyAction

        text: qsTr("Copy")
        shortcut: StandardKey.Copy
        enabled: editorChrome.editor && editorChrome.editor.actionEnabled("copy") && !editorChrome.editor.editingText
        onTriggered: editorChrome.editor.copySelected()
    }

    Action {
        id: pasteAction

        text: qsTr("Paste")
        shortcut: StandardKey.Paste
        enabled: editorChrome.editor && editorChrome.editor.actionEnabled("paste") && !editorChrome.editor.editingText
        onTriggered: editorChrome.editor.pasteBox()
    }

    Action {
        id: duplicateAction

        text: qsTr("Duplicate")
        enabled: editorChrome.editor && editorChrome.editor.actionEnabled("duplicate") && !editorChrome.editor.editingText
        onTriggered: editorChrome.editor.duplicateSelected()
    }

    Action {
        id: deleteAction

        text: qsTr("Delete")
        shortcut: StandardKey.Delete
        enabled: editorChrome.editor && editorChrome.editor.actionEnabled("delete") && !editorChrome.editor.editingText
        onTriggered: editorChrome.editor.deleteSelected()
    }

    Action {
        id: quitAction

        text: qsTr("Quit")
        onTriggered: Qt.quit()
    }

    Action {
        id: zoomInAction

        text: qsTr("Zoom In")
        shortcut: "Ctrl++"
        onTriggered: editorChrome.zoomAtRequested(editorChrome.zoomCenterX, editorChrome.zoomCenterY, 1.1)
    }

    Action {
        id: zoomOutAction

        text: qsTr("Zoom Out")
        shortcut: "Ctrl+-"
        onTriggered: editorChrome.zoomAtRequested(editorChrome.zoomCenterX, editorChrome.zoomCenterY, 1 / 1.1)
    }

    Action {
        id: resetZoomAction

        text: qsTr("Reset Zoom")
        shortcut: "Ctrl+0"
        onTriggered: editorChrome.resetZoomRequested()
    }

    Action {
        id: rawOverlayAction

        text: qsTr("Show Raw Overlay")
        checkable: true
        checked: editorChrome.editor ? editorChrome.editor.rawVisible : false
        shortcut: "Ctrl+H"
        onTriggered: editorChrome.editor.rawVisible = checked
    }

    MenuBar {
        id: chromeMenuBar

        Menu {
            title: qsTr("File")

            ShortcutMenuItem {
                action: openAction
                shortcutLabel: "Ctrl+O"
            }

            ShortcutMenuItem {
                action: saveAction
                shortcutLabel: "Ctrl+S"
            }

            ShortcutMenuItem {
                action: saveAllAction
                shortcutLabel: "Ctrl+Shift+S"
            }

            MenuSeparator {
            }

            ShortcutMenuItem {
                action: quitAction
            }

        }

        Menu {
            title: qsTr("Edit")

            ShortcutMenuItem {
                action: copyAction
                shortcutLabel: "Ctrl+C"
            }

            ShortcutMenuItem {
                action: pasteAction
                shortcutLabel: "Ctrl+V"
            }

            ShortcutMenuItem {
                action: deleteAction
                shortcutLabel: "Del"
            }

            ShortcutMenuItem {
                action: duplicateAction
            }

        }

        Menu {
            title: qsTr("View")

            ShortcutMenuItem {
                action: zoomInAction
                shortcutLabel: "Ctrl++"
            }

            ShortcutMenuItem {
                action: zoomOutAction
                shortcutLabel: "Ctrl+-"
            }

            ShortcutMenuItem {
                action: resetZoomAction
                shortcutLabel: "Ctrl+0"
            }

            MenuSeparator {
            }

            ShortcutMenuItem {
                action: rawOverlayAction
                shortcutLabel: "Ctrl+H"
            }

        }

    }

    Shortcut {
        sequence: "Ctrl+="
        onActivated: zoomInAction.trigger()
    }

    Shortcut {
        sequence: "Ctrl+Space"
        enabled: editorChrome.editor && !editorChrome.editor.editingText && !editorChrome.sidePanelTextInputFocused
        onActivated: editorChrome.editor.applyNextPageText()
    }

    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: editorChrome.escapeRequested()
    }

    Popup {
        id: toastPopup

        x: editorChrome.hostWidth - width - 24
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

    ColorDialog {
        id: colorDialog

        onAccepted: editorChrome.applyColorDialogSelection(editorChrome.dialogColorHex(selectedColor))
    }

}
