#include "app/EditorController.h"

#include "app/EditorViewModels.h"
#include "app/PageTextService.h"
#include "app/ProjectExportService.h"
#include "app/ProjectSessionService.h"
#include "app/TextBoxClipboardService.h"
#include "app/TextBoxEditingService.h"
#include "app/TextBoxSelectionService.h"
#include "app/TextPresetService.h"
#include "render/RenderGraph.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

#include <fstream>
#include <utility>

namespace textfx {
namespace {
QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}
std::string toStdString(const QString &value) { return value.toStdString(); }

bool ensureRegularFile(const std::filesystem::path &path, std::string *error) {
  if (std::filesystem::exists(path) &&
      !std::filesystem::is_regular_file(path)) {
    if (error)
      *error = path.filename().string() + " must be a regular file";
    return false;
  }

  std::ofstream file(path, std::ios::app);
  if (!file.is_open()) {
    if (error)
      *error = "could not open " + path.filename().string();
    return false;
  }
  file.close();

  if (!std::filesystem::is_regular_file(path)) {
    if (error)
      *error = path.filename().string() + " must be a regular file";
    return false;
  }

  std::ifstream readable(path);
  if (!readable.is_open()) {
    if (error)
      *error = "could not read " + path.filename().string();
    return false;
  }

  return true;
}
} // namespace

EditorController::EditorController(QObject *parent) : QObject(parent) {}

QVariantList EditorController::boxes() const {
  return EditorViewModels::textBoxList(document_.textBoxes());
}

QVariantList EditorController::layers() const {
  return EditorViewModels::layerList(document_.textBoxes());
}

QString EditorController::currentPageName() const {
  return ProjectSessionService::pageName(pages_, currentPageIndex_);
}

QUrl EditorController::currentPageUrl() const {
  return currentPage_.empty() ? QUrl{}
                              : QUrl::fromLocalFile(QString::fromStdString(
                                    currentPage_.string()));
}

QUrl EditorController::rawPageUrl() const {
  if (!store_ || currentPage_.empty())
    return {};
  const auto raw = store_->rawPagePathFor(currentPage_);
  return raw.empty()
             ? QUrl{}
             : QUrl::fromLocalFile(QString::fromStdString(raw.string()));
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
  if (command == QStringLiteral("new") || command == QStringLiteral("open")) {
    return true;
  }
  if (command == QStringLiteral("paste")) {
    return hasProject();
  }
  if (command == QStringLiteral("copy") ||
      command == QStringLiteral("delete") ||
      command == QStringLiteral("duplicate")) {
    return selectedBox() != nullptr;
  }
  return hasProject();
}

void EditorController::openProject(const QString &folder) {
  openProjectInternal(folder, QStringLiteral("Project opened"));
}

bool EditorController::openProjectInternal(const QString &folder,
                                           const QString &successNotification) {
  if (!autosaveCurrent())
    return false;
  store_ = std::make_unique<ProjectStore>(folder.toStdString());
  std::string error;
  pageTexts_ = store_->loadPageTexts(&error);
  pageTextPositions_.clear();
  if (!error.empty()) {
    clearProjectState();
    setNotification(toQString(error));
    return false;
  }
  refreshPages();
  document_.clear();
  selectedIndex_ = -1;
  selectedPresetIndex_ = -1;
  currentPage_.clear();
  currentPageIndex_ = -1;
  if (!pagePaths_.empty()) {
    if (!loadPageAt(0)) {
      const QString loadError = notification_;
      clearProjectState();
      setNotification(loadError);
      return false;
    }
  } else if (!reloadPresets()) {
    const QString loadError = notification_;
    clearProjectState();
    setNotification(loadError);
    return false;
  }
  emit selectionChanged();
  emit pageTextsChanged();
  emit documentChanged();
  emit stateChanged();
  setNotification(hasProject() ? successNotification
                               : QStringLiteral("No project open"));
  return hasProject();
}

void EditorController::openProjectUrl(const QUrl &folderUrl) {
  const QString folder = folderUrl.toLocalFile();
  if (folder.isEmpty()) {
    setNotification(
        QStringLiteral("Only local project folders can be opened."));
    return;
  }
  openProject(folder);
}

void EditorController::newProjectUrl(const QUrl &folderUrl) {
  const QString folder = folderUrl.toLocalFile();
  if (folder.isEmpty()) {
    setNotification(
        QStringLiteral("Only local project folders can be created."));
    return;
  }
  newProject(folder);
}

