#include "app/EditorController.h"

#include "app/EditorViewModels.h"
#include "app/ProjectExportService.h"
#include "application/services/ProjectSessionService.h"
#include "app/SelectionQueryService.h"
#include "application/services/TextBoxSelectionService.h"
#include "app/EditorControllerStringUtils.h"
#include "domain/AuthoringLimits.h"
#include "render/RenderGraph.h"

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
  return SelectionQueryService::selectedBoxViewModel(document_.textBoxes(),
                                                    selectedIndex_);
}

QVariantList EditorController::layers() const {
  return EditorViewModels::layerList(document_.textBoxes());
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
  if (w < MinBoxSize || h < MinBoxSize)
    return;
  if (!hasProject()) {
    setNotification(QStringLiteral("Open a project before creating text"));
    return;
  }
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
  return SelectionQueryService::rolesAffectSelectedBoxState(
      roles, boxesModel_.roleNames());
}

} // namespace textfx
