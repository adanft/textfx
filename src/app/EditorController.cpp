#include "app/EditorController.h"

#include "app/EditorViewModels.h"
#include "app/EffectMetadata.h"
#include "app/PageTextService.h"
#include "app/ProjectExportService.h"
#include "app/ProjectSessionService.h"
#include "app/TextBoxClipboardService.h"
#include "app/TextBoxEditingService.h"
#include "app/TextBoxSelectionService.h"
#include "app/TextPresetService.h"
#include "core/AuthoringLimits.h"
#include "render/RenderGraph.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <utility>

namespace textfx {
namespace {
using Role = BoxesModel::Role;

#ifdef TEXTFX_ENABLE_TEST_HOOKS
std::optional<std::filesystem::path> projectFileReadFailurePath;

std::filesystem::path comparablePath(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? path.lexically_normal() : canonical;
}
#endif

QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}
std::string toStdString(const QString &value) { return value.toStdString(); }

std::string normalizedPaintColor(QString color) {
  if (color.startsWith(QStringLiteral("#")))
    color = color.sliced(1);
  if (color.size() == 6)
    color.append(QStringLiteral("ff"));
  if (color.size() != 8)
    return "000000ff";
  color = color.toLower();
  for (const QChar ch : color) {
    if (!ch.isDigit() && (ch < QLatin1Char('a') || ch > QLatin1Char('f')))
      return "000000ff";
  }
  return color.toStdString();
}

QVariantMap paintStrokeMap(const PaintStroke &stroke) {
  QVariantList points;
  for (const auto &point : stroke.points)
    points.append(QVariant::fromValue(QVariantList{point.x, point.y}));
  return {{"color", QString::fromStdString(stroke.color)},
          {"size", stroke.size},
          {"opacity", stroke.opacity},
          {"points", points}};
}

QVariantList paintStrokeList(const std::vector<PaintStroke> &strokes) {
  QVariantList result;
  for (const auto &stroke : strokes)
    result.append(paintStrokeMap(stroke));
  return result;
}

std::vector<PaintStroke> *paintLayer(DocumentModel &document,
                                     const QString &target) {
  if (target == QStringLiteral("above_text"))
    return &document.paint().aboveText;
  if (target == QStringLiteral("behind_text"))
    return &document.paint().behindText;
  return nullptr;
}

std::vector<Point> pointsFromVariantList(const QVariantList &values) {
  std::vector<Point> result;
  for (const auto &value : values) {
    const QVariantList point = value.toList();
    if (point.size() < 2)
      continue;
    result.push_back({point.at(0).toDouble(), point.at(1).toDouble()});
  }
  return result;
}

double distanceToSegment(Point point, Point a, Point b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  if (dx == 0.0 && dy == 0.0)
    return std::hypot(point.x - a.x, point.y - a.y);
  const double t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) /
                                  (dx * dx + dy * dy),
                              0.0, 1.0);
  return std::hypot(point.x - (a.x + t * dx), point.y - (a.y + t * dy));
}

bool strokeIntersectsEraser(const PaintStroke &stroke, Point center,
                            double radius) {
  const double hitRadius = std::max(0.0, radius) + std::max(0.0, stroke.size) / 2.0;
  for (const auto &point : stroke.points) {
    if (std::hypot(point.x - center.x, point.y - center.y) <= hitRadius)
      return true;
  }
  for (std::size_t i = 1; i < stroke.points.size(); ++i) {
    if (distanceToSegment(center, stroke.points[i - 1], stroke.points[i]) <=
        hitRadius)
      return true;
  }
  return false;
}

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

#ifdef TEXTFX_ENABLE_TEST_HOOKS
  if (projectFileReadFailurePath &&
      comparablePath(path) == *projectFileReadFailurePath) {
    if (error)
      *error = "could not read " + path.filename().string();
    return false;
  }
#endif

  std::ifstream readable(path);
  if (!readable.is_open()) {
    if (error)
      *error = "could not read " + path.filename().string();
    return false;
  }

  return true;
}

} // namespace

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace test_hooks {
void failProjectFileRead(std::filesystem::path path) {
  projectFileReadFailurePath = comparablePath(path);
}

void clearProjectFileReadFailure() { projectFileReadFailurePath.reset(); }
} // namespace test_hooks
#endif

EditorController::EditorController(QObject *parent)
    : QObject(parent), boxesModel_(document_, this) {}

QVariantList EditorController::boxes() const {
  return EditorViewModels::textBoxList(document_.textBoxes());
}

QVariantList EditorController::paintBehindText() const {
  return paintStrokeList(document_.paint().behindText);
}

QVariantList EditorController::paintAboveText() const {
  return paintStrokeList(document_.paint().aboveText);
}

int EditorController::boxCount() const {
  return static_cast<int>(document_.textBoxes().size());
}

QAbstractListModel *EditorController::boxesModel() { return &boxesModel_; }