void EditorController::newProject(const QString &folder) {
  if (!autosaveCurrent())
    return;

  const std::filesystem::path root = folder.toStdString();
  try {
    std::filesystem::create_directories(root / ProjectStore::CleanedFolder);
    std::filesystem::create_directories(root / ProjectStore::RawFolder);
    std::filesystem::create_directories(root / ProjectStore::ExportFolder);
    std::string error;
    if (!ensureRegularFile(root / ProjectStore::PageTextsFile, &error)) {
      setNotification(
          QStringLiteral("Could not create project: %1").arg(toQString(error)));
      return;
    }
  } catch (const std::filesystem::filesystem_error &ex) {
    setNotification(QStringLiteral("Could not create project: %1")
                        .arg(toQString(ex.what())));
    return;
  }

  openProjectInternal(folder, QStringLiteral("New project created"));
}

void EditorController::newDocument() {
  if (!autosaveCurrent())
    return;
  store_ = std::make_unique<ProjectStore>(std::filesystem::current_path());
  currentPage_.clear();
  currentPageIndex_ = -1;
  pagePaths_.clear();
  pages_.clear();
  pageTexts_.clear();
  pageTextPositions_.clear();
  document_.clear();
  projectPresets_.clear();
  reloadPresets();
  selectedIndex_ = -1;
  selectedPresetIndex_ = document_.presets().empty() ? -1 : 0;
  emit selectionChanged();
  emit pageTextsChanged();
  emit documentChanged();
  emit stateChanged();
  setNotification(QStringLiteral("New document"));
}

void EditorController::save() {
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before saving"));
    return;
  }
  if (currentPage_.empty()) {
    document_.markSaved();
    emit stateChanged();
    setNotification(QStringLiteral("Document marked saved"));
    return;
  }
  std::string error;
  if (!store_->savePage(currentPage_, document_, &error)) {
    setNotification(
        QStringLiteral("Could not save boxes: %1").arg(toQString(error)));
    emit stateChanged();
    return;
  }

  document_.markSaved();
  const RenderGraph graph;
  const auto exportPath = store_->pageExportPathFor(currentPage_);
  if (graph.exportPagePng(document_, currentPage_, exportPath, &error)) {
    setNotification(QStringLiteral("Saved boxes and exported PNG to %1.")
                        .arg(QString::fromStdString(exportPath.string())));
  } else {
    setNotification(QStringLiteral("Saved boxes, but could not export PNG: %1")
                        .arg(toQString(error)));
  }
  emit stateChanged();
}

void EditorController::saveAll() {
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before saving"));
    return;
  }
  if (currentPage_.empty()) {
    save();
    return;
  }

  std::string error;
  if (store_->savePage(currentPage_, document_, &error)) {
    document_.markSaved();
  } else {
    setNotification(
        QStringLiteral("Could not save current page; Save All stopped: %1")
            .arg(toQString(error)));
    emit stateChanged();
    return;
  }

  const ProjectExportService exportService(*store_);
  const auto exportResult = exportService.exportPages(ExportJob{
      .pagePaths = pagePaths_,
      .currentPage = currentPage_,
      .currentDocument = &document_,
  });

  if (exportResult.failed > 0) {
    setNotification(QStringLiteral("Exported %1 page(s), %2 failed.")
                        .arg(exportResult.completed)
                        .arg(exportResult.failed));
  } else {
    setNotification(
        QStringLiteral("Exported %1 page(s).").arg(exportResult.completed));
  }
  emit stateChanged();
}

void EditorController::previousPage() {
  if (canGoPrevious())
    loadPageAt(currentPageIndex_ - 1);
}

void EditorController::nextPage() {
  if (canGoNext())
    loadPageAt(currentPageIndex_ + 1);
}

void EditorController::goToPage(int index) {
  if (index != currentPageIndex_)
    loadPageAt(index);
}

void EditorController::selectBox(int index) {
  selectedIndex_ =
      TextBoxSelectionService::normalizedIndex(document_.textBoxes(), index);
  emit selectionChanged();
  emit stateChanged();
}

void EditorController::createTextBox(double x, double y, double w, double h) {
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before creating text"));
    return;
  }
  if (w < 12.0 || h < 12.0)
    return;
  TextBox box;
  box.bounds = {x, y, w, h};
  document_.addTextBox(std::move(box));
  selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
  markDocumentChanged();
  emit selectionChanged();
}

void EditorController::updateSelectedText(const QString &text) {
  editSelectedBoxIf([&](TextBox &box) {
    box.text = toStdString(text);
    if (editingText_) {
      document_.setDirty(true);
      pendingDocumentChanged_ = true;
      emit stateChanged();
      return false;
    }
    return true;
  });
}

void EditorController::setSelectedFontFamily(const QString &family) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setFontFamily(box, family); });
}

void EditorController::setSelectedFontSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setFontSize(box, size); });
}

void EditorController::setSelectedTextColor(const QString &color) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setTextColor(box, color); });
}

void EditorController::setSelectedBold(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBold(box, enabled); });
}

