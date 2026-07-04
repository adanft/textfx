import QtQuick.Controls

Menu {
    title: qsTr("Edit")

    property Action copyAction
    property Action pasteAction
    property Action deleteAction
    property Action duplicateAction

    MenuItem {
        action: copyAction
        text: qsTr("Copy\tCtrl+C")
    }

    MenuItem {
        action: pasteAction
        text: qsTr("Paste\tCtrl+V")
    }

    MenuItem {
        action: deleteAction
        text: qsTr("Delete\tDel")
    }

    MenuItem {
        action: duplicateAction
    }
}
