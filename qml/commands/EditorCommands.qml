import QtQuick
import QtQuick.Controls

Item {
    id: editorCommands

    property var editor: null
    property real zoomCenterX: 0
    property real zoomCenterY: 0
    property bool pageNavigationEnabled: true
    property bool sidePanelTextInputFocused: false
    readonly property bool editingText: editor ? editor.editingText : false
    readonly property bool hasSelectedBox: editor ? editor.selectedIndex >= 0 : false
    readonly property bool boxActionsAvailable: hasSelectedBox && !editingText
    readonly property bool projectAvailable: editor ? editor.hasProject : false

    property alias openAction: openAction
    property alias newAction: newAction
    property alias saveAction: saveAction
    property alias saveAllAction: saveAllAction
    property alias previousPageAction: previousPageAction
    property alias nextPageAction: nextPageAction
    property alias copyAction: copyAction
    property alias pasteAction: pasteAction
    property alias duplicateAction: duplicateAction
    property alias deleteAction: deleteAction
    property alias quitAction: quitAction
    property alias zoomInAction: zoomInAction
    property alias zoomOutAction: zoomOutAction
    property alias resetZoomAction: resetZoomAction
    property alias rawOverlayAction: rawOverlayAction

    signal openProjectRequested()
    signal newProjectRequested()
    signal zoomAtRequested(real x, real y, real factor)
    signal resetZoomRequested()
    signal escapeRequested()

    width: 0
    height: 0

    Action {
        id: openAction

        text: qsTr("Open")
        shortcut: StandardKey.Open
        enabled: editorCommands.editor && editorCommands.editor.actionEnabled("open")
        onTriggered: editorCommands.openProjectRequested()
    }

    Action {
        id: newAction

        text: qsTr("New")
        shortcut: StandardKey.New
        enabled: editorCommands.editor && editorCommands.editor.actionEnabled("new")
        onTriggered: editorCommands.newProjectRequested()
    }

    Action {
        id: saveAction

        text: qsTr("Save")
        shortcut: StandardKey.Save
        enabled: editorCommands.editor && editorCommands.editor.hasProject
        onTriggered: editorCommands.editor.save()
    }

    Action {
        id: saveAllAction

        text: qsTr("Save All")
        shortcut: "Ctrl+Shift+S"
        enabled: editorCommands.editor && editorCommands.editor.hasProject
        onTriggered: editorCommands.editor.saveAll()
    }

    Action {
        id: previousPageAction

        text: qsTr("Previous")
        shortcut: "PgUp"
        enabled: editorCommands.pageNavigationEnabled && editorCommands.editor && editorCommands.editor.canGoPrevious
        onTriggered: editorCommands.editor.previousPage()
    }

    Action {
        id: nextPageAction

        text: qsTr("Next")
        shortcut: "PgDown"
        enabled: editorCommands.pageNavigationEnabled && editorCommands.editor && editorCommands.editor.canGoNext
        onTriggered: editorCommands.editor.nextPage()
    }

    Action {
        id: copyAction
        objectName: "copyAction"

        text: qsTr("Copy")
        shortcut: StandardKey.Copy
        enabled: editorCommands.editor && editorCommands.boxActionsAvailable && editorCommands.editor.actionEnabled("copy")
        onTriggered: editorCommands.editor.copySelected()
    }

    Action {
        id: pasteAction
        objectName: "pasteAction"

        text: qsTr("Paste")
        shortcut: StandardKey.Paste
        enabled: editorCommands.editor && editorCommands.projectAvailable && !editorCommands.editingText && editorCommands.editor.actionEnabled("paste")
        onTriggered: editorCommands.editor.pasteBox()
    }

    Action {
        id: duplicateAction
        objectName: "duplicateAction"

        text: qsTr("Duplicate")
        enabled: editorCommands.editor && editorCommands.boxActionsAvailable && editorCommands.editor.actionEnabled("duplicate")
        onTriggered: editorCommands.editor.duplicateSelected()
    }

    Action {
        id: deleteAction
        objectName: "deleteAction"

        text: qsTr("Delete")
        shortcut: StandardKey.Delete
        enabled: editorCommands.editor && editorCommands.boxActionsAvailable && editorCommands.editor.actionEnabled("delete")
        onTriggered: editorCommands.editor.deleteSelected()
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
        onTriggered: editorCommands.zoomAtRequested(editorCommands.zoomCenterX, editorCommands.zoomCenterY, 1.1)
    }

    Action {
        id: zoomOutAction

        text: qsTr("Zoom Out")
        shortcut: "Ctrl+-"
        onTriggered: editorCommands.zoomAtRequested(editorCommands.zoomCenterX, editorCommands.zoomCenterY, 1 / 1.1)
    }

    Action {
        id: resetZoomAction

        text: qsTr("Reset Zoom")
        shortcut: "Ctrl+0"
        onTriggered: editorCommands.resetZoomRequested()
    }

    Action {
        id: rawOverlayAction

        text: qsTr("Show Raw Overlay")
        checkable: true
        checked: editorCommands.editor ? editorCommands.editor.rawVisible : false
        shortcut: "Ctrl+H"
        onTriggered: editorCommands.editor.rawVisible = checked
    }

    Shortcut {
        sequence: "Ctrl+="
        onActivated: zoomInAction.trigger()
    }

    Shortcut {
        sequence: "Ctrl+Space"
        enabled: editorCommands.editor && !editorCommands.sidePanelTextInputFocused
        onActivated: editorCommands.editor.applyNextPageText()
    }

    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: editorCommands.escapeRequested()
    }
}
