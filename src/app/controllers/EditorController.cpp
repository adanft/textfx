#include "app/controllers/EditorController.h"

#include "app/viewmodels/EditorViewModels.h"
#include "app/project/ProjectCreationWorkflow.h"
#include "app/project/ProjectOpenWorkflow.h"
#include "app/project/ProjectSaveExportWorkflow.h"
#include "app/project/ProjectSession.h"
#include "application/queries/SelectionQueryService.h"
#include "application/services/TextBoxSelectionService.h"
#include "app/controllers/EditorControllerStringUtils.h"
#include "domain/AuthoringLimits.h"

#include <QUrl>

#include <algorithm>
#include <cmath>
#include <utility>

namespace textfx {
namespace {
using Role = BoxesModel::Role;

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

} // namespace

EditorController::EditorController(QObject *parent)
    : QObject(parent), boxesModel_(document_, this) {}

EditorController::~EditorController() = default;

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
  const auto *box =
      SelectionQueryService::selectedBox(document_.textBoxes(), selectedIndex_);
  if (!box)
    return {};
  return EditorViewModels::textBoxMap(*box, selectedIndex_);
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
  auto result = ProjectOpenWorkflow::open(folder.toStdString());
  if (!result.success) {
    clearProjectState();
    setNotification(result.error);
    return false;
  }

  session_ = std::move(result.session);
  pageTexts_ = std::move(result.pageTexts);
  pageTextPositions_.clear();
  pagePaths_ = std::move(result.pagePaths);
  pages_ = std::move(result.pageNames);
  prepareProjectDocumentState();
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

  const auto result =
      ProjectCreationWorkflow::createStructure(folder.toStdString());
  if (!result.success) {
    setNotification(
        QStringLiteral("Could not create project: %1").arg(result.error));
    return;
  }

  openProjectInternal(folder, QStringLiteral("New project created"));
}

void EditorController::newDocument() {
  if (!autosaveCurrent())
    return;
  auto newSession = std::make_unique<ProjectSession>(std::filesystem::current_path());
  clearLoadedProjectState();
  session_ = std::move(newSession);
  resetToEmptyDocumentState();
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

void EditorController::applySaveExportResult(QString notification,
                                             bool shouldEmitStateChanged,
                                             bool notifyBeforeStateChanged) {
  if (notifyBeforeStateChanged)
    setNotification(notification);
  if (shouldEmitStateChanged)
    emit stateChanged();
  if (!notifyBeforeStateChanged)
    setNotification(notification);
}

void EditorController::save() {
  const auto result = ProjectSaveExportWorkflow::saveCurrent(
      session_.get(), document_, currentPage_);
  applySaveExportResult(
      result.notification, result.stateChanged,
      result.notificationOrder == SaveNotificationOrder::BeforeStateChanged);
}

void EditorController::saveAll() {
  const auto result = ProjectSaveExportWorkflow::saveAll(
      session_.get(), document_, currentPage_, pagePaths_);
  applySaveExportResult(
      result.notification, result.stateChanged,
      result.notificationOrder == SaveNotificationOrder::BeforeStateChanged);
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