QVariant EditorController::selectedBoxViewModel() const {
  const auto *box = selectedBox();
  if (!box)
    return {};
  return EditorViewModels::textBoxMap(*box, selectedIndex_);
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
  boxesModel_.beginResetBoxes();
  document_.clear();
  boxesModel_.endResetBoxes();
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
  emit selectedBoxChanged();
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
  boxesModel_.beginResetBoxes();
  document_.clear();
  boxesModel_.endResetBoxes();
  projectPresets_.clear();
  reloadPresets();
  selectedIndex_ = -1;
  selectedPresetIndex_ = document_.presets().empty() ? -1 : 0;
  emit selectionChanged();
  emit selectedBoxChanged();
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
  const int normalizedIndex =
      TextBoxSelectionService::normalizedIndex(document_.textBoxes(), index);
  if (normalizedIndex == selectedIndex_)
    return;
  selectedIndex_ = normalizedIndex;
  emit selectionChanged();
  emit selectedBoxChanged();
  emit stateChanged();
}

void EditorController::createTextBox(double x, double y, double w, double h) {
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before creating text"));
    return;
  }
  if (w < MinBoxSize || h < MinBoxSize)
    return;
  TextBox box;
  box.bounds = {x, y, w, h};
  const int row = static_cast<int>(document_.textBoxes().size());
  boxesModel_.beginInsertBox(row);
  document_.addTextBox(std::move(box));
  boxesModel_.endInsertBox();
  selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
  markDocumentChanged();
  emit selectionChanged();
}

void EditorController::addPaintStroke(const QString &target, const QString &color,
                                      double size, double opacity,
                                      const QVariantList &points) {
  if (!hasProject())
    return;
  auto *layer = paintLayer(document_, target);
  if (!layer)
    return;
  PaintStroke stroke;
  stroke.color = normalizedPaintColor(color);
  stroke.size = std::max(1.0, size);
  stroke.opacity = std::clamp(opacity, 0.0, 1.0);
  stroke.points = pointsFromVariantList(points);
  if (stroke.points.empty())
    return;
  layer->push_back(std::move(stroke));
  markDocumentChanged();
}

void EditorController::erasePaintAt(const QString &target, double x, double y,
                                    double radius) {
  if (!hasProject())
    return;
  auto *layer = paintLayer(document_, target);
  if (!layer)
    return;
  const auto oldSize = layer->size();
  std::erase_if(*layer, [&](const PaintStroke &stroke) {
    return strokeIntersectsEraser(stroke, {x, y}, radius);
  });
  if (layer->size() != oldSize)
    markDocumentChanged();
}

QVariant EditorController::boxRole(int row, const QString &roleName) const {
  if (row < 0 || row >= boxCount())
    return {};
  const auto roles = boxesModel_.roleNames();
  const QByteArray requested = roleName.toUtf8();
  for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
    if (it.value() == requested)
      return boxesModel_.data(boxesModel_.index(row, 0), it.key());
  }
  return {};
}

bool EditorController::boxRolesAffectSelectedBoxState(
    const QVariantList &roles) const {
  if (roles.empty())
    return true;

  const auto knownRoles = boxesModel_.roleNames();
  for (const QVariant &roleValue : roles) {
    bool ok = false;
    const int role = roleValue.toInt(&ok);
    if (!ok || !knownRoles.contains(role))
      return true;

    switch (role) {
    case Role::TextRole:
    case Role::RotationRole:
    case Role::FontFamilyRole:
    case Role::FontSizeRole:
    case Role::ColorRole:
    case Role::LineSpacingRole:
    case Role::LetterSpacingRole:
    case Role::BoldRole:
    case Role::ItalicRole:
    case Role::UppercaseRole:
    case Role::LowercaseRole:
    case Role::AlignmentRole:
    case Role::OutlineRole:
    case Role::OutlineColorRole:
    case Role::OutlineSizeRole:
    case Role::BlurRole:
    case Role::BlurSizeRole:
    case Role::ShadowRole:
    case Role::ShadowColorRole:
    case Role::ShadowOffsetXRole:
    case Role::ShadowOffsetYRole:
    case Role::ShadowBlurSizeRole:
    case Role::GradientRole:
    case Role::GradientDirectionRole:
    case Role::GradientColorARole:
    case Role::GradientColorBRole:
    case Role::PerspectiveRole:
    case Role::PerspectiveNwRole:
    case Role::PerspectiveNeRole:
    case Role::PerspectiveSeRole:
    case Role::PerspectiveSwRole:
    case Role::PathRole:
    case Role::PathModeRole:
    case Role::PathPointsRole:
    case Role::EffectsRole:
      return true;
    default:
      break;
    }
  }
  return false;
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

bool EditorController::leftMouseButtonDown() const {
  return QGuiApplication::mouseButtons().testFlag(Qt::LeftButton);
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
  const auto key = currentPageKey();
  const auto result =
      PageTextService::applyText(document_.textBoxes(), selectedIndex_,
                                 pageTexts_, pageTextPositions_, key, index);
  switch (result) {
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
  markDocumentChanged(allBoxRoles());
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