void EditorController::setSelectedItalic(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setItalic(box, enabled); });
}

void EditorController::setSelectedUppercase(bool enabled) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setUppercase(box, enabled); });
}

void EditorController::setSelectedAlignment(int alignment) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setAlignment(box, alignment);
  });
}

void EditorController::setSelectedLineSpacing(int spacing) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setLineSpacing(box, spacing);
  });
}

void EditorController::setSelectedLetterSpacing(int spacing) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setLetterSpacing(box, spacing);
  });
}

void EditorController::moveSelected(double dx, double dy) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::move(box, dx, dy); });
}

void EditorController::resizeSelected(double dw, double dh) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::resize(box, dw, dh); });
}

void EditorController::setSelectedBounds(double x, double y, double w,
                                         double h) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBounds(box, x, y, w, h); });
}

void EditorController::rotateSelected(double degrees) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::rotate(box, degrees); });
}

void EditorController::setSelectedRotation(double degrees) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setRotation(box, degrees); });
}

void EditorController::setSelectedOutlineEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setOutlineEnabled(box, enabled);
  });
}

void EditorController::setSelectedOutlineColor(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setOutlineColor(box, color);
  });
}

void EditorController::setSelectedOutlineSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setOutlineSize(box, size); });
}

void EditorController::setSelectedBlurEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setBlurEnabled(box, enabled);
  });
}

void EditorController::setSelectedBlurSize(int size) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setBlurSize(box, size); });
}

void EditorController::setSelectedShadowEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowEnabled(box, enabled);
  });
}

void EditorController::setSelectedShadowColor(const QString &color) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setShadowColor(box, color); });
}

void EditorController::setSelectedShadowOffsetX(int offset) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowOffsetX(box, offset);
  });
}

void EditorController::setSelectedShadowOffsetY(int offset) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowOffsetY(box, offset);
  });
}

void EditorController::setSelectedShadowBlurSize(int size) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setShadowBlurSize(box, size);
  });
}

void EditorController::setSelectedGradientEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientEnabled(box, enabled);
  });
}

void EditorController::setSelectedGradientDirection(int direction) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientDirection(box, direction);
  });
}

void EditorController::setSelectedGradientColorA(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientColorA(box, color);
  });
}

void EditorController::setSelectedGradientColorB(const QString &color) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setGradientColorB(box, color);
  });
}

void EditorController::setSelectedPerspectiveEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPerspectiveEnabled(box, enabled);
  });
}

void EditorController::setSelectedPathEnabled(bool enabled) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPathEnabled(box, enabled);
  });
}

void EditorController::setSelectedPathMode(int mode) {
  editSelectedBox(
      [&](TextBox &box) { TextBoxEditingService::setPathMode(box, mode); });
}

void EditorController::resetSelectedPerspective() {
  editSelectedBox(
      [](TextBox &box) { TextBoxEditingService::resetPerspective(box); });
}

void EditorController::resetSelectedPath() {
  editSelectedBox([](TextBox &box) { TextBoxEditingService::resetPath(box); });
}

void EditorController::addSelectedPathPoint() {
  editSelectedBox(
      [](TextBox &box) { TextBoxEditingService::addPathPoint(box); });
}

void EditorController::setPerspectiveHandle(const QString &corner, double x,
                                            double y) {
  editSelectedBox([&](TextBox &box) {
    TextBoxEditingService::setPerspectiveHandle(box, corner, x, y);
  });
}

void EditorController::setPathHandle(int index, double x, double y) {
  editSelectedBoxIf([&](TextBox &box) {
    return TextBoxEditingService::setPathHandle(box, index, x, y);
  });
}

bool EditorController::leftMouseButtonDown() const {
  return QGuiApplication::mouseButtons().testFlag(Qt::LeftButton);
}

void EditorController::duplicateSelected() {
  if (TextBoxSelectionService::duplicateSelected(document_.textBoxes(),
                                                 selectedIndex_)) {
    markDocumentChanged();
    emit selectionChanged();
  }
}

void EditorController::deleteSelected() {
  if (TextBoxSelectionService::deleteSelected(document_.textBoxes(),
                                              selectedIndex_)) {
    markDocumentChanged();
    emit selectionChanged();
  }
}

