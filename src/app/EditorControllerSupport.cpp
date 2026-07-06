#include "app/EditorController.h"

#include "app/EditorControllerStringUtils.h"
#include "app/EffectMetadata.h"
#include "app/ProjectSessionService.h"
#include "app/SelectionQueryService.h"
#include "app/TextWorkflowService.h"

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

void EditorController::refreshPages() {
  pages_.clear();
  pagePaths_.clear();
  if (!store_) {
    return;
  }
  const auto pages = ProjectSessionService::discoverPages(*store_);
  pagePaths_ = pages.paths;
  pages_ = pages.names;
}

void EditorController::clearProjectState() {
  store_.reset();
  currentPage_.clear();
  currentPageIndex_ = -1;
  pagePaths_.clear();
  pages_.clear();
  pageTexts_.clear();
  pageTextPositions_.clear();
  boxesModel_.beginResetBoxes();
  document_.clear();
  boxesModel_.endResetBoxes();
  projectPresets_.clear();
  selectedIndex_ = -1;
  selectedPresetIndex_ = -1;
  emit selectionChanged();
  emit selectedBoxChanged();
  emit pageTextsChanged();
  emit documentChanged();
  emit stateChanged();
}

bool EditorController::loadPageAt(int index) {
  if (!store_ ||
      ProjectSessionService::normalizePageIndex(index, pagePaths_.size()) < 0)
    return false;
  if (!autosaveCurrent())
    return false;

  const auto nextPage = pagePaths_.at(static_cast<std::size_t>(index));
  DocumentModel nextDocument;
  std::string error;
  if (!store_->loadPage(nextPage, nextDocument, &error)) {
    setNotification(toQString(error));
    return false;
  }

  currentPageIndex_ = index;
  currentPage_ = nextPage;
  boxesModel_.beginResetBoxes();
  document_ = std::move(nextDocument);
  projectPresets_.clear();
  selectedIndex_ = -1;
  editingText_ = false;
  pendingDocumentChanged_ = false;
  boxesModel_.endResetBoxes();
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
  if (!store_ || currentPage_.empty())
    return true;
  const bool wasDirty = document_.dirty();
  std::string error;
  if (store_->autosave(currentPage_, document_, &error)) {
    if (wasDirty && !document_.dirty())
      emit stateChanged();
    return true;
  }
  setNotification(toQString(error));
  return false;
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
  if (!store_)
    return false;
  std::string error;
  if (!store_->savePresets(projectPresets_, &error)) {
    setNotification(toQString(error));
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
  if (store_) {
    std::string error;
    if (!store_->loadPresets(document_, projectPresets_, &error)) {
      setNotification(toQString(error));
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
