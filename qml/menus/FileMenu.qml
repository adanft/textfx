import QtQuick.Controls

Menu {
    title: qsTr("File")

    property Action newAction
    property Action openAction
    property Action saveAction
    property Action saveAllAction
    property Action quitAction

    MenuItem {
        action: newAction
        text: qsTr("New\tCtrl+N")
    }

    MenuItem {
        action: openAction
        text: qsTr("Open\tCtrl+O")
    }

    MenuItem {
        action: saveAction
        text: qsTr("Save\tCtrl+S")
    }

    MenuItem {
        action: saveAllAction
        text: qsTr("Save All\tCtrl+Shift+S")
    }

    MenuSeparator {
    }

    MenuItem {
        action: quitAction
    }
}
