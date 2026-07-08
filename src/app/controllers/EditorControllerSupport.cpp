#include "app/controllers/EditorController.h"

#include "app/project/ProjectPageLoadWorkflow.h"
#include "app/project/ProjectPersistenceWorkflow.h"
#include "app/project/ProjectSession.h"
#include "application/queries/EffectMetadata.h"
#include "application/services/ProjectSessionService.h"
#include "application/queries/SelectionQueryService.h"
#include "application/services/TextWorkflowService.h"

#include <QGuiApplication>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

#include <utility>

namespace textfx {

bool EditorController::leftMouseButtonDown() const {
  return QGuiApplication::mouseButtons().testFlag(Qt::LeftButton);
}

void EditorController::beginInteraction() { ++interactionDepth_; }

void EditorController::endInteraction() {
  if (interactionDepth_ <= 0)
    return;
  --interactionDepth_;
  if (interactionDepth_ == 0 && flushPendingDocumentChanged())
    emit stateChanged();
}

void EditorController::beginTextEdit() {
  if (selectedBox() && !editingText_) {
    beginInteraction();
    editingText_ = true;
    emit stateChanged();
  }
}

void EditorController::endTextEdit() {
  const bool wasEditing = editingText_;
  if (wasEditing) {
    if (interactionDepth_ > 0)
      --interactionDepth_;
    flushPendingDocumentChanged();
  }
  editingText_ = false;
  emit stateChanged();
}

void EditorController::applyTextLineSpacing(QObject *quickTextDocument,
                                            double spacing) {
  if (!quickTextDocument)
    return;
  auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
  auto *document = wrapper ? wrapper->textDocument() : nullptr;
  if (!document)
    return;

  document->setDocumentMargin(0.0);

  QTextCursor cursor(document);
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    const QTextBlockFormat current = block.blockFormat();
    if (qFuzzyIsNull(current.lineHeight() - spacing) &&
        current.lineHeightType() == QTextBlockFormat::LineDistanceHeight)
      continue;

    QTextBlockFormat updated;
    updated.setLineHeight(spacing, QTextBlockFormat::LineDistanceHeight);
    cursor.setPosition(block.position());
    cursor.mergeBlockFormat(updated);
  }
}

void EditorController::notify(const QString &message) {
  setNotification(message);
}

void EditorController::setRawVisible(bool visible) {
  if (rawVisible_ == visible) {
    return;
  }
  rawVisible_ = visible;
  emit stateChanged();
}

TextBox *EditorController::selectedBox() {
  return SelectionQueryService::selectedBox(document_.textBoxes(), selectedIndex_);
}

const TextBox *EditorController::selectedBox() const {
  return SelectionQueryService::selectedBox(document_.textBoxes(), selectedIndex_);
}

bool EditorController::editSelectedBoxIf(
    const std::function<bool(TextBox &)> &mutation) {
  return editSelectedBoxIf(mutation, allBoxRoles());
}

bool EditorController::editSelectedBoxIf(
    const std::function<bool(TextBox &)> &mutation, const QList<int> &roles) {
  auto *box = selectedBox();
  if (!box || !mutation(*box))
    return false;
  markDocumentChanged(roles);
  return true;
}

void EditorController::editSelectedBox(
    const std::function<void(TextBox &)> &mutation) {
  editSelectedBox(mutation, allBoxRoles());
}

void EditorController::editSelectedBox(
    const std::function<void(TextBox &)> &mutation, const QList<int> &roles) {
  editSelectedBoxIf([&](TextBox &box) {
    mutation(box);
    return true;
  }, roles);
}

void EditorController::markDocumentChanged() {
  markDocumentChanged(allBoxRoles());
}

void EditorController::markDocumentChanged(const QList<int> &roles) {
  document_.setDirty(true);
  boxesModel_.notifyBoxChanged(selectedIndex_, roles);
  if (interactionDepth_ > 0) {
    pendingDocumentChanged_ = true;
    emit selectedBoxChanged();
    emit stateChanged();
    return;
  }
  emit documentChanged();
  emit selectedBoxChanged();
  emit stateChanged();
}

bool EditorController::flushPendingDocumentChanged() {
  if (!pendingDocumentChanged_)
    return false;
  pendingDocumentChanged_ = false;
  emit documentChanged();
  emit selectedBoxChanged();
  return true;
}

void EditorController::setNotification(QString message) {
  notification_ = std::move(message);
  emit notificationChanged();
}

