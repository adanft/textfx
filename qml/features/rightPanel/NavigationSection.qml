import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../constants/UiConstants.js" as UiConstants

ColumnLayout {
    id: navigationSection

    objectName: "rightInspectorNavigationSection"

    property var editor: null
    property real displayScale: 1
    property real minimumDisplayScale: 0.5
    property real maximumDisplayScale: 6
    property bool pageNavigationEnabled: true
    readonly property bool sectionReady: editor && editor.pageCount > 0
    readonly property real displayScaleSnapThreshold: 0.1
    readonly property string maximumDisplayScaleLabel: displayScaleLabel(maximumDisplayScale)

    signal zoomRequested(real displayScale)

    function detentedDisplayScale(displayScale) {
        return Math.abs(displayScale - 1) <= displayScaleSnapThreshold + 1e-9 ? 1 : displayScale;
    }

    function displayScaleLabel(displayScale) {
        return Number(displayScale * 100).toLocaleString(Qt.locale(), "f", 1) + "%";
    }

    Label {
        text: qsTr("Navigation")
        font.bold: true
        enabled: navigationSection.sectionReady
    }

    GroupBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: navigationSection.sectionReady

        ColumnLayout {
            anchors.fill: parent
            spacing: UiConstants.panelSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: UiConstants.panelSpacing

                Label {
                    id: zoomLabel

                    objectName: "rightInspectorZoomLabel"
                    text: navigationSection.displayScaleLabel(navigationSection.displayScale)
                    color: zoomLabel.palette.text
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: zoomLabelMetrics.advanceWidth(navigationSection.maximumDisplayScaleLabel)

                    FontMetrics {
                        id: zoomLabelMetrics

                        font: zoomLabel.font
                    }

                }

                Slider {
                    objectName: "rightInspectorZoomSlider"

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    from: navigationSection.minimumDisplayScale
                    to: navigationSection.maximumDisplayScale
                    value: navigationSection.displayScale
                    live: true
                    onMoved: {
                        navigationSection.zoomRequested(navigationSection.detentedDisplayScale(value));
                    }
                }

            }

            Label {
                text: navigationSection.editor && navigationSection.editor.currentPageName.length > 0 ? navigationSection.editor.currentPageName : qsTr("No page")
                font.bold: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            Flow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: UiConstants.panelSpacing

                Button {
                    text: qsTr("Previous")
                    enabled: navigationSection.pageNavigationEnabled && navigationSection.editor && navigationSection.editor.canGoPrevious
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/qt/qml/TextFX/assets/icons/flat/arrow-left.svg"
                    icon.width: UiConstants.iconSize
                    icon.height: UiConstants.iconSize
                    icon.color: !enabled ? palette.mid : palette.buttonText
                    onClicked: navigationSection.editor.previousPage()
                }

                Button {
                    objectName: "rightInspectorNextPageButton"
                    text: qsTr("Next")
                    enabled: navigationSection.pageNavigationEnabled && navigationSection.editor && navigationSection.editor.canGoNext
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/qt/qml/TextFX/assets/icons/flat/arrow-right.svg"
                    icon.width: UiConstants.iconSize
                    icon.height: UiConstants.iconSize
                    icon.color: !enabled ? palette.mid : palette.buttonText
                    onClicked: navigationSection.editor.nextPage()
                }

            }

            ComboBox {
                id: pageSelect

                objectName: "rightInspectorPageSelect"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: navigationSection.editor ? navigationSection.editor.pageLabels : []
                currentIndex: navigationSection.editor ? navigationSection.editor.currentPageIndex : -1
                enabled: navigationSection.pageNavigationEnabled && navigationSection.editor && navigationSection.editor.pageCount > 0
                onActivated: (index) => {
                    return navigationSection.editor.goToPage(index);
                }

                contentItem: Label {
                    text: pageSelect.displayText
                    font: pageSelect.font
                    enabled: pageSelect.enabled
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

            }

            Label {
                text: navigationSection.editor && navigationSection.editor.pageCount > 0 ? qsTr("Page %1 of %2").arg(navigationSection.editor.currentPageIndex + 1).arg(navigationSection.editor.pageCount) : qsTr("Page 0 of 0")
                color: palette.mid
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            CheckBox {
                objectName: "rightInspectorRawVisibleCheckBox"
                text: qsTr("Show Raw")
                checked: navigationSection.editor ? navigationSection.editor.rawVisible : false
                onToggled: {
                    if (navigationSection.editor)
                        navigationSection.editor.rawVisible = checked;
                }
            }

        }

    }

}
