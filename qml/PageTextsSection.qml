import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: pageTextsSection

    property var editor: null
    readonly property bool sectionReady: editor && editor.hasProject && editor.pageTexts.length > 0

    enabled: sectionReady

    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        Label {
            text: qsTr("Page Texts")
            font.bold: true
            enabled: pageTextsSection.sectionReady
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: pageTextsSection.sectionReady ? pageTextsSection.editor.pageTexts.length : 0
            enabled: pageTextsSection.sectionReady
        }

    }

    GroupBox {
        id: pageTextsFrame

        readonly property real minimumListHeight: 200

        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.minimumHeight: minimumListHeight + topPadding + bottomPadding
        Layout.fillHeight: true
        enabled: pageTextsSection.sectionReady

        ListView {
            id: pageTextsList

            objectName: "leftInspectorPageTextsList"
            anchors.fill: parent
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: pageTextsFrame.minimumListHeight
            clip: true
            model: pageTextsSection.sectionReady ? pageTextsSection.editor.pageTexts : []

            delegate: ItemDelegate {
                id: pageTextDelegate

                objectName: "leftInspectorPageTextDelegate"
                width: ListView.view.width
                hoverEnabled: true
                text: modelData
                onClicked: pageTextsSection.editor.applyPageText(index)

                contentItem: Label {
                    text: pageTextDelegate.text
                    font: pageTextDelegate.font
                    enabled: pageTextDelegate.enabled
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: pageTextDelegate.down || pageTextDelegate.hovered ? Qt.alpha(pageTextDelegate.palette.highlight, pageTextDelegate.down ? 0.24 : 0.14) : "transparent"
                }

            }

        }

    }

}
