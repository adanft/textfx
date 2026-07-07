#include "app/EditorController.h"
#include "app/BoxesModel.h"
#include "app/EditorViewModels.h"
#include "app/SelectionQueryService.h"
#include "app/BoxRenderState.h"
#include "application/queries/CommandAvailability.h"
#include "app/EffectMetadata.h"
#include "domain/AuthoringLimits.h"
#include "infrastructure/fonts/FontResolver.h"
#include "qt_test_helpers.h"

#include <QAbstractItemModel>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <filesystem>

using namespace textfx;
using namespace textfx::test;

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace textfx::test_hooks {
void failPageTextsOpen(std::filesystem::path path);
void clearPageTextsOpenFailure();
void failProjectFileRead(std::filesystem::path path);
void clearProjectFileReadFailure();
} // namespace textfx::test_hooks
#endif

namespace {
struct ProjectFileReadFailureGuard {
  explicit ProjectFileReadFailureGuard(const QString &path) {
    textfx::test_hooks::failProjectFileRead(path.toStdString());
  }

  ~ProjectFileReadFailureGuard() {
    textfx::test_hooks::clearProjectFileReadFailure();
  }
};

struct PageTextsOpenFailureGuard {
  explicit PageTextsOpenFailureGuard(const QString &path) {
    textfx::test_hooks::failPageTextsOpen(path.toStdString());
  }

  ~PageTextsOpenFailureGuard() {
    textfx::test_hooks::clearPageTextsOpenFailure();
  }
};

void writeJsonFile(const QString &path, const QJsonObject &object) {
  QFileInfo info(path);
  QDir().mkpath(info.absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write(QJsonDocument(object).toJson()),
           QJsonDocument(object).toJson().size());
}

int roleForName(const QAbstractItemModel &model, QByteArrayView name) {
  const auto roles = model.roleNames();
  for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
    if (it.value() == name)
      return it.key();
  }
  return -1;
}

void compareBoxModelRolesToRenderState(const BoxesModel &model, int row,
                                       const BoxRenderState &state) {
  const QModelIndex index = model.index(row, 0);
  QVERIFY(index.isValid());

  QCOMPARE(model.data(index, roleForName(model, "boxIndex")).toInt(),
           state.index);
  QCOMPARE(model.data(index, roleForName(model, "boxText")).toString(),
           state.text);
  QCOMPARE(model.data(index, roleForName(model, "boxX")).toDouble(), state.x);
  QCOMPARE(model.data(index, roleForName(model, "boxY")).toDouble(), state.y);
  QCOMPARE(model.data(index, roleForName(model, "boxWidth")).toDouble(),
           state.width);
  QCOMPARE(model.data(index, roleForName(model, "boxHeight")).toDouble(),
           state.height);
  QCOMPARE(model.data(index, roleForName(model, "boxRotation")).toDouble(),
           state.rotation);
  QCOMPARE(model.data(index, roleForName(model, "boxFontFamily")).toString(),
           state.fontFamily);
  QCOMPARE(model.data(index, roleForName(model, "boxResolvedFontFamily")).toString(),
           state.resolvedFontFamily);
  QCOMPARE(model.data(index, roleForName(model, "boxFontSize")).toInt(),
           state.fontSize);
  QCOMPARE(model.data(index, roleForName(model, "boxColor")).toString(),
           state.color);
  QCOMPARE(model.data(index, roleForName(model, "boxLineSpacing")).toInt(),
           state.lineSpacing);
  QCOMPARE(model.data(index, roleForName(model, "boxLetterSpacing")).toInt(),
           state.letterSpacing);
  QCOMPARE(model.data(index, roleForName(model, "boxBold")).toBool(), state.bold);
  QCOMPARE(model.data(index, roleForName(model, "boxItalic")).toBool(),
           state.italic);
  QCOMPARE(model.data(index, roleForName(model, "boxUppercase")).toBool(),
           state.uppercase);
  QCOMPARE(model.data(index, roleForName(model, "boxLowercase")).toBool(),
           state.lowercase);
  QCOMPARE(model.data(index, roleForName(model, "boxAlignment")).toInt(),
           state.alignment);
  QCOMPARE(model.data(index, roleForName(model, "boxOutline")).toBool(),
           state.outline);
  QCOMPARE(model.data(index, roleForName(model, "boxOutlineColor")).toString(),
           state.outlineColor);
  QCOMPARE(model.data(index, roleForName(model, "boxOutlineSize")).toInt(),
           state.outlineSize);
  QCOMPARE(model.data(index, roleForName(model, "boxBlur")).toBool(), state.blur);
  QCOMPARE(model.data(index, roleForName(model, "boxBlurSize")).toInt(),
           state.blurSize);
  QCOMPARE(model.data(index, roleForName(model, "boxShadow")).toBool(),
           state.shadow);
  QCOMPARE(model.data(index, roleForName(model, "boxShadowColor")).toString(),
           state.shadowColor);
  QCOMPARE(model.data(index, roleForName(model, "boxShadowOffsetX")).toInt(),
           state.shadowOffsetX);
  QCOMPARE(model.data(index, roleForName(model, "boxShadowOffsetY")).toInt(),
           state.shadowOffsetY);
  QCOMPARE(model.data(index, roleForName(model, "boxShadowBlurSize")).toInt(),
           state.shadowBlurSize);
  QCOMPARE(model.data(index, roleForName(model, "boxGradient")).toBool(),
           state.gradient);
  QCOMPARE(model.data(index, roleForName(model, "boxGradientDirection")).toInt(),
           state.gradientDirection);
  QCOMPARE(model.data(index, roleForName(model, "boxGradientColorA")).toString(),
           state.gradientColorA);
  QCOMPARE(model.data(index, roleForName(model, "boxGradientColorB")).toString(),
           state.gradientColorB);
  QCOMPARE(model.data(index, roleForName(model, "boxPerspective")).toBool(),
           state.perspective);
  QCOMPARE(model.data(index, roleForName(model, "boxPerspectiveNw")).toList(),
           state.perspectiveNw);
  QCOMPARE(model.data(index, roleForName(model, "boxPerspectiveNe")).toList(),
           state.perspectiveNe);
  QCOMPARE(model.data(index, roleForName(model, "boxPerspectiveSe")).toList(),
           state.perspectiveSe);
  QCOMPARE(model.data(index, roleForName(model, "boxPerspectiveSw")).toList(),
           state.perspectiveSw);
  QCOMPARE(model.data(index, roleForName(model, "boxPath")).toBool(), state.path);
  QCOMPARE(model.data(index, roleForName(model, "boxPathMode")).toInt(),
           state.pathMode);
  QCOMPARE(model.data(index, roleForName(model, "boxPathPoints")).toList(),
           state.pathPoints);
  QCOMPARE(model.data(index, roleForName(model, "boxEffects")).toMap(),
           state.effects);
}
} // namespace

