#include "app/EditorController.h"

#include "application/queries/CommandAvailability.h"
#include "app/EditorControllerStringUtils.h"
#include "app/viewmodels/EditorViewModels.h"
#include "app/EffectMetadata.h"
#include "application/services/PageTextService.h"
#include "application/services/ProjectSessionService.h"
#include "application/services/TextBoxClipboardService.h"
#include "application/services/TextBoxEditingService.h"
#include "application/services/TextBoxSelectionService.h"
#include "application/services/TextWorkflowService.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUrl>

namespace textfx {
namespace {
using Role = BoxesModel::Role;
}

QString EditorController::currentPageName() const {
  return ProjectSessionService::pageName(pages_, currentPageIndex_);
}

QUrl EditorController::currentPageUrl() const {
  return currentPage_.empty()
             ? QUrl{}
             : QUrl::fromLocalFile(toQString(currentPage_.string()));
}

QUrl EditorController::rawPageUrl() const {
  if (!store_ || currentPage_.empty())
    return {};
  const auto raw = store_->rawPagePathFor(currentPage_);
  return raw.empty() ? QUrl{} : QUrl::fromLocalFile(toQString(raw.string()));
}

QVariantList EditorController::presets() const {
  return EditorViewModels::presetList(document_.presets());
}

QStringList EditorController::pageTexts() const {
  return PageTextService::textsForPage(pageTexts_, currentPageKey());
}

QStringList EditorController::pageLabels() const {
  return ProjectSessionService::pageLabels(pages_);
}

bool EditorController::actionEnabled(const QString &command) const {
  const CommandAvailabilityState state{.hasProject = hasProject(),
                                       .hasSelectedBox = selectedBox() != nullptr};
  return CommandAvailability::isEnabled(command, state);
}

void EditorController::updateSelectedText(const QString &text) {
  editSelectedBoxIf([&](TextBox &box) {
    box.text = toStdString(text);
    if (editingText_) {
      document_.setDirty(true);
      pendingDocumentChanged_ = true;
      boxesModel_.notifyBoxChanged(selectedIndex_, {Role::TextRole});
      emit selectedBoxChanged();
      emit stateChanged();
      return false;
    }
    return true;
  }, {Role::TextRole});
}

void EditorController::setSelectedFontFamily(const QString &family) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setFontFamily(box, family); },
      {Role::FontFamilyRole, Role::ResolvedFontFamilyRole});
}

void EditorController::setSelectedFontSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setFontSize(box, size); },
      {Role::FontSizeRole, Role::ResolvedFontFamilyRole});
}

void EditorController::setSelectedTextColor(const QString &color) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setTextColor(box, color); },
      {Role::ColorRole});
}

void EditorController::setSelectedBold(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBold(box, enabled); },
      {Role::BoldRole, Role::ResolvedFontFamilyRole});
}

void EditorController::setSelectedItalic(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setItalic(box, enabled); },
      {Role::ItalicRole, Role::ResolvedFontFamilyRole});
}

void EditorController::setSelectedUppercase(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setUppercase(box, enabled); },
      {Role::UppercaseRole, Role::LowercaseRole});
}

void EditorController::setSelectedLowercase(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setLowercase(box, enabled); },
      {Role::LowercaseRole, Role::UppercaseRole});
}

void EditorController::setSelectedAlignment(int alignment) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setAlignment(box, alignment);
  }, {Role::AlignmentRole});
}

void EditorController::setSelectedLineSpacing(int spacing) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setLineSpacing(box, spacing);
  }, {Role::LineSpacingRole});
}

void EditorController::setSelectedLetterSpacing(int spacing) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setLetterSpacing(box, spacing);
  }, {Role::LetterSpacingRole, Role::ResolvedFontFamilyRole});
}

void EditorController::moveSelected(double dx, double dy) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::move(box, dx, dy); },
      {Role::XRole, Role::YRole});
}

void EditorController::resizeSelected(double dw, double dh) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::resize(box, dw, dh); },
      {Role::WidthRole, Role::HeightRole});
}

void EditorController::setSelectedBounds(double x, double y, double w,
                                         double h) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBounds(box, x, y, w, h); },
      {Role::XRole, Role::YRole, Role::WidthRole, Role::HeightRole});
}

void EditorController::rotateSelected(double degrees) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::rotate(box, degrees); },
      {Role::RotationRole});
}

void EditorController::setSelectedRotation(double degrees) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setRotation(box, degrees); },
      {Role::RotationRole});
}

void EditorController::setSelectedOutlineEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setOutlineEnabled(box, enabled);
  }, effectRoles(EffectId::Outline));
}

