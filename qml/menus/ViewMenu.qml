import QtQuick.Controls

Menu {
    title: qsTr("View")

    property Action zoomInAction
    property Action zoomOutAction
    property Action resetZoomAction
    property Action rawOverlayAction

    MenuItem {
        action: zoomInAction
        text: qsTr("Zoom In\tCtrl++")
    }

    MenuItem {
        action: zoomOutAction
        text: qsTr("Zoom Out\tCtrl+-")
    }

    MenuItem {
        action: resetZoomAction
        text: qsTr("Reset Zoom\tCtrl+0")
    }

    MenuSeparator {
    }

    MenuItem {
        action: rawOverlayAction
        text: qsTr("Show Raw Overlay\tCtrl+H")
    }
}