void EditorController::clearLoadedProjectState() {
  session_.reset();
  currentPage_.clear();
  currentPageIndex_ = -1;
  pagePaths_.clear();
  pages_.clear();
  pageTexts_.clear();
  pageTextPositions_.clear();
}

void EditorController::resetToEmptyDocumentState() {
  boxesModel_.beginResetBoxes();
  document_.clear();
  boxesModel_.endResetBoxes();
  projectPresets_.clear();
  selectedIndex_ = -1;
  selectedPresetIndex_ = -1;
}

void EditorController::prepareProjectDocumentState() {
  boxesModel_.beginResetBoxes();
  document_.clear();
  boxesModel_.endResetBoxes();
  selectedIndex_ = -1;
  selectedPresetIndex_ = -1;
  currentPage_.clear();
  currentPageIndex_ = -1;
}

void EditorController::applyLoadedPageDocument(int index,
                                               std::filesystem::path page,
                                               DocumentModel document) {
  currentPageIndex_ = index;
  currentPage_ = std::move(page);
  boxesModel_.beginResetBoxes();
  document_ = std::move(document);
  projectPresets_.clear();
  selectedIndex_ = -1;
  editingText_ = false;
  pendingDocumentChanged_ = false;
  boxesModel_.endResetBoxes();
}

void EditorController::clearProjectState() {
  clearLoadedProjectState();
  resetToEmptyDocumentState();
  emit selectionChanged();
  emit selectedBoxChanged();
  emit pageTextsChanged();
  emit documentChanged();
  emit stateChanged();
}

bool EditorController::loadPageAt(int index) {
  if (!session_ ||
      ProjectPageLoadWorkflow::normalizeRequestedIndex(index, pagePaths_) < 0)
    return false;
  if (!autosaveCurrent())
    return false;

  auto result = ProjectPageLoadWorkflow::load(&session_->documentStore(), index,
                                              pagePaths_);
  if (!result.success) {
    if (!result.error.isEmpty())
      setNotification(result.error);
    return false;
  }

  applyLoadedPageDocument(result.index, std::move(result.page),
                          std::move(result.document));
  reloadPresets();
  emit selectionChanged();
  emit selectedBoxChanged();
  emit pageTextsChanged();
  emit documentChanged();
  emit stateChanged();
  setNotification(QStringLiteral("Page %1 of %2")
                      .arg(currentPageIndex_ + 1)
                      .arg(pageCount()));
  return true;
}

bool EditorController::autosaveCurrent() {
  if (!session_ || currentPage_.empty())
    return true;
  const bool wasDirty = document_.dirty();
  const auto result = ProjectPersistenceWorkflow::autosave(
      &session_->documentStore(), currentPage_, document_);
  if (!result.success) {
    setNotification(result.error);
    return false;
  }
  if (wasDirty && !document_.dirty())
    emit stateChanged();
  return true;
}

std::string EditorController::currentPageKey() const {
  return ProjectSessionService::pageKey(currentPage_);
}

PageTextWorkflowContext EditorController::pageTextWorkflowContext() {
  return {document_.textBoxes(), selectedIndex_, pageTexts_, pageTextPositions_,
          currentPageKey()};
}

PresetWorkflowContext EditorController::presetWorkflowContext() {
  return {selectedBox(), projectPresets_, document_.presets(),
          selectedPresetIndex_};
}

bool EditorController::saveProjectPresets(const std::string &preferredName) {
  if (!session_)
    return false;
  const auto result = ProjectPersistenceWorkflow::savePresets(
      &session_->presetStore(), projectPresets_);
  if (!result.success) {
    setNotification(result.error);
    return false;
  }
  reloadPresets(preferredName);
  return true;
}

bool EditorController::reloadPresets(const std::string &preferredName) {
  std::string currentName = preferredName;
  if (currentName.empty() && selectedPresetIndex_ >= 0 &&
      selectedPresetIndex_ < static_cast<int>(document_.presets().size())) {
    currentName = document_.presets()
                      .at(static_cast<std::size_t>(selectedPresetIndex_))
                      .name;
  }
  document_.presets().clear();
  if (session_) {
    const auto result = ProjectPersistenceWorkflow::loadPresets(
        &session_->presetStore(), document_, projectPresets_);
    if (!result.success) {
      setNotification(result.error);
      return false;
    }
  }
  selectedPresetIndex_ = document_.presets().empty() ? -1 : 0;
  for (int i = 0; i < static_cast<int>(document_.presets().size()); ++i) {
    if (document_.presets().at(static_cast<std::size_t>(i)).name == currentName)
      selectedPresetIndex_ = i;
  }
  emit documentChanged();
  return true;
}

} // namespace textfx
