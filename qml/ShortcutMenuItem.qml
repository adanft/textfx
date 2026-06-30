import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: shortcutMenuItem

    property string shortcutLabel: ""

    contentItem: Item {
        implicitWidth: shortcutRow.implicitWidth
        implicitHeight: shortcutRow.implicitHeight

        RowLayout {
            id: shortcutRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: shortcutMenuItem.checkable ? (Math.max(shortcutMenuItem.indicator ? shortcutMenuItem.indicator.width : 0, shortcutMenuItem.indicator ? shortcutMenuItem.indicator.implicitWidth : 0, 20) + shortcutMenuItem.spacing) : 0
            spacing: 24

            Label {
                text: shortcutMenuItem.text
                enabled: shortcutMenuItem.enabled
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: shortcutMenuItem.shortcutLabel
                visible: shortcutMenuItem.shortcutLabel.length > 0
                enabled: shortcutMenuItem.enabled
                color: shortcutMenuItem.palette.mid
            }
        }
    }
}