class EditorControllerWorkflowTests final : public QObject {
  Q_OBJECT

private slots:
  void noProjectDisablesProjectActions() {
    EditorController editor;

    QVERIFY(!editor.hasProject());
    QVERIFY(editor.actionEnabled(QStringLiteral("new")));
    QVERIFY(editor.actionEnabled(QStringLiteral("open")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("save")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
  }

  void commandAvailabilityPolicyPreservesCurrentMatrix() {
    const CommandAvailabilityState noProject{};
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("new"), noProject));
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("open"), noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("paste"), noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("save"), noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("unknown-command"),
                                           noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("copy"), noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("delete"), noProject));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("duplicate"),
                                            noProject));

    const CommandAvailabilityState projectWithoutSelection{true, false};
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("paste"),
                                           projectWithoutSelection));
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("save"),
                                           projectWithoutSelection));
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("unknown-command"),
                                           projectWithoutSelection));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("copy"),
                                            projectWithoutSelection));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("delete"),
                                            projectWithoutSelection));
    QVERIFY(!CommandAvailability::isEnabled(QStringLiteral("duplicate"),
                                            projectWithoutSelection));

    const CommandAvailabilityState projectWithSelection{true, true};
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("copy"),
                                           projectWithSelection));
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("delete"),
                                           projectWithSelection));
    QVERIFY(CommandAvailability::isEnabled(QStringLiteral("duplicate"),
                                           projectWithSelection));
  }

  void actionEnabledUsesProjectAndSelectedBoxAvailability() {
    EditorController editor;

    QVERIFY(editor.actionEnabled(QStringLiteral("new")));
    QVERIFY(editor.actionEnabled(QStringLiteral("open")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("paste")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("save")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("unknown-command")));

    editor.newDocument();

    QVERIFY(editor.actionEnabled(QStringLiteral("paste")));
    QVERIFY(editor.actionEnabled(QStringLiteral("save")));
    QVERIFY(editor.actionEnabled(QStringLiteral("unknown-command")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("copy")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("duplicate")));

    editor.createTextBox(10, 20, MinBoxSize, MinBoxSize);

    QVERIFY(editor.actionEnabled(QStringLiteral("copy")));
    QVERIFY(editor.actionEnabled(QStringLiteral("delete")));
    QVERIFY(editor.actionEnabled(QStringLiteral("duplicate")));

    editor.selectBox(-1);

    QVERIFY(!editor.actionEnabled(QStringLiteral("copy")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("delete")));
    QVERIFY(!editor.actionEnabled(QStringLiteral("duplicate")));
    QVERIFY(editor.actionEnabled(QStringLiteral("save")));
    QVERIFY(editor.actionEnabled(QStringLiteral("unknown-command")));
  }

  void commandRefreshesDocumentState() {
    EditorController editor;
    QSignalSpy changed(&editor, &EditorController::documentChanged);

    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(editor.selectedIndex(), 0);
    QVERIFY(editor.dirty());
    QVERIFY(changed.count() >= 1);
  }

  void paintWorkflowKeepsTextBoxesSeparate() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Keep me"));

    const QVariantList strokePoints{QVariantList{12.0, 14.0},
                                    QVariantList{30.0, 14.0}};
    editor.addPaintStroke(QStringLiteral("behind_text"), QStringLiteral("ff0000ff"),
                          8.0, 0.75, strokePoints);
    editor.addPaintStroke(QStringLiteral("above_text"), QStringLiteral("0000ffff"),
                          8.0, 1.0, strokePoints);
    editor.addPaintStroke(QStringLiteral("above_text"), QStringLiteral("zzzzzzzz"),
                          8.0, 1.0, strokePoints);

    QCOMPARE(editor.paintBehindText().size(), 1);
    QCOMPARE(editor.paintAboveText().size(), 2);
    QCOMPARE(editor.paintAboveText().at(1).toMap().value(QStringLiteral("color")).toString(),
             QStringLiteral("000000ff"));
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("Keep me"));
    QVERIFY(editor.dirty());

    editor.erasePaintAt(QStringLiteral("behind_text"), 20.0, 14.0, 10.0);

    QCOMPARE(editor.paintBehindText().size(), 0);
    QCOMPARE(editor.paintAboveText().size(), 2);
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("Keep me"));
  }

  void undersizedTextBoxCreateWithoutProjectIsSilent() {
    EditorController editor;
    QSignalSpy notificationChanged(&editor,
                                   &EditorController::notificationChanged);

    editor.createTextBox(10, 20, MinBoxSize - 1.0, MinBoxSize);

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.boxes().size(), 0);
    QCOMPARE(editor.notification(), QString());
    QCOMPARE(notificationChanged.count(), 0);
  }

  void validTextBoxCreateWithoutProjectStillNotifies() {
    EditorController editor;
    QSignalSpy notificationChanged(&editor,
                                   &EditorController::notificationChanged);

    editor.createTextBox(10, 20, MinBoxSize, MinBoxSize);

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.boxes().size(), 0);
    QCOMPARE(editor.notification(),
             QStringLiteral("Open a project before creating text"));
    QCOMPARE(notificationChanged.count(), 1);
  }

  void undersizedTextBoxCreateWithProjectDoesNotMutateDocument() {
    EditorController editor;
    editor.newDocument();
    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy selectionChanged(&editor, &EditorController::selectionChanged);
    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

    editor.createTextBox(10, 20, MinBoxSize - 1.0, MinBoxSize);
    editor.createTextBox(10, 20, MinBoxSize, MinBoxSize - 1.0);

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.boxes().size(), 0);
    QCOMPARE(editor.selectedIndex(), -1);
    QVERIFY(!editor.dirty());
    QCOMPARE(documentChanged.count(), 0);
    QCOMPARE(selectionChanged.count(), 0);
    QCOMPARE(stateChanged.count(), 0);
  }

  void createdBoxesAreEmptyExactDragWithTypeXMinimum() {
    EditorController editor;
    editor.newDocument();

    editor.createTextBox(10, 20, 13, 14);
    editor.createTextBox(30, 40, 1, 2);

    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QString());
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("w")).toDouble(),
             13.0);
    QCOMPARE(editor.boxes().at(0).toMap().value(QStringLiteral("h")).toDouble(),
             14.0);
  }

  void boxesModelExposesDocumentBackedRoles() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 120, 60);
    editor.updateSelectedText(QStringLiteral("Live model"));
    editor.setSelectedFontFamily(QStringLiteral("TextFX Missing Model Font"));
    editor.setSelectedFontSize(28);
    editor.setSelectedOutlineEnabled(true);
    editor.addSelectedOutlineLayer();
    editor.setSelectedOutlineLayerSize(1, 9);
    editor.setSelectedOutlineLayerColor(1, QStringLiteral("#445566"));
    editor.setSelectedPathEnabled(true);
    editor.setPerspectiveHandle(QStringLiteral("ne"), 0.3, 0.4);

    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1);

    const QModelIndex first = model->index(0, 0);
    QVERIFY(first.isValid());
    QCOMPARE(model->data(first, roleForName(*model, "boxIndex")).toInt(), 0);
    QCOMPARE(model->data(first, roleForName(*model, "boxText")).toString(),
             QStringLiteral("Live model"));
    QCOMPARE(model->data(first, roleForName(*model, "boxX")).toDouble(), 10.0);
    QCOMPARE(model->data(first, roleForName(*model, "boxWidth")).toDouble(), 120.0);
    QCOMPARE(model->data(first, roleForName(*model, "boxFontSize")).toInt(), 28);
    QCOMPARE(model->data(first, roleForName(*model, "boxFontFamily")).toString(),
             QStringLiteral("TextFX Missing Model Font"));
    QVERIFY(!model->data(first, roleForName(*model, "boxResolvedFontFamily"))
                 .toString()
                 .isEmpty());
    QVERIFY(model->data(first, roleForName(*model, "boxOutline")).toBool());
    QVERIFY(model->data(first, roleForName(*model, "boxPath")).toBool());
    QCOMPARE(model->data(first, roleForName(*model, "boxPathPoints")).toList().size(),
             3);
    QVERIFY(model->data(first, roleForName(*model, "boxPerspective")).toBool());
    QCOMPARE(model->data(first, roleForName(*model, "boxPerspectiveNe"))
                 .toList()
                 .at(0)
                 .toDouble(),
             0.3);
    const auto effects = model->data(first, roleForName(*model, "boxEffects"))
                             .toMap();
    QVERIFY(effects.value(QStringLiteral("outline")).toMap()
                .value(QStringLiteral("enabled"))
                .toBool());
    QCOMPARE(effects.value(QStringLiteral("outline"))
                 .toMap()
                 .value(QStringLiteral("layers"))
                 .toList()
                 .size(),
             2);
    QCOMPARE(effects.value(QStringLiteral("path")).toMap()
                 .value(QStringLiteral("points"))
                 .toList()
                 .size(),
             3);
  }

  void boxesModelRoleValuesMatchRenderStateMapping() {
    DocumentModel document;
    TextBox box;
    box.text = "Mapped role values";
    box.bounds = {12.0, 34.0, 156.0, 78.0};
    box.rotationDegrees = 17.5;
    box.style.fontFamily = "TextFX Missing Model Font";
    box.style.fontSize = 29;
    box.style.textColor = "abcdef99";
    box.style.lineSpacing = 6;
    box.style.letterSpacing = 2;
    box.style.bold = true;
    box.style.italic = true;
    box.style.uppercase = true;
    box.style.lowercase = true;
    box.style.alignment = TextAlignment::Right;
    box.effects.outlineEnabled = true;
    box.effects.outlineColor = "111111ff";
    box.effects.outlineSize = 4;
    box.effects.outlineLayers = {{true, "111111ff", 12},
                                 {false, "333333ff", 4}};
    box.effects.blurEnabled = true;
    box.effects.blurSize = 5;
    box.effects.shadowEnabled = true;
    box.effects.shadowColor = "222222ff";
    box.effects.shadowOffsetX = -3;
    box.effects.shadowOffsetY = 8;
    box.effects.shadowBlurSize = 9;
    box.effects.gradientEnabled = true;
    box.effects.gradientDirection = 1;
    box.effects.gradientColorA = "123456ff";
    box.effects.gradientColorB = "654321ff";
    box.effects.pathEnabled = true;
    box.effects.pathMode = 1;
    box.effects.pathPoints = {{0.1, 0.2}, {0.4, 0.5}, {0.8, 0.9}};
    box.effects.perspectiveEnabled = true;
    box.effects.perspectiveNw = {1.0, 2.0};
    box.effects.perspectiveNe = {3.0, 4.0};
    box.effects.perspectiveSe = {5.0, 6.0};
    box.effects.perspectiveSw = {7.0, 8.0};
    document.addTextBox(box);

    TextBox legacyOutlineBox;
    legacyOutlineBox.text = "Legacy outline compatibility";
    legacyOutlineBox.effects.outlineLayersSet = false;
    legacyOutlineBox.effects.outlineLayers.clear();
    legacyOutlineBox.effects.outlineEnabled = true;
    legacyOutlineBox.effects.outlineColor = "aa5500ff";
    legacyOutlineBox.effects.outlineSize = 7;
    document.addTextBox(legacyOutlineBox);

    TextBox explicitEmptyOutlineBox;
    explicitEmptyOutlineBox.text = "Explicit empty outline layers";
    explicitEmptyOutlineBox.effects.outlineLayersSet = true;
    explicitEmptyOutlineBox.effects.outlineLayers.clear();
    explicitEmptyOutlineBox.effects.outlineEnabled = true;
    explicitEmptyOutlineBox.effects.outlineColor = "00aa55ff";
    explicitEmptyOutlineBox.effects.outlineSize = 11;
    document.addTextBox(explicitEmptyOutlineBox);

    BoxesModel model(document);
    QCOMPARE(model.rowCount(), 3);

    for (int row = 0; row < model.rowCount(); ++row) {
      compareBoxModelRolesToRenderState(
          model, row, mapBoxRenderState(document.textBoxes().at(row), row));
    }

    const QModelIndex legacyIndex = model.index(1, 0);
    QVERIFY(model.data(legacyIndex, roleForName(model, "boxOutline")).toBool());
    QCOMPARE(model.data(legacyIndex, roleForName(model, "boxOutlineSize")).toInt(),
             7);

    const QModelIndex explicitEmptyIndex = model.index(2, 0);
    QVERIFY(!model.data(explicitEmptyIndex, roleForName(model, "boxOutline")).toBool());
    QCOMPARE(model.data(explicitEmptyIndex, roleForName(model, "boxOutlineSize")).toInt(),
             0);
    const auto explicitEmptyOutline =
        model.data(explicitEmptyIndex, roleForName(model, "boxEffects"))
            .toMap()
            .value(QStringLiteral("outline"))
            .toMap();
    QVERIFY(explicitEmptyOutline.value(QStringLiteral("layersSet")).toBool());
    QVERIFY(explicitEmptyOutline.value(QStringLiteral("layers")).toList().isEmpty());
  }

  void addOutlineLayerDefaultsToVisibleLargerSize() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 120, 60);
    editor.setSelectedOutlineEnabled(true);
    editor.setSelectedOutlineSize(6);

    editor.addSelectedOutlineLayer();

    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);
    const auto layers = model->data(model->index(0, 0), roleForName(*model, "boxEffects"))
                            .toMap()
                            .value(QStringLiteral("outline"))
                            .toMap()
                            .value(QStringLiteral("layers"))
                            .toList();
    QCOMPARE(layers.size(), 2);
    const int firstSize = layers.at(0).toMap().value(QStringLiteral("size")).toInt();
    const int addedSize = layers.at(1).toMap().value(QStringLiteral("size")).toInt();
    QCOMPARE(firstSize, 6);
    QVERIFY(addedSize > firstSize);
    QCOMPARE(model->data(model->index(0, 0), roleForName(*model, "boxOutlineSize"))
                 .toInt(),
             firstSize);
  }

  void effectMetadataKeepsLegacyRolesAndGroupedRoleTogether() {
    EditorController editor;
    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);

    const auto names = model->roleNames();
    QVERIFY(names.values().contains("boxOutline"));
    QVERIFY(names.values().contains("boxShadowBlurSize"));
    QVERIFY(names.values().contains("boxPathPoints"));
    QVERIFY(names.values().contains("boxEffects"));

    const int effectsRole = roleForName(*model, "boxEffects");
    QVERIFY(effectRoles(EffectId::Outline)
                .contains(roleForName(*model, "boxOutline")));
    QVERIFY(effectRoles(EffectId::Outline).contains(effectsRole));
    QVERIFY(effectRoles(EffectId::Perspective)
                .contains(roleForName(*model, "boxPerspectiveSw")));
    QVERIFY(effectRoles(EffectId::Perspective).contains(effectsRole));
    QVERIFY(allEffectRoles().contains(effectsRole));
  }

  void selectionQueryServiceSelectedBoxLookupPreservesInvalidIndexes() {
    std::vector<TextBox> boxes;
    int selectedIndex = -1;

    QVERIFY(SelectionQueryService::selectedBox(boxes, selectedIndex) == nullptr);
    QCOMPARE(selectedIndex, -1);

    TextBox first;
    first.text = "first";
    TextBox second;
    second.text = "second";
    boxes = {first, second};

    selectedIndex = -2;
    QVERIFY(SelectionQueryService::selectedBox(boxes, selectedIndex) == nullptr);
    QCOMPARE(selectedIndex, -2);

    selectedIndex = 1;
    const TextBox *selected = SelectionQueryService::selectedBox(boxes, selectedIndex);
    QVERIFY(selected != nullptr);
    QCOMPARE(QString::fromStdString(selected->text), QStringLiteral("second"));
    QCOMPARE(selectedIndex, 1);

    selectedIndex = 2;
    QVERIFY(SelectionQueryService::selectedBox(boxes, selectedIndex) == nullptr);
    QCOMPARE(selectedIndex, 2);
  }

  void selectionQueryServiceSelectedBoxViewModelMatchesEditorProjection() {
    std::vector<TextBox> boxes;
    TextBox box;
    box.text = "Projected box";
    box.bounds = {12.0, 34.0, 156.0, 78.0};
    box.style.fontSize = 29;
    boxes.push_back(box);

    const QVariant invalid = SelectionQueryService::selectedBoxViewModel(boxes, -1);
    QVERIFY(!invalid.isValid());

    const QVariant outOfRange = SelectionQueryService::selectedBoxViewModel(boxes, 1);
    QVERIFY(!outOfRange.isValid());

    const QVariantMap actual = SelectionQueryService::selectedBoxViewModel(boxes, 0).toMap();
    const QVariantMap expected = EditorViewModels::textBoxMap(boxes.at(0), 0);
    QCOMPARE(actual.value(QStringLiteral("index")), expected.value(QStringLiteral("index")));
    QCOMPARE(actual.value(QStringLiteral("text")), expected.value(QStringLiteral("text")));
    QCOMPARE(actual.value(QStringLiteral("fontSize")), expected.value(QStringLiteral("fontSize")));
  }

  void boxRolesAffectSelectedBoxStateClassifiesProductionRoles() {
    EditorController editor;
    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);

    auto role = [model](QByteArrayView name) {
      const int value = roleForName(*model, name);
      if (value < 0)
        QTest::qFail("Missing box role", __FILE__, __LINE__);
      return value;
    };

    const auto knownRoles = model->roleNames();

    QVERIFY(editor.boxRolesAffectSelectedBoxState({}));
    QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState({}, knownRoles));

    QVERIFY(editor.boxRolesAffectSelectedBoxState({Qt::UserRole + 10000}));
    QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState({Qt::UserRole + 10000}, knownRoles));
    QVERIFY(editor.boxRolesAffectSelectedBoxState(
        {QVariant(QStringLiteral("not-a-role-id"))}));
    QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState(
        {QVariant(QStringLiteral("not-a-role-id"))}, knownRoles));

    const QList<int> affectingRoles{
        role("boxText"), role("boxRotation"), role("boxFontFamily"),
        role("boxFontSize"), role("boxColor"), role("boxLineSpacing"),
        role("boxLetterSpacing"), role("boxBold"), role("boxItalic"),
        role("boxUppercase"), role("boxLowercase"), role("boxAlignment"),
        role("boxEffects"), role("boxOutline"), role("boxOutlineColor"),
        role("boxOutlineSize"), role("boxBlur"), role("boxBlurSize"),
        role("boxShadow"), role("boxShadowColor"), role("boxShadowOffsetX"),
        role("boxShadowOffsetY"), role("boxShadowBlurSize"), role("boxGradient"),
        role("boxGradientDirection"), role("boxGradientColorA"),
        role("boxGradientColorB"), role("boxPath"), role("boxPathMode"),
        role("boxPathPoints"), role("boxPerspective"), role("boxPerspectiveNw"),
        role("boxPerspectiveNe"), role("boxPerspectiveSe"), role("boxPerspectiveSw")};
    for (const int affectingRole : affectingRoles) {
      QVERIFY(editor.boxRolesAffectSelectedBoxState({affectingRole}));
      QVERIFY(SelectionQueryService::roleAffectsSelectedBoxState(affectingRole));
      QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState({affectingRole}, knownRoles));
    }

    const QList<int> nonAffectingRoles{role("boxIndex"), role("boxX"), role("boxY"),
                                       role("boxWidth"), role("boxHeight"),
                                       role("boxResolvedFontFamily")};
    for (const int nonAffectingRole : nonAffectingRoles) {
      QVERIFY(!editor.boxRolesAffectSelectedBoxState({nonAffectingRole}));
      QVERIFY(!SelectionQueryService::roleAffectsSelectedBoxState(nonAffectingRole));
      QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState({nonAffectingRole}, knownRoles) == false);
    }

    QVERIFY(editor.boxRolesAffectSelectedBoxState({role("boxX"), role("boxText")}));
    QVERIFY(SelectionQueryService::rolesAffectSelectedBoxState(
        {role("boxX"), role("boxText")}, knownRoles));
    QVERIFY(!SelectionQueryService::rolesAffectSelectedBoxState(
        {role("boxX"), role("boxY")}, knownRoles));
  }

  void boxesModelEmitsPreciseLiveRoleChangesAndStructuralSignals() {
    EditorController editor;
    editor.newDocument();

    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);
    QSignalSpy rowsInserted(model, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemoved(model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy dataChanged(model, &QAbstractItemModel::dataChanged);
    QSignalSpy modelReset(model, &QAbstractItemModel::modelReset);
    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);

    editor.createTextBox(1, 2, 80, 40);
    QCOMPARE(rowsInserted.count(), 1);
    QCOMPARE(model->rowCount(), 1);

    editor.beginTextEdit();
    editor.updateSelectedText(QStringLiteral("draft"));
    QCOMPARE(documentChanged.count(), 1);
    QVERIFY(dataChanged.count() >= 1);
    auto changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxText")));
    QCOMPARE(model->data(model->index(0, 0), roleForName(*model, "boxText")).toString(),
             QStringLiteral("draft"));
    editor.endTextEdit();

    editor.beginInteraction();
    editor.moveSelected(5, 7);
    changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxX")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxY")));
    QCOMPARE(model->data(model->index(0, 0), roleForName(*model, "boxX")).toDouble(),
             6.0);
    editor.endInteraction();

    editor.setSelectedOutlineEnabled(true);
    changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxOutline")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxOutlineColor")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxOutlineSize")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxEffects")));
    QVERIFY(!changedRoles.contains(roleForName(*model, "boxX")));

    editor.addSelectedOutlineLayer();
    changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxOutline")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxEffects")));

    editor.setSelectedOutlineLayerSize(1, 11);
    changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxOutlineSize")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxEffects")));
    const auto layeredOutline = model->data(model->index(0, 0), roleForName(*model, "boxEffects"))
                                    .toMap()
                                    .value(QStringLiteral("outline"))
                                    .toMap()
                                    .value(QStringLiteral("layers"))
                                    .toList();
    QCOMPARE(layeredOutline.size(), 2);
    QCOMPARE(layeredOutline.at(1).toMap().value(QStringLiteral("size")).toInt(), 11);

    editor.setSelectedPathEnabled(true);
    changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxPath")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxPathMode")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxPathPoints")));
    QVERIFY(changedRoles.contains(roleForName(*model, "boxEffects")));

    editor.deleteSelected();
    QCOMPARE(rowsRemoved.count(), 1);
    QCOMPARE(editor.selectedIndex(), -1);
    QCOMPARE(model->rowCount(), 0);

    editor.newDocument();
    QVERIFY(modelReset.count() >= 1);
  }

  void boxesExposeRequestedAndResolvedFontFamilies() {
    auto expectedResolvedFamily = [](const QString &family) {
      QFont font(family);
      return resolveFont(font).font.family();
    };

    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    const QString missingFamily =
        QStringLiteral("TextFX Missing Font For Resolver Test");
    editor.setSelectedFontFamily(missingFamily);

    QVariantMap box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), missingFamily);
    QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(),
             expectedResolvedFamily(missingFamily));
    QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() !=
            missingFamily);

    const QString beatDown = QStringLiteral("000 BeatDownBB [TeddyBear]");
    QFont beatDownFont(beatDown);
    const ResolvedFont beatDownResolution = resolveFont(beatDownFont);
    if (beatDownResolution.requestedAvailable) {
      QCOMPARE(QFontInfo(beatDownResolution.font).family(), beatDown);

      editor.setSelectedFontFamily(beatDown);
      box = editor.boxes().at(0).toMap();

      QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(), beatDown);
      QCOMPARE(box.value(QStringLiteral("resolvedFontFamily")).toString(),
               beatDownResolution.font.family());
      QVERIFY(!box.value(QStringLiteral("resolvedFontFamily"))
                   .toString()
                   .isEmpty());
    }
  }

  void activeTextEditingDefersModelResetUntilEditEnds() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.beginTextEdit();

    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

    editor.updateSelectedText(QStringLiteral("abc"));

    QCOMPARE(documentChanged.count(), 0);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("abc"));
    QVERIFY(editor.dirty());
    QVERIFY(stateChanged.count() >= 1);

    editor.endTextEdit();

    QCOMPARE(documentChanged.count(), 1);
  }

  void activeTextEditingDefersDocumentChangedForStyleMutations() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.beginTextEdit();

    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy selectedBoxChanged(&editor,
                                  &EditorController::selectedBoxChanged);
    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

    editor.setSelectedFontSize(32);

    QCOMPARE(documentChanged.count(), 0);
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("fontSize"))
                 .toInt(),
             32);
    QVERIFY(editor.dirty());
    QVERIFY(selectedBoxChanged.count() >= 1);
    QVERIFY(stateChanged.count() >= 1);

    editor.endTextEdit();

    QCOMPARE(documentChanged.count(), 1);
  }

  void copyPastePreservesCopiedBox() {
    EditorController editor;

    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Copied text"));
    editor.setSelectedOutlineEnabled(true);
    editor.setSelectedOutlineColor(QStringLiteral("#112233"));
    editor.setSelectedOutlineSize(5);
    editor.addSelectedOutlineLayer();
    editor.setSelectedOutlineLayerColor(1, QStringLiteral("#445566"));
    editor.setSelectedOutlineLayerSize(1, 9);
    editor.rotateSelected(12.5);
    editor.setPerspectiveHandle(QStringLiteral("ne"), 0.9, 0.1);
    editor.setSelectedPathMode(1);
    editor.setPathHandle(0, 0.2, 0.8);
    const auto copied = editor.boxes().at(0).toMap();
    editor.copySelected();
    editor.pasteBox();

    const auto boxes = editor.boxes();
    QCOMPARE(boxes.size(), 2);
    const auto pasted = boxes.at(1).toMap();
    QCOMPARE(pasted.value(QStringLiteral("text")).toString(),
             QStringLiteral("Copied text"));
    QVERIFY(!pasted.value(QStringLiteral("text"))
                 .toString()
                 .startsWith(QLatin1Char('{')));
    QCOMPARE(pasted.value(QStringLiteral("x")).toDouble(),
             copied.value(QStringLiteral("x")).toDouble() + 16.0);
    QCOMPARE(pasted.value(QStringLiteral("y")).toDouble(),
             copied.value(QStringLiteral("y")).toDouble() + 16.0);
    for (auto it = copied.cbegin(); it != copied.cend(); ++it) {
      const auto key = it.key();
      if (key == QStringLiteral("index") || key == QStringLiteral("x") ||
          key == QStringLiteral("y"))
        continue;
      QCOMPARE(pasted.value(key), it.value());
    }
    QCOMPARE(pasted.value(QStringLiteral("rotation")).toDouble(), 12.5);
    QVERIFY(pasted.value(QStringLiteral("outline")).toBool());
    QCOMPARE(pasted.value(QStringLiteral("outlineColor")).toString(),
             QStringLiteral("112233ff"));
    QCOMPARE(pasted.value(QStringLiteral("outlineSize")).toInt(), 5);
    const auto pastedOutlineLayers =
        pasted.value(QStringLiteral("outlineLayers")).toList();
    QCOMPARE(pastedOutlineLayers.size(), 2);
    QCOMPARE(pastedOutlineLayers.at(0).toMap().value(QStringLiteral("color")).toString(),
             QStringLiteral("112233ff"));
    QCOMPARE(pastedOutlineLayers.at(0).toMap().value(QStringLiteral("size")).toInt(),
             5);
    QCOMPARE(pastedOutlineLayers.at(1).toMap().value(QStringLiteral("color")).toString(),
             QStringLiteral("445566ff"));
    QCOMPARE(pastedOutlineLayers.at(1).toMap().value(QStringLiteral("size")).toInt(),
             9);
    QVERIFY(pasted.value(QStringLiteral("perspective")).toBool());
    const auto pastedNe =
        pasted.value(QStringLiteral("perspectiveNe")).toList();
    QCOMPARE(pastedNe.at(0).toDouble(), 0.9);
    QCOMPARE(pastedNe.at(1).toDouble(), 0.1);
    QVERIFY(pasted.value(QStringLiteral("path")).toBool());
    QCOMPARE(pasted.value(QStringLiteral("pathMode")).toInt(), 1);
    const auto pastedPathPoint =
        pasted.value(QStringLiteral("pathPoints")).toList().at(0).toList();
    QCOMPARE(pastedPathPoint.at(0).toDouble(), 0.2);
    QCOMPARE(pastedPathPoint.at(1).toDouble(), 0.8);
    QCOMPARE(editor.selectedIndex(), 1);
    QVERIFY(editor.dirty());

    QGuiApplication::clipboard()->setText(QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("text"), QStringLiteral("Legacy outline")},
                                  {QStringLiteral("x"), 1},
                                  {QStringLiteral("y"), 2},
                                  {QStringLiteral("w"), 80},
                                  {QStringLiteral("h"), 40},
                                  {QStringLiteral("outline"), true},
                                  {QStringLiteral("outlineColor"), QStringLiteral("778899ff")},
                                  {QStringLiteral("outlineSize"), 7}})
            .toJson(QJsonDocument::Compact)));
    editor.pasteBox();

    const auto legacyPasted = editor.boxes().at(2).toMap();
    QVERIFY(legacyPasted.value(QStringLiteral("outline")).toBool());
    QCOMPARE(legacyPasted.value(QStringLiteral("outlineColor")).toString(),
             QStringLiteral("778899ff"));
    QCOMPARE(legacyPasted.value(QStringLiteral("outlineSize")).toInt(), 7);
    const auto legacyOutlineLayers =
        legacyPasted.value(QStringLiteral("outlineLayers")).toList();
    QCOMPARE(legacyOutlineLayers.size(), 0);

    editor.copySelected();
    const auto legacyCopiedJson = QJsonDocument::fromJson(
        QGuiApplication::clipboard()->text().toUtf8()).object();
    QCOMPARE(legacyCopiedJson.value(QStringLiteral("outlineSize")).toInt(), 7);
    QVERIFY(!legacyCopiedJson.contains(QStringLiteral("outlineLayers")));

    QGuiApplication::clipboard()->setText(QStringLiteral("Plain text"));
    editor.pasteBox();

    QCOMPARE(
        editor.boxes().at(3).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Plain text"));
  }
  void openProjectLoadsFixtureImagesAndEmptyMissingData() {
    EditorController editor;
    editor.openProject(QStringLiteral(TEXTFX_FIXTURE_DIR "/project-workflow"));

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.pageCount(), 2);
    QCOMPARE(editor.currentPageIndex(), 0);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QVERIFY(editor.currentPageUrl().isLocalFile());
    QCOMPARE(editor.boxes().size(), 0);
    QVERIFY(!editor.dirty());
  }

  void openProjectUrlUsesQtLocalFileConversion() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath =
        dir.filePath(QStringLiteral("project with spaces"));
    QVERIFY(QDir().mkpath(projectPath));
    touch(projectPath + QStringLiteral("/page1.png"));

    EditorController editor;
    editor.openProjectUrl(QUrl::fromLocalFile(projectPath));

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.pageCount(), 1);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QVERIFY(editor.currentPageUrl().isLocalFile());
  }

  void failedSaveKeepsDocumentDirty() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.updateSelectedText(QStringLiteral("Unsaved"));
    QVERIFY(editor.dirty());

    QFile saveFolderBlocker(dir.filePath(QStringLiteral(".textfx")));
    QVERIFY(saveFolderBlocker.open(QIODevice::WriteOnly));
    saveFolderBlocker.write("not a directory");
    saveFolderBlocker.close();

    editor.save();

    QVERIFY(editor.dirty());
    QVERIFY(editor.notification().contains(QStringLiteral("Could not save boxes")));
  }

  void openProjectUrlRejectsNonLocalUrls() {
    EditorController editor;
    editor.openProjectUrl(
        QUrl(QStringLiteral("https://example.invalid/project")));

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(),
             QStringLiteral("Only local project folders can be opened."));
  }

  void newProjectCreatesCanonicalStructure() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));

    EditorController editor;
    editor.newProject(projectPath);

    QVERIFY(editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Cleaned")));
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Raw")));
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Typeset")));
    QVERIFY(QFile::exists(projectPath + QStringLiteral("/Texts.txt")));
    QCOMPARE(editor.pageCount(), 0);
    QCOMPARE(editor.notification(), QStringLiteral("New project created"));
  }

  void newProjectRejectsDirectoryAtTextsPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));
    QVERIFY(QDir().mkpath(projectPath + QStringLiteral("/Texts.txt")));

    EditorController editor;
    editor.newProject(projectPath);

    QVERIFY(!editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Texts.txt")));
    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not create project:")));
    QVERIFY(editor.notification().contains(QStringLiteral("Texts.txt")));
  }

  void newProjectRejectsUnreadableTextsFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath = dir.filePath(QStringLiteral("NewProject"));
    QVERIFY(QDir().mkpath(projectPath));

    const QString textsPath = projectPath + QStringLiteral("/Texts.txt");
    QFile texts(textsPath);
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page.png]\nExisting text\n");
    texts.close();
    const ProjectFileReadFailureGuard failProjectFileRead(textsPath);

    EditorController editor;
    editor.newProject(projectPath);

    QFile preserved(textsPath);
    QVERIFY(preserved.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(preserved.readAll()),
             QStringLiteral("[page.png]\nExisting text\n"));
    QVERIFY(!editor.hasProject());
    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not create project:")));
    QVERIFY(editor.notification().contains(QStringLiteral("Texts.txt")));
  }

  void openProjectRejectsUnreadableTextsFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page.png]\nExisting text\n");
    texts.close();
    const PageTextsOpenFailureGuard failTextsOpen(texts.fileName());

    EditorController editor;
    editor.openProject(dir.path());

    QFile preserved(texts.fileName());
    QVERIFY(preserved.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(preserved.readAll()),
             QStringLiteral("[page.png]\nExisting text\n"));
    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(), QStringLiteral("Could not open Texts.txt."));
  }

  void openProjectRejectsDirectoryAtTextsPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Texts.txt")));

    EditorController editor;
    editor.openProject(dir.path());

    QVERIFY(!editor.hasProject());
    QCOMPARE(editor.notification(), QStringLiteral("Could not open Texts.txt."));
  }

  void newProjectUrlUsesQtLocalFileConversion() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString projectPath =
        dir.filePath(QStringLiteral("project with spaces"));

    EditorController editor;
    editor.newProjectUrl(QUrl::fromLocalFile(projectPath));

    QVERIFY(editor.hasProject());
    QVERIFY(QDir(projectPath).exists(QStringLiteral("Cleaned")));
    QVERIFY(QFile::exists(projectPath + QStringLiteral("/Texts.txt")));
  }

  void openProjectLoadsCanonicalCleanedImagesAndTexts() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Cleaned")));
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("Cleaned/page2.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page2.png]\nCanonical text\n");
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());

    QVERIFY(editor.hasProject());
    QCOMPARE(editor.pageCount(), 1);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QCOMPARE(editor.pageTexts(),
             QStringList({QStringLiteral("Canonical text")}));
  }

  void navigationAutosavesAndReloadsCurrentPage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Saved on navigation"));
    QVERIFY(editor.dirty());

    editor.nextPage();

    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QVERIFY(!editor.dirty());
    const auto savedPath = dir.filePath(QStringLiteral(".textfx/page1.json"));
    QVERIFY(QFile::exists(savedPath));
    QCOMPARE(
        readJson(savedPath).value(QStringLiteral("boxes")).toArray().size(), 1);

    editor.previousPage();

    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Saved on navigation"));
  }

  void directPageSelectorAutosavesWithoutExportingPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    QCOMPARE(editor.pageLabels(),
             QStringList({QStringLiteral("1 - page1.png"),
                          QStringLiteral("2 - page2.png")}));
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Jump saved"));

    editor.goToPage(1);

    QCOMPARE(editor.currentPageName(), QStringLiteral("page2.png"));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".textfx/page1.json"))));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("Typeset/page1.png"))));
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
  }

  void failedPageNavigationPreservesCurrentDocument() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 100, 50);
    editor.updateSelectedText(QStringLiteral("Preserved page"));
    QVERIFY(editor.dirty());
    const auto selectedBox = editor.selectedBoxViewModel();

    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral(".textfx")));
    QFile corruptPage(dir.filePath(QStringLiteral(".textfx/page2.json")));
    QVERIFY(corruptPage.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corruptPage.write("not json");
    corruptPage.close();

    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);
    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy selectionChanged(&editor, &EditorController::selectionChanged);
    QSignalSpy selectedBoxChanged(&editor,
                                  &EditorController::selectedBoxChanged);
    QSignalSpy pageTextsChanged(&editor, &EditorController::pageTextsChanged);
    QSignalSpy notificationChanged(&editor,
                                   &EditorController::notificationChanged);

    editor.goToPage(1);

    QCOMPARE(editor.currentPageIndex(), 0);
    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QCOMPARE(editor.boxes().size(), 1);
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Preserved page"));
    QCOMPARE(editor.selectedIndex(), 0);
    QCOMPARE(editor.selectedBoxViewModel(), selectedBox);
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".textfx/page1.json"))));
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
    QVERIFY(!editor.dirty());
    QCOMPARE(editor.notification(),
             QStringLiteral("Save file is not valid JSON."));
    QCOMPARE(notificationChanged.count(), 1);
    QCOMPARE(stateChanged.count(), 1);
    QCOMPARE(documentChanged.count(), 0);
    QCOMPARE(selectionChanged.count(), 0);
    QCOMPARE(selectedBoxChanged.count(), 0);
    QCOMPARE(pageTextsChanged.count(), 0);
  }

  void openingProjectAbortsWhenCurrentAutosaveFails() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    touch(first.filePath(QStringLiteral("page1.png")));
    touch(second.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(first.path());
    editor.createTextBox(10, 20, 100, 50);
    QVERIFY(editor.dirty());

    QVERIFY(QFile(first.filePath(QStringLiteral(".textfx")))
                .open(QIODevice::WriteOnly));
    editor.openProject(second.path());

    QCOMPARE(editor.currentPageName(), QStringLiteral("page1.png"));
    QVERIFY(editor.notification().contains(QStringLiteral(".textfx")));
  }

  void saveWritesJsonAndCurrentExportedPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.updateSelectedText(QStringLiteral("Saved page"));
    const auto output = dir.filePath(QStringLiteral("Typeset/page1.png"));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Typeset")));
    QFile stale(output);
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("stale export");
    stale.close();
    editor.save();

    QVERIFY(!editor.dirty());
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral(".textfx/page1.json"))));
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);

    QVERIFY2(hasPngMagic(output), qPrintable(editor.notification()));
    QVERIFY(imageDiffers(dir.filePath(QStringLiteral("page1.png")), output));
    QFile first(output);
    QVERIFY(first.open(QIODevice::ReadOnly));
    const QByteArray firstBytes = first.readAll();
    first.close();
    editor.updateSelectedText(QStringLiteral("Changed page"));
    editor.save();
    QFile second(output);
    QVERIFY(second.open(QIODevice::ReadOnly));
    QVERIFY(firstBytes != second.readAll());
    QVERIFY(editor.notification().contains(
        QStringLiteral("Saved boxes and exported PNG to")));
  }

  void saveReportsBoxSaveFailure() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    QVERIFY(QFile(dir.filePath(QStringLiteral(".textfx")))
                .open(QIODevice::WriteOnly));

    editor.save();

    QVERIFY(editor.notification().startsWith(
        QStringLiteral("Could not save boxes:")));
  }

  void saveAllExportsMultiplePagesWithSavedBoxes() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));
    touch(dir.filePath(QStringLiteral("page3.png")));
    touch(dir.filePath(QStringLiteral("page4.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.updateSelectedText(QStringLiteral("First page"));
    editor.save();
    editor.nextPage();
    editor.createTextBox(3, 4, 90, 50);
    editor.updateSelectedText(QStringLiteral("Second page"));
    QFile emptyPage(dir.filePath(QStringLiteral(".textfx/page3.json")));
    QVERIFY(emptyPage.open(QIODevice::WriteOnly | QIODevice::Text));
    emptyPage.write(
        R"({"format":"textfx.page-boxes.v1","page":"page3.png","boxes":[]})");
    emptyPage.close();
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Typeset")));
    QFile stale(dir.filePath(QStringLiteral("Typeset/page3.png")));
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("stale export");
    stale.close();
    QFile staleMissingJson(dir.filePath(QStringLiteral("Typeset/page4.png")));
    QVERIFY(staleMissingJson.open(QIODevice::WriteOnly));
    staleMissingJson.write("stale export without json");
    staleMissingJson.close();
    editor.saveAll();

    QVERIFY(!editor.dirty());
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page2.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             1);
    QCOMPARE(readJson(dir.filePath(QStringLiteral(".textfx/page3.json")))
                 .value(QStringLiteral("boxes"))
                 .toArray()
                 .size(),
             0);

    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page1.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page2.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page3.png"))),
             qPrintable(editor.notification()));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page4.png"))),
             qPrintable(editor.notification()));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral(".textfx/page4.json"))));
    QCOMPARE(editor.notification(), QStringLiteral("Exported 4 page(s)."));
  }

  void saveAllReportsPageExportFailures() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.updateSelectedText(QStringLiteral("Missing source"));
    editor.save();
    editor.nextPage();
    editor.createTextBox(3, 4, 90, 50);
    editor.updateSelectedText(QStringLiteral("Still exports"));
    QVERIFY(QFile::remove(dir.filePath(QStringLiteral("page1.png"))));

    editor.saveAll();

    QVERIFY(!editor.dirty());
    QVERIFY(editor.notification().contains(QStringLiteral("1 failed")));
    QVERIFY2(hasPngMagic(dir.filePath(QStringLiteral("Typeset/page2.png"))),
             qPrintable(editor.notification()));
  }
  void liveInteractionDefersPreviewButRefreshesModel() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    QSignalSpy changed(&editor, &EditorController::documentChanged);
    QSignalSpy selectedBoxChanged(&editor,
                                  &EditorController::selectedBoxChanged);

    editor.beginInteraction();
    editor.moveSelected(5, 7);

    QCOMPARE(changed.count(), 0);
    QVERIFY(selectedBoxChanged.count() >= 1);
    editor.endInteraction();

    QCOMPARE(changed.count(), 1);
  }

  void gradientPathInteractionDoesNotGeneratePreviewArtifact() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 24, 18);
    editor.updateSelectedText(QStringLiteral("FX"));
    editor.setSelectedGradientEnabled(true);
    editor.setSelectedPathEnabled(true);
    QSignalSpy changed(&editor, &EditorController::documentChanged);
    QSignalSpy selectedBoxChanged(&editor,
                                  &EditorController::selectedBoxChanged);

    editor.beginInteraction();
    editor.moveSelected(5, 7);

    QCOMPARE(changed.count(), 0);
    QVERIFY(selectedBoxChanged.count() >= 1);
    editor.endInteraction();

    QCOMPARE(changed.count(), 1);
  }

  void transformChangesPersistOnSave() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(10, 20, 100, 50);
    editor.moveSelected(5, 7);
    editor.resizeSelected(10, 20);
    editor.rotateSelected(15);
    editor.save();

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 15.0);
    QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 27.0);
    QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 110.0);
    QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 70.0);
    QCOMPARE(box.value(QStringLiteral("rotation_degrees")).toDouble(), 15.0);
  }
  void selectedBoundsUseTypeXMinimum() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(10, 20, 100, 50);

    editor.setSelectedBounds(30, 40, 4, 6);

    const auto box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 30.0);
    QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 40.0);
    QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 12.0);
    QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 12.0);
  }
  void documentCoordinatesPersistAsCreated() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(12.5, 34.25, 123.75, 56.5);
    editor.moveSelected(3.5, -4.25);
    editor.resizeSelected(10.25, 20.5);
    editor.save();

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QCOMPARE(box.value(QStringLiteral("x")).toDouble(), 16.0);
    QCOMPARE(box.value(QStringLiteral("y")).toDouble(), 30.0);
    QCOMPARE(box.value(QStringLiteral("w")).toDouble(), 134.0);
    QCOMPARE(box.value(QStringLiteral("h")).toDouble(), 77.0);
  }

  void multilineEditUpdatesDocumentState() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);
    editor.beginTextEdit();
    QVERIFY(editor.editingText());
    editor.updateSelectedText(QStringLiteral("Line 1\nLine 2"));
    editor.endTextEdit();

    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Line 1\nLine 2"));
    QVERIFY(!editor.editingText());
  }

  void selectedBoxCanBeDeselectedWithoutDeletion() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.selectBox(-1);

    QCOMPARE(editor.selectedIndex(), -1);
    QCOMPARE(editor.boxes().size(), 1);
  }

  void selectedBoxViewModelTracksSelectionDirectly() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);
    editor.setSelectedFontFamily(QStringLiteral("TextFX Direct Selection"));
    editor.createTextBox(3, 4, 90, 50);

    QCOMPARE(editor.selectedIndex(), 1);
    QCOMPARE(editor.selectedBoxViewModel()
                 .toMap()
                 .value(QStringLiteral("index"))
                 .toInt(),
             1);

    editor.selectBox(0);

    const QVariantMap selected = editor.property("selectedBox").toMap();
    QCOMPARE(selected.value(QStringLiteral("index")).toInt(), 0);
    QCOMPARE(selected.value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("TextFX Direct Selection"));
  }

  void selectingAlreadySelectedBoxDoesNotEmitChanges() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);
    editor.createTextBox(3, 4, 90, 50);

    QSignalSpy selectionChanged(&editor, &EditorController::selectionChanged);
    QSignalSpy selectedBoxChanged(&editor,
                                  &EditorController::selectedBoxChanged);
    QSignalSpy stateChanged(&editor, &EditorController::stateChanged);

    editor.selectBox(1);

    QCOMPARE(selectionChanged.count(), 0);
    QCOMPARE(selectedBoxChanged.count(), 0);
    QCOMPARE(stateChanged.count(), 0);
  }

  void textPropertiesUpdateSelectedBox() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.setSelectedFontFamily(
        QStringLiteral("TextFX Custom Missing Family"));
    editor.setSelectedFontSize(42);
    editor.setSelectedTextColor(QStringLiteral("#ff00aa"));
    editor.setSelectedBold(true);
    editor.setSelectedItalic(true);
    editor.setSelectedUppercase(true);
    editor.setSelectedLowercase(true);
    editor.setSelectedAlignment(1);
    editor.setSelectedLineSpacing(12);
    editor.setSelectedLetterSpacing(3);

    const auto box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("TextFX Custom Missing Family"));
    QVERIFY(box.contains(QStringLiteral("resolvedFontFamily")));
    QVERIFY(
        !box.value(QStringLiteral("resolvedFontFamily")).toString().isEmpty());
    QVERIFY(box.value(QStringLiteral("resolvedFontFamily")).toString() !=
            box.value(QStringLiteral("fontFamily")).toString());
    QCOMPARE(box.value(QStringLiteral("fontSize")).toInt(), 42);
    QCOMPARE(box.value(QStringLiteral("color")).toString(),
             QStringLiteral("ff00aaff"));
    QVERIFY(box.value(QStringLiteral("bold")).toBool());
    QVERIFY(box.value(QStringLiteral("italic")).toBool());
    QVERIFY(!box.value(QStringLiteral("uppercase")).toBool());
    QVERIFY(box.value(QStringLiteral("lowercase")).toBool());
    QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("lineSpacing")).toInt(), 12);
    QCOMPARE(box.value(QStringLiteral("letterSpacing")).toInt(), 3);
    QVERIFY(editor.dirty());
  }

  void uppercaseAndLowercaseAreMutuallyExclusive() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.setSelectedLowercase(true);
    auto box = editor.boxes().at(0).toMap();
    QVERIFY(box.value(QStringLiteral("lowercase")).toBool());
    QVERIFY(!box.value(QStringLiteral("uppercase")).toBool());

    editor.setSelectedUppercase(true);
    box = editor.boxes().at(0).toMap();
    QVERIFY(box.value(QStringLiteral("uppercase")).toBool());
    QVERIFY(!box.value(QStringLiteral("lowercase")).toBool());
  }

  void effectControlsUpdateSelectedBox() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(1, 2, 80, 40);

    editor.setSelectedRotation(25.0);
    editor.rotateSelected(5.0);
    editor.setSelectedOutlineEnabled(true);
    editor.setSelectedOutlineColor(QStringLiteral("#112233"));
    editor.setSelectedOutlineSize(7);
    editor.setSelectedBlurEnabled(true);
    editor.setSelectedBlurSize(8);
    editor.setSelectedShadowEnabled(true);
    editor.setSelectedShadowColor(QStringLiteral("#445566"));
    editor.setSelectedShadowOffsetX(-9);
    editor.setSelectedShadowOffsetY(10);
    editor.setSelectedShadowBlurSize(11);
    editor.setSelectedGradientEnabled(true);
    editor.setSelectedGradientDirection(1);
    editor.setSelectedGradientColorA(QStringLiteral("#abcdef"));
    editor.setSelectedGradientColorB(QStringLiteral("#123456"));
    editor.setSelectedPerspectiveEnabled(true);
    editor.setPerspectiveHandle(QStringLiteral("ne"), 18.0, -2.0);
    editor.setPerspectiveHandle(QStringLiteral("se"), 7.0, 9.0);
    editor.setPerspectiveHandle(QStringLiteral("sw"), 1.0, 8.0);
    editor.setPerspectiveHandle(QStringLiteral("n"), 0.0, 6.0);
    editor.setPerspectiveHandle(QStringLiteral("e"), 22.0, 0.0);
    editor.setSelectedPathEnabled(true);
    editor.setSelectedPathMode(1);
    editor.addSelectedPathPoint();

    const auto box = editor.boxes().at(0).toMap();
    QCOMPARE(box.value(QStringLiteral("rotation")).toDouble(), 30.0);
    QVERIFY(box.value(QStringLiteral("outline")).toBool());
    QCOMPARE(box.value(QStringLiteral("outlineColor")).toString(),
             QStringLiteral("112233ff"));
    QCOMPARE(box.value(QStringLiteral("outlineSize")).toInt(), 7);
    QVERIFY(box.value(QStringLiteral("blur")).toBool());
    QCOMPARE(box.value(QStringLiteral("blurSize")).toInt(), 8);
    QVERIFY(box.value(QStringLiteral("shadow")).toBool());
    QCOMPARE(box.value(QStringLiteral("shadowColor")).toString(),
             QStringLiteral("445566ff"));
    QCOMPARE(box.value(QStringLiteral("shadowOffsetX")).toInt(), -9);
    QCOMPARE(box.value(QStringLiteral("shadowOffsetY")).toInt(), 10);
    QCOMPARE(box.value(QStringLiteral("shadowBlurSize")).toInt(), 11);
    QVERIFY(box.value(QStringLiteral("gradient")).toBool());
    QCOMPARE(box.value(QStringLiteral("gradientDirection")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("gradientColorA")).toString(),
             QStringLiteral("abcdefff"));
    QCOMPARE(box.value(QStringLiteral("gradientColorB")).toString(),
             QStringLiteral("123456ff"));
    QVERIFY(box.value(QStringLiteral("perspective")).toBool());
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNe")).toList().at(0).toDouble(),
        22.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNe")).toList().at(1).toDouble(),
        6.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveNw")).toList().at(1).toDouble(),
        6.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSe")).toList().at(0).toDouble(),
        22.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSe")).toList().at(1).toDouble(),
        9.0);
    QCOMPARE(
        box.value(QStringLiteral("perspectiveSw")).toList().at(0).toDouble(),
        1.0);
    QVERIFY(box.value(QStringLiteral("path")).toBool());
    QCOMPARE(box.value(QStringLiteral("pathMode")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("pathPoints")).toList().size(), 4);
    editor.setPathHandle(0, -0.3, 1.4);
    const auto clampedPath = editor.boxes()
                                 .at(0)
                                 .toMap()
                                 .value(QStringLiteral("pathPoints"))
                                 .toList()
                                 .at(0)
                                 .toList();
    QCOMPARE(clampedPath.at(0).toDouble(), 0.0);
    QCOMPARE(clampedPath.at(1).toDouble(), 1.0);
  }

  void pathPointsInsertIntoLongestSegmentInOrder() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(0, 0, 100, 40);
    editor.setSelectedPathEnabled(true);

    editor.addSelectedPathPoint();
    auto points = editor.boxes()
                      .at(0)
                      .toMap()
                      .value(QStringLiteral("pathPoints"))
                      .toList();
    QCOMPARE(points.size(), 4);
    QCOMPARE(points.at(1).toList().at(0).toDouble(), 0.25);
    QCOMPARE(points.at(1).toList().at(1).toDouble(), 0.5);

    editor.addSelectedPathPoint();
    points = editor.boxes()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("pathPoints"))
                 .toList();
    QCOMPARE(points.size(), 5);
    QCOMPARE(points.at(3).toList().at(0).toDouble(), 0.75);
    QCOMPARE(points.at(3).toList().at(1).toDouble(), 0.5);
  }

  void straightPathHandleMovementMatchesTypeXFreeDrag() {
    EditorController editor;
    editor.newDocument();
    editor.createTextBox(0, 0, 100, 40);
    editor.setSelectedPathEnabled(true);
    editor.setSelectedPathMode(0);
    editor.setPathHandle(1, 0.25, 0.9);

    auto point = editor.boxes()
                     .at(0)
                     .toMap()
                     .value(QStringLiteral("pathPoints"))
                     .toList()
                     .at(1)
                     .toList();
    QCOMPARE(point.at(0).toDouble(), 0.25);
    QCOMPARE(point.at(1).toDouble(), 0.9);

    editor.setPathHandle(0, 0.8, 0.2);
    point = editor.boxes()
                .at(0)
                .toMap()
                .value(QStringLiteral("pathPoints"))
                .toList()
                .at(0)
                .toList();
    QCOMPARE(point.at(0).toDouble(), 0.8);
    QCOMPARE(point.at(1).toDouble(), 0.2);

    editor.setSelectedPathMode(1);
    editor.setPathHandle(1, 0.25, 0.9);
    point = editor.boxes()
                .at(0)
                .toMap()
                .value(QStringLiteral("pathPoints"))
                .toList()
                .at(1)
                .toList();
    QCOMPARE(point.at(0).toDouble(), 0.25);
    QCOMPARE(point.at(1).toDouble(), 0.9);
  }

  void effectsPersistAsTextFXFields() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.setSelectedLowercase(true);
    editor.setSelectedAlignment(2);
    editor.setSelectedLineSpacing(9);
    editor.setSelectedLetterSpacing(4);
    editor.setSelectedShadowBlurSize(12);
    editor.setSelectedGradientEnabled(true);
    editor.setSelectedGradientDirection(1);
    editor.setSelectedGradientColorA(QStringLiteral("#ff0000"));
    editor.setSelectedGradientColorB(QStringLiteral("#0000ff"));
    editor.setPerspectiveHandle(QStringLiteral("nw"), 10.0, 20.0);
    editor.setSelectedPathEnabled(true);
    editor.setSelectedPathMode(1);
    editor.save();

    const auto box =
        readJson(dir.filePath(QStringLiteral(".textfx/page1.json")))
            .value(QStringLiteral("boxes"))
            .toArray()
            .at(0)
            .toObject();
    QVERIFY(box.value(QStringLiteral("lowercase")).toBool());
    QVERIFY(!box.value(QStringLiteral("uppercase")).toBool());
    QCOMPARE(box.value(QStringLiteral("alignment")).toInt(), 2);
    QCOMPARE(box.value(QStringLiteral("line_spacing")).toInt(), 9);
    QCOMPARE(box.value(QStringLiteral("letter_spacing")).toInt(), 4);
    QCOMPARE(box.value(QStringLiteral("shadow_blur_size")).toInt(), 12);
    QCOMPARE(box.value(QStringLiteral("gradient_direction")).toInt(), 1);
    QCOMPARE(box.value(QStringLiteral("gradient_color_a")).toString(),
             QStringLiteral("ff0000ff"));
    QCOMPARE(box.value(QStringLiteral("gradient_color_b")).toString(),
             QStringLiteral("0000ffff"));
    QVERIFY(box.value(QStringLiteral("perspective_offsets"))
                .toObject()
                .contains(QStringLiteral("nw")));
    QCOMPARE(box.value(QStringLiteral("perspective_offsets"))
                 .toObject()
                 .value(QStringLiteral("nw"))
                 .toArray()
                 .at(0)
                 .toDouble(),
             10.0);
    QCOMPARE(box.value(QStringLiteral("perspective_offsets"))
                 .toObject()
                 .value(QStringLiteral("nw"))
                 .toArray()
                 .at(1)
                 .toDouble(),
             20.0);
    QCOMPARE(box.value(QStringLiteral("path_points")).toArray().size(), 3);
    QCOMPARE(box.value(QStringLiteral("path_mode")).toInt(), 1);
  }
  void pageTextsLoadAndApplyToSelectedBox() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    touch(dir.filePath(QStringLiteral("page2.png")));
    QFile texts(dir.filePath(QStringLiteral("Texts.txt")));
    QVERIFY(texts.open(QIODevice::WriteOnly | QIODevice::Text));
    texts.write("[page1.png]\nFirst\nSecond\n[page2.png]\nOther\n");
    texts.close();

    EditorController editor;
    editor.openProject(dir.path());
    QCOMPARE(editor.pageTexts(),
             QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
    QVERIFY(!editor.applyNextPageText());
    editor.createTextBox(1, 2, 80, 40);

    QSignalSpy documentChanged(&editor, &EditorController::documentChanged);
    QSignalSpy pageTextsChanged(&editor, &EditorController::pageTextsChanged);

    QVERIFY(editor.applyNextPageText());
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("First"));
    QCOMPARE(documentChanged.count(), 1);
    QCOMPARE(pageTextsChanged.count(), 1);
    QVERIFY(editor.applyNextPageText());
    QCOMPARE(
        editor.boxes().at(0).toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Second"));
    QCOMPARE(documentChanged.count(), 2);
    QCOMPARE(pageTextsChanged.count(), 2);
    QVERIFY(!editor.applyNextPageText());
    QCOMPARE(documentChanged.count(), 2);
    QCOMPARE(pageTextsChanged.count(), 2);
    editor.nextPage();
    QCOMPARE(editor.pageTexts(), QStringList({QStringLiteral("Other")}));
  }

  void textPresetsApplyAddUpdateRenameAndDelete() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));

    EditorController editor;
    editor.openProject(dir.path());
    QVERIFY(editor.presets().empty());
    QCOMPARE(editor.selectedPresetIndex(), -1);
    editor.createTextBox(1, 2, 80, 40);

    QVERIFY(!editor.applySelectedPreset());
    QCOMPARE(editor.notification(), QStringLiteral("Select a text preset first"));
    EditorController noBoxEditor;
    noBoxEditor.openProject(dir.path());
    QVERIFY(!noBoxEditor.addPreset(QStringLiteral("No Box")));
    QCOMPARE(noBoxEditor.notification(),
             QStringLiteral("Select a box before saving a preset"));
    QVERIFY(!noBoxEditor.updateSelectedPreset());
    QCOMPARE(noBoxEditor.notification(),
             QStringLiteral("Select a box before updating a preset"));

    editor.setSelectedFontFamily(QStringLiteral("Inter"));
    editor.setSelectedFontSize(31);
    editor.setSelectedBold(true);
    QVERIFY(editor.addPreset(QStringLiteral("Narration")));
    QVERIFY(
        QFile::exists(dir.filePath(QStringLiteral(".textfx/presets.json"))));
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("name"))
                 .toString(),
             QStringLiteral("Narration"));

    editor.setSelectedFontSize(44);
    QVERIFY(editor.updateSelectedPreset());
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("fontSize"))
                 .toInt(),
             44);

    QVERIFY(editor.renameSelectedPreset(QStringLiteral("Caption")));
    QCOMPARE(editor.presets()
                 .last()
                 .toMap()
                 .value(QStringLiteral("name"))
                 .toString(),
             QStringLiteral("Caption"));
    QVERIFY(editor.deleteSelectedPreset());
    QVERIFY(editor.presets().empty());
    QCOMPARE(editor.selectedPresetIndex(), -1);
  }

  void loadedUserPresetCanBeAppliedUpdatedRenamedAndDeleted() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.filePath(QStringLiteral("page1.png")));
    writeJsonFile(
        dir.filePath(QStringLiteral(".textfx/presets.json")),
        QJsonObject{{QStringLiteral("format"),
                     QStringLiteral("textfx.text-presets.v1")},
                    {QStringLiteral("presets"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("name"), QStringLiteral("Narration")},
                         {QStringLiteral("properties"),
                          QJsonObject{{QStringLiteral("font_family"),
                                       QStringLiteral("Inter")},
                                      {QStringLiteral("font_size"), 24},
                                      {QStringLiteral("lowercase"), true}}}}}}});

    EditorController editor;
    editor.openProject(dir.path());
    editor.createTextBox(1, 2, 80, 40);
    editor.selectPreset(0);

    auto *model = qobject_cast<QAbstractItemModel *>(editor.boxesModel());
    QVERIFY(model);
    QSignalSpy dataChanged(model, &QAbstractItemModel::dataChanged);

    QVERIFY(editor.applySelectedPreset());
    QCOMPARE(dataChanged.count(), 1);
    const auto changedRoles = dataChanged.takeLast().at(2).value<QList<int>>();
    QVERIFY(changedRoles.contains(roleForName(*model, "boxLowercase")));
    QCOMPARE(editor.boxes()
                 .first()
                 .toMap()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Inter"));
    QVERIFY(model->data(model->index(0, 0), roleForName(*model, "boxLowercase"))
                .toBool());
    editor.setSelectedFontFamily(QStringLiteral("Custom Bubble"));
    QVERIFY(editor.updateSelectedPreset());

    QCOMPARE(editor.presets()
                 .first()
                 .toMap()
                  .value(QStringLiteral("name"))
                  .toString(),
             QStringLiteral("Narration"));
    QCOMPARE(editor.presets()
                 .first()
                 .toMap()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Custom Bubble"));
    QVERIFY(editor.renameSelectedPreset(QStringLiteral("Caption")));
    const auto saved =
        readJson(dir.filePath(QStringLiteral(".textfx/presets.json")))
            .value(QStringLiteral("presets"))
            .toArray();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Caption"));
    QVERIFY(editor.deleteSelectedPreset());
    QVERIFY(editor.presets().empty());
  }
};

QTEST_MAIN(EditorControllerWorkflowTests)

#include "editor_controller_workflow_tests.moc"