void EditorController::setSelectedOutlineColor(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setOutlineColor(box, color);
  }, {Role::OutlineColorRole, Role::EffectsRole});
}

void EditorController::setSelectedOutlineSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setOutlineSize(box, size); },
      {Role::OutlineSizeRole, Role::EffectsRole});
}

void EditorController::addSelectedOutlineLayer() {
  editSelectedBox([](TextBox &box) { TextBoxEditingService::addOutlineLayer(box); },
                  effectRoles(EffectId::Outline));
}

void EditorController::removeSelectedOutlineLayer(int index) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::removeOutlineLayer(box, index);
  }, effectRoles(EffectId::Outline));
}

void EditorController::setSelectedOutlineLayerEnabled(int index, bool enabled) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::setOutlineLayerEnabled(box, index, enabled);
  }, effectRoles(EffectId::Outline));
}

void EditorController::setSelectedOutlineLayerColor(int index,
                                                    const QString &color) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::setOutlineLayerColor(box, index, color);
  }, effectRoles(EffectId::Outline));
}

void EditorController::setSelectedOutlineLayerSize(int index, int size) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::setOutlineLayerSize(box, index, size);
  }, effectRoles(EffectId::Outline));
}

void EditorController::setSelectedBlurEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setBlurEnabled(box, enabled);
  }, effectRoles(EffectId::Blur));
}

void EditorController::setSelectedBlurSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBlurSize(box, size); },
      {Role::BlurSizeRole, Role::EffectsRole});
}

void EditorController::setSelectedShadowEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowEnabled(box, enabled);
  }, effectRoles(EffectId::Shadow));
}

void EditorController::setSelectedShadowColor(const QString &color) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setShadowColor(box, color); },
      {Role::ShadowColorRole, Role::EffectsRole});
}

void EditorController::setSelectedShadowOffsetX(int offset) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowOffsetX(box, offset);
  }, {Role::ShadowOffsetXRole, Role::EffectsRole});
}

void EditorController::setSelectedShadowOffsetY(int offset) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowOffsetY(box, offset);
  }, {Role::ShadowOffsetYRole, Role::EffectsRole});
}

void EditorController::setSelectedShadowBlurSize(int size) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowBlurSize(box, size);
  }, {Role::ShadowBlurSizeRole, Role::EffectsRole});
}

void EditorController::setSelectedGradientEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientEnabled(box, enabled);
  }, effectRoles(EffectId::Gradient));
}

void EditorController::setSelectedGradientDirection(int direction) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientDirection(box, direction);
  }, {Role::GradientDirectionRole, Role::EffectsRole});
}

void EditorController::setSelectedGradientColorA(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientColorA(box, color);
  }, {Role::GradientColorARole, Role::EffectsRole});
}

void EditorController::setSelectedGradientColorB(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientColorB(box, color);
  }, {Role::GradientColorBRole, Role::EffectsRole});
}

void EditorController::setSelectedPerspectiveEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPerspectiveEnabled(box, enabled);
  }, effectRoles(EffectId::Perspective));
}

void EditorController::setSelectedPathEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPathEnabled(box, enabled);
  }, effectRoles(EffectId::Path));
}

void EditorController::setSelectedPathMode(int mode) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setPathMode(box, mode); },
      {Role::PathModeRole, Role::EffectsRole});
}

void EditorController::resetSelectedPerspective() {
  editSelectedBox(
      [](TextBox &box) { TextBoxEditingService::resetPerspective(box); },
      effectRoles(EffectId::Perspective));
}

void EditorController::resetSelectedPath() {
  editSelectedBox([](TextBox &box) { TextBoxEditingService::resetPath(box); },
                  effectRoles(EffectId::Path));
}

void EditorController::addSelectedPathPoint() {
  editSelectedBox(
      [](TextBox &box) { TextBoxEditingService::addPathPoint(box); },
      {Role::PathPointsRole, Role::EffectsRole});
}

void EditorController::setPerspectiveHandle(const QString &corner, double x,
                                            double y) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPerspectiveHandle(box, corner, x, y);
  }, effectRoles(EffectId::Perspective));
}

void EditorController::setPathHandle(int index, double x, double y) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::setPathHandle(box, index, x, y);
  }, {Role::PathRole, Role::PathPointsRole, Role::EffectsRole});
}

void EditorController::duplicateSelected() {
  if (!selectedBox())
    return;
  const int row = static_cast<int>(document_.textBoxes().size());
  boxesModel_.beginInsertBox(row);
  if (TextBoxSelectionService::duplicateSelected(document_.textBoxes(),
                                                  selectedIndex_)) {
    boxesModel_.endInsertBox();
    markDocumentChanged();
    emit selectionChanged();
  } else {
    boxesModel_.endInsertBox();
  }
}