void EditorController::moveLayer(int to) {
  if (TextBoxSelectionService::moveLayer(document_.textBoxes(), selectedIndex_,
                                         to)) {
    markDocumentChanged();
    emit selectionChanged();
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
  document_.addTextBox(std::move(box));
  selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
  markDocumentChanged();
  emit selectionChanged();
}

bool EditorController::applyPageText(int index) {
  const auto key = currentPageKey();
  const auto result =
      PageTextService::applyText(document_.textBoxes(), selectedIndex_,
                                 pageTexts_, pageTextPositions_, key, index);
  switch (result) {
  case PageTextApplyStatus::Applied:
    markDocumentChanged();
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
  const auto key = currentPageKey();
  const int index = PageTextService::nextTextIndex(pageTextPositions_, key);
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
  if (!selectedBox()) {
    setNotification(QStringLiteral("Select a box before applying a preset"));
    return false;
  }
  if (selectedPresetIndex_ < 0 ||
      selectedPresetIndex_ >= static_cast<int>(document_.presets().size())) {
    setNotification(QStringLiteral("Select a text preset first"));
    return false;
  }
  if (!TextPresetService::applySelectedPreset(
          *selectedBox(), document_.presets(), selectedPresetIndex_))
    return false;
  markDocumentChanged();
  return true;
}

bool EditorController::addPreset(const QString &name) {
  if (!selectedBox()) {
    setNotification(QStringLiteral("Select a box before saving a preset"));
    return false;
  }
  std::string preferredName;
  if (!TextPresetService::addPreset(projectPresets_, *selectedBox(), name,
                                    preferredName))
    return false;
  return saveProjectPresets(preferredName);
}

bool EditorController::updateSelectedPreset() {
  if (!selectedBox()) {
    setNotification(QStringLiteral("Select a box before updating a preset"));
    return false;
  }
  std::string preferredName;
  if (!TextPresetService::updateSelectedPreset(
          projectPresets_, document_.presets(), selectedPresetIndex_,
          *selectedBox(), preferredName))
    return false;
  return saveProjectPresets(preferredName);
}

bool EditorController::renameSelectedPreset(const QString &name) {
  std::string preferredName;
  if (!TextPresetService::renameSelectedPreset(
          projectPresets_, document_.presets(), selectedPresetIndex_, name,
          preferredName))
    return false;
  return saveProjectPresets(preferredName);
}

bool EditorController::deleteSelectedPreset() {
  if (!TextPresetService::deleteSelectedPreset(
          projectPresets_, document_.presets(), selectedPresetIndex_))
    return false;
  return saveProjectPresets();
}

void EditorController::beginInteraction() { ++interactionDepth_; }

void EditorController::endInteraction() {
  if (interactionDepth_ <= 0)
    return;
  --interactionDepth_;
  if (interactionDepth_ == 0 && pendingDocumentChanged_) {
    pendingDocumentChanged_ = false;
    emit documentChanged();
    emit stateChanged();
  }
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
  editingText_ = false;
  emit stateChanged();
  if (wasEditing)
    endInteraction();
}

void EditorController::applyTextLineSpacing(QObject *quickTextDocument,
                                            double spacing) {
  if (!quickTextDocument)
    return;
  auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
  auto *document = wrapper ? wrapper->textDocument() : nullptr;
  if (!document)
    return;

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
  return const_cast<TextBox *>(std::as_const(*this).selectedBox());
}

const TextBox *EditorController::selectedBox() const {
  if (selectedIndex_ < 0 ||
      selectedIndex_ >= static_cast<int>(document_.textBoxes().size())) {
    return nullptr;
  }
  return &document_.textBoxes()[static_cast<std::size_t>(selectedIndex_)];
}

bool EditorController::editSelectedBoxIf(
    const std::function<bool(TextBox &)> &mutation) {
  auto *box = selectedBox();
  if (!box || !mutation(*box))
    return false;
  markDocumentChanged();
  return true;
}

void EditorController::editSelectedBox(
    const std::function<void(TextBox &)> &mutation) {
  editSelectedBoxIf([&](TextBox &box) {
    mutation(box);
    return true;
  });
}

void EditorController::markDocumentChanged() {
  document_.setDirty(true);
  if (interactionDepth_ > 0) {
    pendingDocumentChanged_ = true;
    emit documentChanged();
    emit stateChanged();
    return;
  }
  emit documentChanged();
  emit stateChanged();
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
  document_.clear();
  projectPresets_.clear();
  selectedIndex_ = -1;
  selectedPresetIndex_ = -1;
  emit selectionChanged();
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

  currentPageIndex_ = index;
  currentPage_ = pagePaths_.at(static_cast<std::size_t>(index));
  document_.clear();
  projectPresets_.clear();
  selectedIndex_ = -1;
  editingText_ = false;
  pendingDocumentChanged_ = false;
  std::string error;
  if (!store_->loadPage(currentPage_, document_, &error)) {
    setNotification(toQString(error));
    return false;
  }
  reloadPresets();
  emit selectionChanged();
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
  std::string error;
  if (store_->autosave(currentPage_, document_, &error))
    return true;
  setNotification(toQString(error));
  return false;
}

std::string EditorController::currentPageKey() const {
  return ProjectSessionService::pageKey(currentPage_);
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
