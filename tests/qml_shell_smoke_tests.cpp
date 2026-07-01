#include "app/EditorController.h"
#include "qt_test_helpers.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

#include <cmath>

using namespace textfx;
using namespace textfx::test;

class QmlShellSmokeTests final : public QObject {
  Q_OBJECT

private slots:
  void qmlCtrlSpaceIgnoresFocusedSidePanelTextInputs() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(source.contains(QStringLiteral("sidePanelFocusedTextInputs")));
    QVERIFY(source.contains(QStringLiteral(
        "sidePanelTextInputFocused: window.sidePanelFocusedTextInputs > 0")));
    QVERIFY(source.contains(QStringLiteral(
        "enabled: editorChrome.editor && !editorChrome.editor.editingText && "
        "!editorChrome.sidePanelTextInputFocused")));
    QVERIFY(source.contains(QStringLiteral(
        "onActiveFocusChanged: "
        "leftInspectorPanel.textInputFocusChanged(activeFocus)")));
  }

  void qmlEscapeCancelsCurrentContextInOrder() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    const qsizetype handlerStart =
        source.indexOf(QStringLiteral("function handleEscape()"));
    const qsizetype handlerEnd =
        source.indexOf(QStringLiteral("menuBar: chrome.menuBar"), handlerStart);
    QVERIFY(handlerStart >= 0);
    QVERIFY(handlerEnd > handlerStart);
    const QString handler = source.mid(handlerStart, handlerEnd - handlerStart);

    const qsizetype edit =
        handler.indexOf(QStringLiteral("if (Editor.editingText)"));
    const qsizetype transient = handler.indexOf(QStringLiteral(
        "else if (dragMode !== editorInteraction.dragModeIdle)"));
    const qsizetype deselect =
        handler.indexOf(QStringLiteral("else if (Editor.selectedIndex >= 0)"));
    QVERIFY(edit >= 0);
    QVERIFY(transient > edit);
    QVERIFY(deselect > transient);
    QVERIFY(handler.contains(QStringLiteral("Editor.endTextEdit()")));
    QVERIFY(handler.contains(QStringLiteral("canvas.forceActiveFocus()")));
    QVERIFY(handler.contains(QStringLiteral("endResizeDrag(false)")));
    QVERIFY(handler.contains(
        QStringLiteral("dragMode = editorInteraction.dragModeIdle")));
    QVERIFY(handler.contains(QStringLiteral("Editor.selectBox(-1)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral(
            "Shortcut { sequence: \"Esc\"; context: Qt.ApplicationShortcut; "
            "onActivated: editorChrome.escapeRequested() }")));
    QVERIFY(source.contains(
        QStringLiteral("onEscapeRequested: window.handleEscape()")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral("if (event.key === Qt.Key_Escape) { "
                       "rootWindow.handleEscape(); event.accepted = true }")));
    QVERIFY(!handler.contains(QStringLiteral("Editor.deleteSelected")));
  }

  void qmlColorControlsUseNativeColorDialog() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString colorButtonSource =
        readQmlFile(QStringLiteral("ColorButton.qml"));
    const QString editorChromeSource =
        readQmlFile(QStringLiteral("EditorChrome.qml"));
    const QString source = qmlSource();

    QVERIFY(
        editorChromeSource.contains(QStringLiteral("import QtQuick.Dialogs")));
    QVERIFY(editorChromeSource.contains(QStringLiteral("ColorDialog")));
    QVERIFY(colorButtonSource.contains(
        QStringLiteral("color: colorButton.enabled ? colorButton.swatchColor : "
                       "colorButton.palette.mid")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        colorButtonSource,
        QStringLiteral("Label { text: colorButton.swatchText; enabled: "
                       "colorButton.enabled; Layout.fillWidth: true }")));
    QVERIFY(editorChromeSource.contains(QStringLiteral("selectedColor")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("dialogColorHex(selectedColor)")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editor.setSelectedTextColor(hex)")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editor.setSelectedOutlineColor(hex)")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editor.setSelectedShadowColor(hex)")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editor.setSelectedGradientColorA(hex)")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editor.setSelectedGradientColorB(hex)")));
    QVERIFY(!mainSource.contains(QStringLiteral("ColorDialog {")));
    QVERIFY(
        !mainSource.contains(QStringLiteral("dialogColorHex(selectedColor)")));
    QVERIFY(!source.contains(QStringLiteral("inputMask")));
    QVERIFY(!source.contains(
        QStringLiteral("onEditingFinished: Editor.setSelectedTextColor")));
    QVERIFY(!source.contains(
        QStringLiteral("onEditingFinished: Editor.setSelectedOutlineColor")));
    QVERIFY(!source.contains(
        QStringLiteral("onEditingFinished: Editor.setSelectedShadowColor")));
    QVERIFY(!source.contains(
        QStringLiteral("onEditingFinished: Editor.setSelectedGradientColorA")));
    QVERIFY(!source.contains(
        QStringLiteral("onEditingFinished: Editor.setSelectedGradientColorB")));
  }

  void qmlChromeUsesPaletteAndResponsivePanels() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString canvasShellSource =
        readQmlFile(QStringLiteral("CentralCanvasShell.qml"));
    const QString leftPanelSource =
        readQmlFile(QStringLiteral("LeftInspectorPanel.qml"));
    const QString rightPanelSource =
        readQmlFile(QStringLiteral("RightInspectorPanel.qml"));
    const QString source = qmlSource();

    for (const QString &removedChromeColor :
         {QStringLiteral("#202124"), QStringLiteral("#24282f"),
          QStringLiteral("#111318"), QStringLiteral("#c9d1d9"),
          QStringLiteral("#8b949e"), QStringLiteral("#30363d"),
          QStringLiteral("#58a6ff"), QStringLiteral("#58a6ff22")}) {
      QVERIFY2(!source.contains(removedChromeColor),
               qPrintable(removedChromeColor));
    }

    QVERIFY(source.contains(QStringLiteral("color: window.palette.window")));
    QVERIFY(
        source.contains(QStringLiteral("color: canvasShell.hostPalette.base")));
    QVERIFY(source.contains(QStringLiteral("window.palette.highlight")));
    QVERIFY(source.contains(QStringLiteral("hostPalette: window.palette")));
    QVERIFY(
        source.contains(QStringLiteral("editorChrome.hostPalette.highlight")));
    QVERIFY(source.contains(
        QStringLiteral("editorChrome.hostPalette.highlightedText")));
    QVERIFY(source.contains(QStringLiteral("SplitView {")));
    QVERIFY(source.contains(QStringLiteral("orientation: Qt.Horizontal")));
    QVERIFY(source.contains(QStringLiteral("SplitView.minimumWidth: 240")));
    QVERIFY(source.contains(QStringLiteral("SplitView.preferredWidth: 280")));
    QVERIFY(source.contains(QStringLiteral("SplitView.fillWidth: true")));
    QVERIFY(mainSource.contains(QStringLiteral("LeftInspectorPanel {")));
    QVERIFY(mainSource.contains(
        QStringLiteral("objectName: \"leftInspectorPanel\"")));
    QVERIFY(source.contains(QStringLiteral("id: sidePanel")));
    QVERIFY(mainSource.contains(QStringLiteral("CentralCanvasShell {")));
    QVERIFY(mainSource.contains(
        QStringLiteral("objectName: \"centralCanvasShell\"")));
    QVERIFY(source.contains(QStringLiteral("id: canvas")));
    QVERIFY(canvasShellSource.contains(
        QStringLiteral("objectName: \"centralCanvas\"")));
    QVERIFY(source.contains(QStringLiteral("id: rightPanel")));
    QVERIFY(mainSource.contains(QStringLiteral("RightInspectorPanel {")));
    QVERIFY(mainSource.contains(
        QStringLiteral("objectName: \"rightInspectorPanel\"")));
    QVERIFY(canvasShellSource.contains(QStringLiteral("x: -canvasShell.x")));
    QVERIFY(canvasShellSource.contains(QStringLiteral(
        "width: canvasShell.SplitView.view ? canvasShell.SplitView.view.width "
        ": canvasShell.width")));

    const qsizetype sidePanelStart =
        mainSource.indexOf(QStringLiteral("id: sidePanel"));
    const qsizetype canvasShellStart =
        mainSource.indexOf(QStringLiteral("id: canvasShell"), sidePanelStart);
    const qsizetype canvasStart =
        canvasShellSource.indexOf(QStringLiteral("id: canvas\n"));
    QVERIFY(sidePanelStart >= 0);
    QVERIFY(canvasShellStart > sidePanelStart);
    QVERIFY(canvasStart >= 0);
    const qsizetype rightPanelStart =
        mainSource.indexOf(QStringLiteral("id: rightPanel"), canvasShellStart);
    QVERIFY(rightPanelStart > canvasShellStart);
    QVERIFY(leftPanelSource.contains(QStringLiteral("z: 1")));
    QVERIFY(mainSource.mid(canvasShellStart, rightPanelStart - canvasShellStart)
                .contains(QStringLiteral("id: canvasShell")));
    QVERIFY(
        canvasShellSource.mid(0, canvasStart).contains(QStringLiteral("z: 0")));
    QVERIFY(canvasShellSource
                .mid(canvasStart,
                     canvasShellSource.indexOf(
                         QStringLiteral("x: -canvasShell.x"), canvasStart) -
                         canvasStart)
                .contains(QStringLiteral("z: 0")));
    QVERIFY(rightPanelSource.contains(QStringLiteral("z: 1")));
    const QString &sidePanelSource = leftPanelSource;
    QVERIFY(leftPanelSource.contains(QStringLiteral("Pane {")));
    QVERIFY(sidePanelSource.contains(QStringLiteral("GroupBox")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: "
                       "true; enabled: textPropertiesSection.sectionReady }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; "
                       "enabled: textPresetsSection.sectionReady }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; "
                       "enabled: pageTextsSection.sectionReady }")));
    QVERIFY(!sidePanelSource.contains(
        QStringLiteral("ScrollBar.vertical.policy: ScrollBar.AlwaysOff")));
    QVERIFY(!sidePanelSource.contains(
        QStringLiteral("ScrollBar.horizontal.policy: ScrollBar.AlwaysOff")));
    QVERIFY(
        !sidePanelSource.contains(QStringLiteral("Layout.minimumHeight: 160")));
    QVERIFY(sidePanelSource.contains(
        QStringLiteral("Layout.minimumHeight: minimumListHeight + topPadding + "
                       "bottomPadding")));
    QVERIFY(sidePanelSource.contains(QStringLiteral("id: leftPanelScroll")));
    QVERIFY(sidePanelSource.contains(
        QStringLiteral("contentWidth: availableWidth")));
    QVERIFY(sidePanelSource.contains(
        QStringLiteral("width: leftPanelScroll.availableWidth")));
    QVERIFY(sidePanelSource.contains(QStringLiteral(
        "height: Math.max(implicitHeight, leftPanelScroll.availableHeight)")));
    QVERIFY(!sidePanelSource.contains(QStringLiteral(
        "height: Math.max(implicitHeight, sidePanel.availableHeight)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("TextField { id: presetNameField; Layout.fillWidth: "
                       "true; Layout.minimumWidth: 0")));
    QVERIFY(sidePanelSource.contains(QStringLiteral(
        "RowLayout {\n                            Layout.fillWidth: true\n     "
        "                       Layout.minimumWidth: 0\n                       "
        "     spacing: 8")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral(
            "SpinBox { objectName: \"leftInspectorFontSizeSpinBox\"; "
            "Layout.fillWidth: true; Layout.minimumWidth: 0; from: "
            "leftInspectorPanel.editorLimits.minimumFontSize; to: "
            "leftInspectorPanel.editorLimits.maximumFontSize")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral(
            "SpinBox { objectName: \"leftInspectorLineSpacingSpinBox\"; "
            "Layout.fillWidth: true; Layout.minimumWidth: 0; from: "
            "leftInspectorPanel.editorLimits.minimumTextSpacing; to: "
            "leftInspectorPanel.editorLimits.maximumTextSpacing")));
    QVERIFY(sidePanelSource.count(QStringLiteral(
                "Flow {\n                            Layout.fillWidth: true\n  "
                "                          Layout.minimumWidth: 0")) >= 3);
    QVERIFY(rightPanelSource.contains(QStringLiteral("Pane {")));
    QVERIFY(rightPanelSource.contains(QStringLiteral("z: 1")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("SplitView.minimumWidth: 180")));
    QVERIFY(rightPanelSource.contains(QStringLiteral("id: rightPanelScroll")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("width: rightPanelScroll.availableWidth")));
    const qsizetype propertiesStart = indexOfIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Text Properties\"); font.bold: "
                       "true; enabled: textPropertiesSection.sectionReady }"));
    const qsizetype presetsStart = indexOfIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Text Presets\"); font.bold: true; "
                       "enabled: textPresetsSection.sectionReady }"));
    const qsizetype pageTextsStart = indexOfIgnoringWhitespace(
        sidePanelSource,
        QStringLiteral("Label { text: qsTr(\"Page Texts\"); font.bold: true; "
                       "enabled: pageTextsSection.sectionReady }"));
    QVERIFY(propertiesStart >= 0);
    QVERIFY(presetsStart > propertiesStart);
    QVERIFY(pageTextsStart > presetsStart);
    QVERIFY(sidePanelSource.mid(propertiesStart, presetsStart - propertiesStart)
                .contains(QStringLiteral("Layout.fillWidth: true")));
    QVERIFY(sidePanelSource.mid(presetsStart, pageTextsStart - presetsStart)
                .contains(QStringLiteral("Layout.fillWidth: true")));
    QVERIFY(sidePanelSource.mid(pageTextsStart)
                .contains(QStringLiteral("Layout.fillWidth: true")));
    const qsizetype pageTextsBox = sidePanelSource.indexOf(
        QStringLiteral("id: pageTextsFrame"), pageTextsStart);
    const qsizetype pageTextsList = sidePanelSource.indexOf(
        QStringLiteral("id: pageTextsList"), pageTextsBox);
    QVERIFY(pageTextsBox > pageTextsStart);
    QVERIFY(pageTextsList > pageTextsBox);
    QVERIFY(sidePanelSource.mid(pageTextsBox, pageTextsList - pageTextsBox)
                .contains(QStringLiteral("Layout.fillHeight: true")));
    QVERIFY(
        sidePanelSource
            .mid(pageTextsList,
                 sidePanelSource.indexOf(
                     QStringLiteral("delegate: ItemDelegate"), pageTextsList) -
                     pageTextsList)
            .contains(QStringLiteral("Layout.fillHeight: true")));
    QVERIFY(!sidePanelSource.contains(QStringLiteral("color: \"#")));
    QVERIFY(!sidePanelSource.contains(QStringLiteral("border.color: \"#")));
  }

  void qmlStartScreenShowsNewOpenActions() {
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString chromeSource =
        readQmlFile(QStringLiteral("EditorChrome.qml"));

    QVERIFY(mainSource.contains(QStringLiteral("objectName: \"startScreen\"")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: !Editor.hasProject")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: Editor.hasProject")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: Editor.hasProject")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Open \")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"New \")")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onClicked: chrome.openProjectPicker()")));
    QVERIFY(mainSource.contains(
        QStringLiteral("onClicked: chrome.openNewProjectPicker()")));
    QVERIFY(
        chromeSource.contains(QStringLiteral("function openProjectPicker()")));
    QVERIFY(chromeSource.contains(
        QStringLiteral("function openNewProjectPicker()")));
    QVERIFY(chromeSource.contains(
        QStringLiteral("editorChrome.editor.openProjectUrl(selectedFolder)")));
    QVERIFY(chromeSource.contains(
        QStringLiteral("editorChrome.editor.newProjectUrl(selectedFolder)")));
  }

  void qmlStartScreenIsVisibleBeforeProjectOpen() {
    registerQmlTypes();

    EditorController editor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *startScreen = qobject_cast<QQuickItem *>(findVisualChildByName(
        window->contentItem(), QStringLiteral("startScreen")));
    auto *openButton = qobject_cast<QQuickItem *>(findVisualChildByName(
        window->contentItem(), QStringLiteral("startOpenButton")));
    auto *newButton = qobject_cast<QQuickItem *>(findVisualChildByName(
        window->contentItem(), QStringLiteral("startNewButton")));
    QVERIFY(startScreen);
    QVERIFY(openButton);
    QVERIFY(newButton);
    QTRY_VERIFY(startScreen->isVisible());
    QTRY_VERIFY(openButton->isVisible());
    QTRY_VERIFY(newButton->isVisible());
    QCOMPARE(openButton->property("text").toString(),
             QStringLiteral("Open "));
    QCOMPARE(newButton->property("text").toString(), QStringLiteral("New "));
  }

  void qmlPageScaleIsNotBoundToCanvasViewport() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(sourceContainsIgnoringWhitespace(
        readQmlFile(QStringLiteral("ViewportMetrics.qml")),
        QStringLiteral("property real pageBaseScale: 1")));
    QVERIFY(source.contains(QStringLiteral("ViewportMetrics {")));
    QVERIFY(source.contains(QStringLiteral("objectName: \"viewportMetrics\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source, QStringLiteral("function fitPageScale() { return "
                               "viewportMetrics.fitPageScale() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        source,
        QStringLiteral(
            "function pageScale() { return viewportMetrics.pageScale() }")));
    QVERIFY(source.contains(QStringLiteral("onStatusChanged")));
    QVERIFY(
        sourceContainsIgnoringWhitespace(
            source,
            QStringLiteral("if (status === Image.Ready) { "
                           "window.pageBaseScale = window.fitPageScale() }")) ||
        sourceContainsIgnoringWhitespace(
            source, QStringLiteral("if (status === Image.Ready) "
                                   "window.pageBaseScale = "
                                   "window.fitPageScale()")));

    const QString viewportSource =
        readQmlFile(QStringLiteral("ViewportMetrics.qml"));
    const qsizetype pageScaleStart = indexOfIgnoringWhitespace(
        viewportSource,
        QStringLiteral("function pageScale() { return pageBaseScale }"));
    const qsizetype pageScaleEnd = viewportSource.indexOf(
        QStringLiteral("function pageDisplayWidth()"), pageScaleStart);
    QVERIFY(pageScaleStart >= 0);
    QVERIFY(pageScaleEnd > pageScaleStart);
    const QString pageScaleSource =
        viewportSource.mid(pageScaleStart, pageScaleEnd - pageScaleStart);
    QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.width")));
    QVERIFY(!pageScaleSource.contains(QStringLiteral("canvas.height")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        viewportSource, QStringLiteral("function pageDisplayWidth() { return "
                                       "pageSourceWidth * pageScale() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        viewportSource, QStringLiteral("function pageDisplayHeight() { return "
                                       "pageSourceHeight * pageScale() }")));
  }

  void qmlRightPanelUsesSectionTabsAndNoHorizontalOverflow() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString source = qmlSource();
    const QString rightPanelSource =
        readQmlFile(QStringLiteral("RightInspectorPanel.qml"));
    const qsizetype rightPanelStart =
        source.indexOf(QStringLiteral("id: rightPanel"));
    QVERIFY(rightPanelStart >= 0);
    QVERIFY(mainSource.contains(QStringLiteral("RightInspectorPanel {")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource,
        QStringLiteral(
            "selectedBoxProvider: () => { return window.selectedBox() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource,
        QStringLiteral(
            "qmlColorProvider: (hex) => { return window.qmlColor(hex) }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource,
        QStringLiteral("onColorDialogRequested: (hex, setter) => { "
                       "return window.openColorDialog(hex, setter) }")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("Label { text: qsTr(\"Navigation\")")));
    QVERIFY(!mainSource.contains(
        QStringLiteral("Label { text: qsTr(\"Text Effects\")")));

    QVERIFY(rightPanelSource.contains(QStringLiteral("id: rightPanelScroll")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("contentWidth: availableWidth")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("width: rightPanelScroll.availableWidth")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("property real displayScale: 1")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("property real minimumDisplayScale: 0.5")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("property real maximumDisplayScale: 6")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("readonly property real displayScaleSnapThreshold: "
                       "0.1")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("readonly property string maximumDisplayScaleLabel: "
                       "displayScaleLabel(maximumDisplayScale)")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("signal zoomRequested(real displayScale)")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("objectName: \"rightInspectorZoomLabel\"")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("objectName: \"rightInspectorZoomSlider\"")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("color: rightInspectorPanel.palette.mid")));
    QVERIFY(rightPanelSource.contains(
        QStringLiteral("color: rightInspectorPanel.palette.text")));
    QVERIFY(!rightPanelSource.contains(QStringLiteral(
        "color: Qt.alpha(rightInspectorPanel.palette.text, 0.75)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("function displayScaleLabel(displayScale) { return "
                       "Number(displayScale * 100).toLocaleString(Qt.locale(), "
                       "\"f\", 1) + \"%\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("text: rightInspectorPanel.displayScaleLabel("
                       "rightInspectorPanel.displayScale)")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("Layout.preferredWidth: "
                       "zoomLabelMetrics.advanceWidth("
                       "rightInspectorPanel.maximumDisplayScaleLabel)")));
    QVERIFY(!rightPanelSource.contains(QStringLiteral(
        "zoomLabelMetrics.advanceWidth(\"600.0%\")")));
    QVERIFY(rightPanelSource.contains(QStringLiteral("FontMetrics {")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("function detentedDisplayScale(displayScale) { return "
                        "Math.abs(displayScale - 1) <= "
                       "displayScaleSnapThreshold + 1e-9 ? 1 : displayScale }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("from: rightInspectorPanel.minimumDisplayScale; to: "
                       "rightInspectorPanel.maximumDisplayScale; value: "
                       "rightInspectorPanel.displayScale; live: true")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("onMoved: { rightInspectorPanel.zoomRequested("
                       "rightInspectorPanel.detentedDisplayScale(value)) }")));
    QVERIFY(!rightPanelSource.contains(QStringLiteral("dragDisplayScale")));
    QVERIFY(!rightPanelSource.contains(QStringLiteral("onPressedChanged")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource, QStringLiteral("function setDisplayScaleAtCenter(displayScale) "
                                   "{ if (displayScale <= 0 || window.pageBaseScale "
                                   "<= 0) return; window.setZoomAtCenter(displayScale "
                                   "/ window.pageBaseScale) }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource, QStringLiteral("displayScale: viewportMetrics.viewDocScale(); "
                                   "minimumDisplayScale: window.pageBaseScale * "
                                   "viewportMetrics.minimumZoom; maximumDisplayScale: "
                                   "window.pageBaseScale * "
                                   "viewportMetrics.maximumZoom")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        mainSource, QStringLiteral("onZoomRequested: (displayScale) => { return "
                                   "window.setDisplayScaleAtCenter(displayScale) "
                                   "}")));
    QVERIFY(!rightPanelSource.contains(
        QStringLiteral("width: Math.max(1, rightPanel.availableWidth)")));

    for (const QString &sectionId : {QStringLiteral("navigationSection"),
                                     QStringLiteral("boxEffectsSection"),
                                     QStringLiteral("textEffectsSection"),
                                     QStringLiteral("layersSection")}) {
      QVERIFY2(rightPanelSource.contains(QStringLiteral("id: ") + sectionId),
               qPrintable(sectionId));
    }
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("Label { text: qsTr(\"Navigation\"); font.bold: true; "
                       "enabled: navigationSection.sectionReady }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("Label { text: qsTr(\"Box Effects\"); font.bold: true; "
                       "enabled: boxEffectsSection.sectionReady }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("Label { text: qsTr(\"Text Effects\"); font.bold: true; "
                       "enabled: textEffectsSection.sectionReady }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("Label { text: qsTr(\"Layers\"); font.bold: true; "
                       "enabled: layersSection.sectionReady }")));
    QVERIFY(rightPanelSource.count(QStringLiteral("GroupBox {")) >= 4);
    QVERIFY(rightPanelSource.count(QStringLiteral("Layout.minimumWidth: 0")) >=
            20);
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("contentItem: Label { text: pageSelect.displayText")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        rightPanelSource,
        QStringLiteral("contentItem: Label { text: layerDelegate.text")));
    QVERIFY(
        rightPanelSource.contains(QStringLiteral("elide: Text.ElideRight")));

    const qsizetype zoomLabelStart = rightPanelSource.indexOf(
        QStringLiteral("objectName: \"rightInspectorZoomLabel\""));
    const qsizetype pageNameStart = rightPanelSource.indexOf(
        QStringLiteral("rightInspectorPanel.editor.currentPageName"),
        zoomLabelStart);
    QVERIFY(zoomLabelStart >= 0);
    QVERIFY(pageNameStart > zoomLabelStart);

    const qsizetype boxStart =
        rightPanelSource.indexOf(QStringLiteral("id: boxEffectsSection"));
    const qsizetype textStart = rightPanelSource.indexOf(
        QStringLiteral("id: textEffectsSection"), boxStart);
    QVERIFY(boxStart >= 0);
    QVERIFY(textStart > boxStart);
    const QString boxEffectsSource =
        rightPanelSource.mid(boxStart, textStart - boxStart);
    QVERIFY(boxEffectsSource.contains(QStringLiteral("id: boxEffectsTabs")));
    QVERIFY(boxEffectsSource.contains(QStringLiteral("StackLayout")));
    QVERIFY(!boxEffectsSource.contains(QStringLiteral("TabButton { width:")));
    QVERIFY(!boxEffectsSource.contains(QStringLiteral("boxEffectsTabs.width")));
    QVERIFY(!boxEffectsSource.contains(QStringLiteral("parent.width")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        boxEffectsSource,
        QStringLiteral("TabButton { text: qsTr(\"Rotation\") }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        boxEffectsSource,
        QStringLiteral("TabButton { text: qsTr(\"Perspective\") }")));
    QVERIFY(boxEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedRotation(value)")));
    QVERIFY(boxEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedPerspectiveEnabled(checked)")));
    QVERIFY(boxEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.resetSelectedPerspective()")));

    const qsizetype layersStart = rightPanelSource.indexOf(
        QStringLiteral("id: layersSection"), textStart);
    QVERIFY(layersStart > textStart);
    const QString textEffectsSource =
        rightPanelSource.mid(textStart, layersStart - textStart);
    QVERIFY(textEffectsSource.contains(QStringLiteral("id: textEffectsTabs")));
    QVERIFY(textEffectsSource.contains(QStringLiteral("StackLayout")));
    QVERIFY(!textEffectsSource.contains(QStringLiteral("TabButton { width:")));
    QVERIFY(
        !textEffectsSource.contains(QStringLiteral("textEffectsTabs.width")));
    QVERIFY(!textEffectsSource.contains(QStringLiteral("parent.width")));
    for (const QString &feature :
         {QStringLiteral("Outline"), QStringLiteral("Blur"),
          QStringLiteral("Shadow"), QStringLiteral("Gradient"),
          QStringLiteral("Path")}) {
      QVERIFY2(
          sourceContainsIgnoringWhitespace(
              textEffectsSource, QStringLiteral("TabButton { text: qsTr(\"") +
                                     feature + QStringLiteral("\") }")),
          qPrintable(feature));
    }
    QVERIFY(textEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedOutlineEnabled(checked)")));
    QVERIFY(textEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedBlurEnabled(checked)")));
    QVERIFY(textEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedShadowEnabled(checked)")));
    QVERIFY(textEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedGradientEnabled(checked)")));
    QVERIFY(textEffectsSource.contains(QStringLiteral(
        "rightInspectorPanel.editor.setSelectedPathEnabled(checked)")));
  }

  void qmlMenusOwnPrimaryControlsAndToolbarIsRemoved() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = readQmlFile(QStringLiteral("Main.qml"));
    const QString editorChromeSource =
        readQmlFile(QStringLiteral("EditorChrome.qml"));
    const QString source = qmlSource();
    const QString menuItemSource =
        readQmlFile(QStringLiteral("ShortcutMenuItem.qml"));
    const qsizetype menuItemStart =
        menuItemSource.indexOf(QStringLiteral("MenuItem {"));
    const qsizetype menuItemEnd = menuItemSource.size();
    QVERIFY(menuItemStart >= 0);
    QVERIFY(menuItemEnd > menuItemStart);
    const QString menuItemDefinition =
        menuItemSource.mid(menuItemStart, menuItemEnd - menuItemStart);

    QVERIFY(!mainSource.contains(
        QStringLiteral("component ShortcutMenuItem: MenuItem")));
    QVERIFY(menuItemDefinition.contains(QStringLiteral("MenuItem {")));
    QVERIFY(menuItemDefinition.contains(QStringLiteral("contentItem: Item")));
    QVERIFY(menuItemDefinition.contains(QStringLiteral("RowLayout {")));
    QVERIFY(
        menuItemDefinition.contains(QStringLiteral("Layout.fillWidth: true")));
    QVERIFY(menuItemDefinition.contains(
        QStringLiteral("shortcutMenuItem.shortcutLabel")));
    QVERIFY(!menuItemSource.contains(QStringLiteral("indicator:")));
    QVERIFY(
        !menuItemSource.contains(QStringLiteral("shortcutMenuItem.checked")));
    QVERIFY(!menuItemSource.contains(QStringLiteral("✓")));
    QVERIFY(
        !menuItemSource.contains(QStringLiteral("Layout.preferredWidth: 16")));
    QVERIFY(!source.contains(QStringLiteral(
        "Layout.preferredWidth: shortcutMenuItem.checkable ? 16 : 0")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: openAction; text: qsTr(\"Open\"); "
                       "shortcut: StandardKey.Open")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: saveAction; text: qsTr(\"Save\"); "
                       "shortcut: StandardKey.Save")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: saveAllAction; text: qsTr(\"Save All\"); "
                       "shortcut: \"Ctrl+Shift+S\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: quitAction; text: qsTr(\"Quit\"); "
                       "onTriggered: Qt.quit() }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: copyAction; text: qsTr(\"Copy\"); "
                       "shortcut: StandardKey.Copy")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: pasteAction; text: qsTr(\"Paste\"); "
                       "shortcut: StandardKey.Paste")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: deleteAction; text: qsTr(\"Delete\"); "
                       "shortcut: StandardKey.Delete")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: duplicateAction; text: "
                       "qsTr(\"Duplicate\"); enabled:")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: zoomInAction; text: qsTr(\"Zoom In\"); "
                       "shortcut: \"Ctrl++\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: zoomOutAction; text: qsTr(\"Zoom Out\"); "
                       "shortcut: \"Ctrl+-\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("Action { id: resetZoomAction; text: qsTr(\"Reset "
                       "Zoom\"); shortcut: \"Ctrl+0\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral(
            "Action { id: rawOverlayAction; text: qsTr(\"Show Raw Overlay\"); "
            "checkable: true; checked: editorChrome.editor ? "
            "editorChrome.editor.rawVisible : false; shortcut: \"Ctrl+H\"")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: openAction; shortcutLabel: "
                       "\"Ctrl+O\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: saveAction; shortcutLabel: "
                       "\"Ctrl+S\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: saveAllAction; "
                       "shortcutLabel: \"Ctrl+Shift+S\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: quitAction }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: copyAction; shortcutLabel: "
                       "\"Ctrl+C\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: pasteAction; shortcutLabel: "
                       "\"Ctrl+V\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: deleteAction; "
                       "shortcutLabel: \"Del\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: zoomInAction; "
                       "shortcutLabel: \"Ctrl++\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: zoomOutAction; "
                       "shortcutLabel: \"Ctrl+-\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: resetZoomAction; "
                       "shortcutLabel: \"Ctrl+0\" }")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        editorChromeSource,
        QStringLiteral("ShortcutMenuItem { action: rawOverlayAction; "
                       "shortcutLabel: \"Ctrl+H\" }")));

    QVERIFY(mainSource.contains(QStringLiteral("menuBar: chrome.menuBar")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("property alias menuBar: chromeMenuBar")));
    QVERIFY(mainSource.contains(QStringLiteral("EditorChrome {")));
    QVERIFY(!mainSource.contains(QStringLiteral("Action { id: openAction")));
    QVERIFY(!mainSource.contains(QStringLiteral("FolderDialog")));
    QVERIFY(!mainSource.contains(QStringLiteral("ColorDialog {")));
    QVERIFY(!source.contains(QStringLiteral("menuBarVisible")));
    QVERIFY(!source.contains(QStringLiteral("menuBarAction")));
    QVERIFY(!source.contains(QStringLiteral("Hide Menu Bar")));
    QVERIFY(!source.contains(QStringLiteral("Show Menu Bar")));
    QVERIFY(!source.contains(QStringLiteral("Shortcut { sequence: \"Alt\"")));
    QVERIFY(!source.contains(QStringLiteral("ToolBar")));
    QVERIFY(!source.contains(QStringLiteral("ToolButton")));
    QVERIFY(!source.contains(QStringLiteral("Show Top Bar")));
    QVERIFY(!source.contains(QStringLiteral("Hide Top Bar")));
    QVERIFY(!source.contains(QStringLiteral("shortcut: \"Ctrl+D\"")));
    QVERIFY(!source.contains(QStringLiteral("StandardKey.Quit")));
    QVERIFY(!source.contains(QStringLiteral("Ctrl+Q")));
    QVERIFY(!source.contains(QStringLiteral("Ctrl+Alt+M")));
    QVERIFY(!menuItemSource.contains(QStringLiteral("CheckBox")));
    QVERIFY(!menuItemSource.contains(QStringLiteral("CheckIndicator")));
    QVERIFY(!source.contains(QStringLiteral("Duplicate\"), \"Ctrl+")));
    QVERIFY(!source.contains(QStringLiteral(
        "ShortcutMenuItem { action: duplicateAction; shortcutLabel:")));
    QVERIFY(!source.contains(QStringLiteral("qsTr(\"Open Ctrl+")));
    QVERIFY(!source.contains(QStringLiteral("qsTr(\"Save Ctrl+")));
    QVERIFY(!source.contains(QStringLiteral("qsTr(\"Save All Ctrl+")));
    QVERIFY(!source.contains(QStringLiteral("qsTr(\"Open Project…\")")));
    QVERIFY(!source.contains(QStringLiteral("qsTr(\"Export\")")));
    QVERIFY(!source.contains(QStringLiteral("Editor.newDocument()")));
    QVERIFY(!source.contains(QStringLiteral("Editor.exportPng()")));
    QVERIFY(!source.contains(QStringLiteral("localPathFromUrl")));
    QVERIFY(editorChromeSource.contains(
        QStringLiteral("editorChrome.editor.openProjectUrl(selectedFolder)")));
  }

  void qmlMainWiresEditorChromeSignalsAndBindings() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);

    auto *chrome = window->findChild<QObject *>(QStringLiteral("editorChrome"));
    QVERIFY(chrome);
    QCOMPARE(chrome->property("editor").value<QObject *>(), &editor);
    QCOMPARE(chrome->property("hostWidth").toReal(), window->width());
    QCOMPARE(chrome->property("hostHeight").toReal(), window->height());
    QVERIFY(chrome->property("zoomCenterX").toReal() > 0.0);
    QVERIFY(chrome->property("zoomCenterY").toReal() >= 0.0);

    QVERIFY(window->setProperty("zoom", 2.0));
    QVERIFY(QMetaObject::invokeMethod(chrome, "resetZoomRequested"));
    QTRY_COMPARE(window->property("zoom").toReal(), 1.0);

    QVERIFY(QMetaObject::invokeMethod(
        chrome, "zoomAtRequested", Q_ARG(double, 320.0), Q_ARG(double, 240.0),
        Q_ARG(double, 1.1)));
    QTRY_COMPARE(window->property("zoom").toReal(), 1.1);

    QCOMPARE(editor.selectedIndex(), 0);
    QVERIFY(QMetaObject::invokeMethod(chrome, "escapeRequested"));
    QTRY_COMPARE(editor.selectedIndex(), -1);
  }

  void qmlMainWiresLeftInspectorPanelApi() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedFontFamily(QStringLiteral("TextFX Inspector Test"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);

    auto *leftInspector =
        window->findChild<QObject *>(QStringLiteral("leftInspectorPanel"));
    QVERIFY(leftInspector);
    QCOMPARE(leftInspector->property("editor").value<QObject *>(), &editor);
    QVERIFY(leftInspector->property("editorLimits").value<QObject *>());

    QVariant selected;
    QVERIFY(QMetaObject::invokeMethod(leftInspector, "selectedBox",
                                      Q_RETURN_ARG(QVariant, selected)));
    QCOMPARE(selected.toMap().value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("TextFX Inspector Test"));

    QCOMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(leftInspector, "textInputFocusChanged",
                                      Q_ARG(bool, true)));
    QTRY_COMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(leftInspector, "textInputFocusChanged",
                                      Q_ARG(bool, false)));
    QTRY_COMPARE(window->property("sidePanelFocusedTextInputs").toInt(), 0);
  }

  void qmlLeftInspectorTextPropertyControlUpdatesSelectedBox() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedBold(false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(
        object = findVisualChildByName(
            window->contentItem(), QStringLiteral("leftInspectorBoldButton")));
    auto *button = qobject_cast<QQuickItem *>(object);
    QVERIFY(button);
    QTRY_VERIFY(button->isVisible());
    QVERIFY(button->isEnabled());

    const QPoint center =
        button->mapToScene(QPointF(button->width() / 2, button->height() / 2))
            .toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

    QTRY_VERIFY(
        editor.boxes().at(0).toMap().value(QStringLiteral("bold")).toBool());
  }

  void qmlLeftInspectorPageTextDelegateAppliesThroughEditor() {
    registerQmlTypes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page1.png]\nPanel page text\n");
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());
    QCOMPARE(editor.pageTexts(),
             QStringList({QStringLiteral("Panel page text")}));
    editor.createTextBox(10, 20, 100, 50);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("leftInspectorPageTextDelegate")));
    auto *delegate = qobject_cast<QQuickItem *>(object);
    QVERIFY(delegate);
    QTRY_VERIFY(delegate->isVisible());
    QVERIFY(delegate->isEnabled());

    const QPoint center =
        delegate
            ->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2))
            .toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

    QTRY_COMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Panel page text"));
  }

  void qmlLeftInspectorColorButtonRoutesThroughEditorChrome() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedTextColor(QStringLiteral("#112233"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *chrome = window->findChild<QObject *>(QStringLiteral("editorChrome"));
    QVERIFY(chrome);
    QCOMPARE(chrome->property("colorDialogSetter").toString(), QString());

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("leftInspectorTextColorButton")));
    auto *button = qobject_cast<QQuickItem *>(object);
    QVERIFY(button);
    QTRY_VERIFY(button->isVisible());
    QVERIFY(button->isEnabled());

    const QPoint center =
        button->mapToScene(QPointF(button->width() / 2, button->height() / 2))
            .toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

    QTRY_COMPARE(chrome->property("colorDialogSetter").toString(),
                 QStringLiteral("text"));
  }

  void qmlMainWiresRightInspectorPanelApi() {
    registerQmlTypes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedOutlineColor(QStringLiteral("#112233"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *rightInspector =
        window->findChild<QObject *>(QStringLiteral("rightInspectorPanel"));
    QVERIFY(rightInspector);
    QCOMPARE(rightInspector->property("editor").value<QObject *>(), &editor);
    QVERIFY(rightInspector->property("editorLimits").value<QObject *>());
    QCOMPARE(rightInspector->property("displayScale").toDouble(), 1.0);
    QCOMPARE(rightInspector->property("minimumDisplayScale").toDouble(), 0.5);
    QCOMPARE(rightInspector->property("maximumDisplayScale").toDouble(), 6.0);

    QVariant selected;
    QVERIFY(QMetaObject::invokeMethod(rightInspector, "selectedBox",
                                      Q_RETURN_ARG(QVariant, selected)));
    QCOMPARE(selected.toMap().value(QStringLiteral("outlineColor")).toString(),
             QStringLiteral("112233ff"));

    QVERIFY(window->setProperty("pageBaseScale", 0.25));
    QVERIFY(window->setProperty("zoom", 2.0));
    QTRY_COMPARE(rightInspector->property("displayScale").toDouble(), 0.5);
    QCOMPARE(rightInspector->property("minimumDisplayScale").toDouble(), 0.125);
    QCOMPARE(rightInspector->property("maximumDisplayScale").toDouble(), 1.5);

    QObject *zoomLabelObject = nullptr;
    QTRY_VERIFY(zoomLabelObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorZoomLabel")));
    auto *zoomLabel = qobject_cast<QQuickItem *>(zoomLabelObject);
    QVERIFY(zoomLabel);
    QTRY_VERIFY(zoomLabel->isVisible());
    QCOMPARE(zoomLabel->property("text").toString(), QStringLiteral("50.0%"));
    const double singleDigitLabelWidth = zoomLabel->width();
    QVERIFY(singleDigitLabelWidth > 0.0);

    QVERIFY(window->setProperty("zoom", 4.1));
    QTRY_VERIFY(std::abs(rightInspector->property("displayScale").toDouble() -
                         1.025) < 0.0001);
    QCOMPARE(zoomLabel->property("text").toString(), QStringLiteral("102.5%"));
    QVERIFY(!zoomLabel->property("text").toString().startsWith(
        QLatin1Char('0')));
    QCOMPARE(zoomLabel->width(), singleDigitLabelWidth);

    QObject *sliderObject = nullptr;
    QTRY_VERIFY(sliderObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorZoomSlider")));
    auto *slider = qobject_cast<QQuickItem *>(sliderObject);
    QVERIFY(slider);
    QTRY_VERIFY(slider->isVisible());
    QVERIFY(slider->isEnabled());
    QVERIFY(std::abs(slider->property("value").toDouble() - 1.025) < 0.0001);

    QVariant snapped;
    QVERIFY(QMetaObject::invokeMethod(
        rightInspector, "detentedDisplayScale", Q_RETURN_ARG(QVariant, snapped),
        Q_ARG(QVariant, 0.9)));
    QCOMPARE(snapped.toDouble(), 1.0);
    QVERIFY(QMetaObject::invokeMethod(
        rightInspector, "detentedDisplayScale", Q_RETURN_ARG(QVariant, snapped),
        Q_ARG(QVariant, 1.1)));
    QCOMPARE(snapped.toDouble(), 1.0);
    QVERIFY(QMetaObject::invokeMethod(
        rightInspector, "detentedDisplayScale", Q_RETURN_ARG(QVariant, snapped),
        Q_ARG(QVariant, 1.05)));
    QCOMPARE(snapped.toDouble(), 1.0);
    QVERIFY(QMetaObject::invokeMethod(
        rightInspector, "detentedDisplayScale", Q_RETURN_ARG(QVariant, snapped),
        Q_ARG(QVariant, 0.899)));
    QCOMPARE(snapped.toDouble(), 0.899);
    QVERIFY(QMetaObject::invokeMethod(
        rightInspector, "detentedDisplayScale", Q_RETURN_ARG(QVariant, snapped),
        Q_ARG(QVariant, 1.101)));
    QCOMPARE(snapped.toDouble(), 1.101);

    QVERIFY(window->setProperty("pageBaseScale", 1.25));
    QVERIFY(window->setProperty("zoom", 1.0));
    QTRY_VERIFY(std::abs(rightInspector->property("displayScale").toDouble() -
                         1.25) < 0.0001);
    QCOMPARE(rightInspector->property("maximumDisplayScale").toDouble(), 7.5);
    QCOMPARE(zoomLabel->property("text").toString(), QStringLiteral("125.0%"));
    const double aboveSixHundredReservedWidth = zoomLabel->width();
    QVERIFY(aboveSixHundredReservedWidth >= singleDigitLabelWidth);

    QVERIFY(window->setProperty("zoom", 5.8));
    QTRY_VERIFY(std::abs(rightInspector->property("displayScale").toDouble() -
                         7.25) < 0.0001);
    QCOMPARE(zoomLabel->property("text").toString(), QStringLiteral("725.0%"));
    QVERIFY(!zoomLabel->property("text").toString().startsWith(
        QLatin1Char('0')));
    QCOMPARE(zoomLabel->width(), aboveSixHundredReservedWidth);
  }

  void qmlRightInspectorNavigationAndEffectsRouteThroughEditor() {
    registerQmlTypes();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedOutlineEnabled(false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorNextPageButton")));
    auto *nextButton = qobject_cast<QQuickItem *>(object);
    QVERIFY(nextButton);
    QTRY_VERIFY(nextButton->isVisible());
    QVERIFY(nextButton->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      nextButton
                          ->mapToScene(QPointF(nextButton->width() / 2,
                                               nextButton->height() / 2))
                          .toPoint());
    QTRY_COMPARE(editor.currentPageIndex(), 1);

    editor.previousPage();
    QTRY_COMPARE(editor.currentPageIndex(), 0);
    editor.selectBox(0);
    QTRY_COMPARE(editor.selectedIndex(), 0);

    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorRawVisibleCheckBox")));
    auto *rawCheckBox = qobject_cast<QQuickItem *>(object);
    QVERIFY(rawCheckBox);
    QTRY_VERIFY(rawCheckBox->isVisible());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      rawCheckBox
                          ->mapToScene(QPointF(rawCheckBox->width() / 2,
                                               rawCheckBox->height() / 2))
                          .toPoint());
    QTRY_VERIFY(editor.rawVisible());

    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorOutlineEnabledCheckBox")));
    auto *outlineCheckBox = qobject_cast<QQuickItem *>(object);
    QVERIFY(outlineCheckBox);
    QTRY_VERIFY(outlineCheckBox->isVisible());
    QVERIFY(outlineCheckBox->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      outlineCheckBox
                          ->mapToScene(QPointF(outlineCheckBox->width() / 2,
                                               outlineCheckBox->height() / 2))
                          .toPoint());
    QTRY_VERIFY(
        editor.boxes().at(0).toMap().value(QStringLiteral("outline")).toBool());
  }

  void qmlRightInspectorColorButtonRoutesThroughEditorChrome() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.setSelectedOutlineColor(QStringLiteral("#112233"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto *chrome = window->findChild<QObject *>(QStringLiteral("editorChrome"));
    QVERIFY(chrome);
    QCOMPARE(chrome->property("colorDialogSetter").toString(), QString());

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorOutlineColorButton")));
    auto *button = qobject_cast<QQuickItem *>(object);
    QVERIFY(button);
    QTRY_VERIFY(button->isVisible());
    QVERIFY(button->isEnabled());

    const QPoint center =
        button->mapToScene(QPointF(button->width() / 2, button->height() / 2))
            .toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);

    QTRY_COMPARE(chrome->property("colorDialogSetter").toString(),
                 QStringLiteral("outline"));
  }

  void qmlRightInspectorPathAndPerspectiveControlsRouteThroughEditor() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.setPerspectiveHandle(QStringLiteral("nw"), 12.0, 8.0);
    editor.setSelectedPathEnabled(false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1000);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    const auto click = [&](const QString &objectName) {
      QObject *object = nullptr;
      QTRY_VERIFY(object =
                      findVisualChildByName(window->contentItem(), objectName));
      auto *item = qobject_cast<QQuickItem *>(object);
      QVERIFY(item);
      QTRY_VERIFY(item->isVisible());
      QVERIFY(item->isEnabled());
      QTest::mouseClick(
          window, Qt::LeftButton, Qt::NoModifier,
          item->mapToScene(QPointF(item->width() / 2, item->height() / 2))
              .toPoint());
    };

    QObject *tabsObject = nullptr;
    QTRY_VERIFY(tabsObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorTextEffectsTabs")));
    QVERIFY(tabsObject->setProperty("currentIndex", 4));
    QCoreApplication::processEvents();

    click(QStringLiteral("rightInspectorPathEnabledCheckBox"));
    QTRY_VERIFY(
        editor.boxes().at(0).toMap().value(QStringLiteral("path")).toBool());
    const qsizetype pathPointCount = editor.boxes()
                                         .at(0)
                                         .toMap()
                                         .value(QStringLiteral("pathPoints"))
                                         .toList()
                                         .size();

    click(QStringLiteral("rightInspectorAddPathPointButton"));
    QTRY_VERIFY(editor.boxes()
                    .at(0)
                    .toMap()
                    .value(QStringLiteral("pathPoints"))
                    .toList()
                    .size() > pathPointCount);

    QTRY_VERIFY(tabsObject = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorBoxEffectsTabs")));
    QVERIFY(tabsObject->setProperty("currentIndex", 1));
    QCoreApplication::processEvents();

    click(QStringLiteral("rightInspectorResetPerspectiveButton"));
    const QVariantList resetNw = editor.boxes()
                                     .at(0)
                                     .toMap()
                                     .value(QStringLiteral("perspectiveNw"))
                                     .toList();
    QCOMPARE(resetNw.at(0).toDouble(), 0.0);
    QCOMPARE(resetNw.at(1).toDouble(), 0.0);
  }

  void qmlRightInspectorLayersListSelectsAndMovesThroughEditor() {
    registerQmlTypes();

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Bottom"));
    editor.createTextBox(30, 40, 100, 50);
    editor.updateSelectedText(QStringLiteral("Middle"));
    editor.createTextBox(50, 60, 100, 50);
    editor.updateSelectedText(QStringLiteral("Top"));
    editor.selectBox(0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
    engine.load(QUrl::fromLocalFile(
        QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window =
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    window->resize(1400, 1000);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *object = nullptr;
    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorLayersListView")));
    auto *listView = qobject_cast<QQuickItem *>(object);
    QVERIFY(listView);
    QTRY_VERIFY(listView->isVisible());
    QVERIFY(listView->isEnabled());

    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorLayerDelegate")));
    auto *delegate = qobject_cast<QQuickItem *>(object);
    QVERIFY(delegate);
    QTRY_VERIFY(delegate->isVisible());
    QVERIFY(delegate->isEnabled());

    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier,
        delegate
            ->mapToScene(QPointF(delegate->width() / 2, delegate->height() / 2))
            .toPoint());
    QTRY_COMPARE(editor.selectedIndex(), 2);

    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorLayerDownButton")));
    auto *downButton = qobject_cast<QQuickItem *>(object);
    QVERIFY(downButton);
    QTRY_VERIFY(downButton->isVisible());
    QVERIFY(downButton->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      downButton
                          ->mapToScene(QPointF(downButton->width() / 2,
                                               downButton->height() / 2))
                          .toPoint());
    QTRY_COMPARE(editor.selectedIndex(), 1);
    QCOMPARE(
        editor.boxes().at(1).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Top"));

    QTRY_VERIFY(object = findVisualChildByName(
                    window->contentItem(),
                    QStringLiteral("rightInspectorLayerUpButton")));
    auto *upButton = qobject_cast<QQuickItem *>(object);
    QVERIFY(upButton);
    QTRY_VERIFY(upButton->isVisible());
    QVERIFY(upButton->isEnabled());
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier,
        upButton
            ->mapToScene(QPointF(upButton->width() / 2, upButton->height() / 2))
            .toPoint());
    QTRY_COMPARE(editor.selectedIndex(), 2);
    QCOMPARE(
        editor.boxes().at(2).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Top"));
  }

  void qmlHasPageSelectorAndTextFXEffectControls() {
    QFile qml(QStringLiteral(TEXTFX_FIXTURE_DIR "/../../qml/Main.qml"));
    QVERIFY(qml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = qmlSource();

    QVERIFY(source.contains(
        QStringLiteral("model: rightInspectorPanel.editor.pageLabels")));
    QVERIFY(source.contains(
        QStringLiteral("rightInspectorPanel.editor.goToPage(index)")));
    QVERIFY(source.contains(
        QStringLiteral("model: [qsTr(\"Vertical\"), qsTr(\"Horizontal\")]")));
    QVERIFY(source.contains(
        QStringLiteral("model: [qsTr(\"Straight\"), qsTr(\"Smooth\")]")));
    QVERIFY(source.contains(
        QStringLiteral("rightInspectorPanel.editor.addSelectedPathPoint()")));
    QVERIFY(source.contains(QStringLiteral("PathHandleInteractionState")));
    QVERIFY(source.contains(
        QStringLiteral("objectName: \"pathHandleInteractionState\"")));
    QVERIFY(source.contains(
        QStringLiteral("pathHandleInteraction.update(canvas, canvasX, canvasY, "
                       "window.editor.leftMouseButtonDown())")));
    QVERIFY(source.contains(
        QStringLiteral("model: pathHandlePlane.boxRef.boxModel.pathPoints")));
    QVERIFY(
        !source.contains(QStringLiteral("Double-click canvas to create text")));
    QVERIFY(!source.contains(QStringLiteral(
        "Live editing uses QML layout; rendered effects apply on export.")));
  }
};

QTEST_MAIN(QmlShellSmokeTests)

#include "qml_shell_smoke_tests.moc"