void EditorController::deleteSelected() {
  if (!selectedBox())
    return;
  const int row = selectedIndex_;
  boxesModel_.beginRemoveBox(row);
  if (TextBoxSelectionService::deleteSelected(document_.textBoxes(),
                                               selectedIndex_)) {
    boxesModel_.endRemoveBox();
    markDocumentChanged();
    emit selectionChanged();
  } else {
    boxesModel_.endRemoveBox();
  }
}

void EditorController::moveLayer(int to) {
  boxesModel_.beginResetBoxes();
  if (TextBoxSelectionService::moveLayer(document_.textBoxes(), selectedIndex_,
                                          to)) {
    boxesModel_.endResetBoxes();
    markDocumentChanged();
    emit selectionChanged();
  } else {
    boxesModel_.endResetBoxes();
  }
}

void EditorController::copySelected() {
  if (const auto *box = selectedBox()) {
    QGuiApplication::clipboard()->setText(
        TextBoxClipboardService::serialize(*box, selectedIndex_));
    setNotification(QStringLiteral("Copied"));
  }
}

void EditorController::pasteBox() {
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before pasting"));
    return;
  }
  const auto clipboardText = QGuiApplication::clipboard()->text();
  auto box = TextBoxClipboardService::deserializeOrPlainText(clipboardText);
  const int row = static_cast<int>(document_.textBoxes().size());
  boxesModel_.beginInsertBox(row);
  document_.addTextBox(std::move(box));
  boxesModel_.endInsertBox();
  selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
  markDocumentChanged();
  emit selectionChanged();
}

bool EditorController::applyPageText(int index) {
  auto ctx = pageTextWorkflowContext();
  const auto result = TextWorkflowService::applyPageText(ctx, index);
  switch (result.status) {
  case PageTextApplyStatus::Applied:
    markDocumentChanged({Role::TextRole});
    emit pageTextsChanged();
    return true;
  case PageTextApplyStatus::NoSelectedBox:
    setNotification(QStringLiteral("Select a box before applying page text"));
    return false;
  case PageTextApplyStatus::MissingText:
    return false;
  }
  return false;
}

bool EditorController::applyNextPageText() {
  auto ctx = pageTextWorkflowContext();
  const int index = TextWorkflowService::nextPageTextIndex(ctx);
  if (!applyPageText(index))
    return false;
  return true;
}

void EditorController::selectPreset(int index) {
  selectedPresetIndex_ =
      index >= 0 && index < static_cast<int>(document_.presets().size()) ? index
                                                                         : -1;
  emit documentChanged();
}

bool EditorController::applySelectedPreset() {
  auto ctx = presetWorkflowContext();
  const auto result = TextWorkflowService::applySelectedPreset(ctx);
  if (result.precondition == WorkflowPrecondition::MissingSelectedBox) {
    setNotification(QStringLiteral("Select a box before applying a preset"));
    return false;
  }
  if (result.serviceStatus == TextPresetStatus::InvalidPresetIndex) {
    setNotification(QStringLiteral("Select a text preset first"));
    return false;
  }
  if (!result.succeeded())
    return false;
  markDocumentChanged(allBoxRoles());
  return true;
}

bool EditorController::addPreset(const QString &name) {
  auto ctx = presetWorkflowContext();
  const auto result = TextWorkflowService::addPreset(ctx, name);
  if (result.precondition == WorkflowPrecondition::MissingSelectedBox) {
    setNotification(QStringLiteral("Select a box before saving a preset"));
    return false;
  }
  if (!result.succeeded())
    return false;
  return saveProjectPresets(result.preferredName);
}

bool EditorController::updateSelectedPreset() {
  auto ctx = presetWorkflowContext();
  const auto result = TextWorkflowService::updateSelectedPreset(ctx);
  if (result.precondition == WorkflowPrecondition::MissingSelectedBox) {
    setNotification(QStringLiteral("Select a box before updating a preset"));
    return false;
  }
  if (!result.succeeded())
    return false;
  return saveProjectPresets(result.preferredName);
}

bool EditorController::renameSelectedPreset(const QString &name) {
  auto ctx = presetWorkflowContext();
  const auto result = TextWorkflowService::renameSelectedPreset(ctx, name);
  if (!result.succeeded())
    return false;
  return saveProjectPresets(result.preferredName);
}

bool EditorController::deleteSelectedPreset() {
  auto ctx = presetWorkflowContext();
  const auto result = TextWorkflowService::deleteSelectedPreset(ctx);
  if (!result.succeeded())
    return false;
  return saveProjectPresets();
}

} // namespace textfx
