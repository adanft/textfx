#include "app/project/ProjectSaveExportWorkflow.h"

#include "application/services/ProjectExportService.h"
#include "domain/document/DocumentModel.h"
#include "infrastructure/persistence/ProjectStore.h"
#include "render/RenderGraph.h"

namespace textfx {
namespace {
QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}
} // namespace

SaveCurrentProjectResult ProjectSaveExportWorkflow::saveCurrent(
    ProjectStore *store, DocumentModel &document,
    const std::filesystem::path &currentPage) {
  if (store == nullptr) {
    return {.notification = QStringLiteral("Open a project before saving")};
  }
  if (currentPage.empty()) {
    document.markSaved();
    return {.notification = QStringLiteral("Document marked saved"),
            .stateChanged = true,
            .notificationOrder = SaveNotificationOrder::AfterStateChanged};
  }

  std::string error;
  if (!store->savePage(currentPage, document, &error)) {
    return {.notification = QStringLiteral("Could not save boxes: %1")
                            .arg(toQString(error)),
            .stateChanged = true};
  }

  document.markSaved();
  const RenderGraph graph;
  const auto exportPath = store->pageExportPathFor(currentPage);
  if (graph.exportPagePng(document, currentPage, exportPath, &error)) {
    return {.notification = QStringLiteral("Saved boxes and exported PNG to %1.")
                            .arg(QString::fromStdString(exportPath.string())),
            .stateChanged = true};
  }

  return {.notification = QStringLiteral("Saved boxes, but could not export PNG: %1")
                          .arg(toQString(error)),
          .stateChanged = true};
}

SaveAllProjectResult ProjectSaveExportWorkflow::saveAll(
    ProjectStore *store, DocumentModel &document,
    const std::filesystem::path &currentPage,
    const std::vector<std::filesystem::path> &pagePaths) {
  if (store == nullptr) {
    return {.notification = QStringLiteral("Open a project before saving")};
  }
  if (currentPage.empty()) {
    const auto saveResult = saveCurrent(store, document, currentPage);
    return saveResult;
  }

  std::string error;
  if (store->savePage(currentPage, document, &error)) {
    document.markSaved();
  } else {
    return {.notification = QStringLiteral(
                "Could not save current page; Save All stopped: %1")
                            .arg(toQString(error)),
            .stateChanged = true};
  }

  const RenderGraph graph;
  const ProjectExportService exportService(*store, graph);
  const auto exportResult = exportService.exportPages(ExportJob{
      .pagePaths = pagePaths,
      .currentPage = currentPage,
      .currentDocument = &document,
  });

  if (exportResult.failed > 0) {
    return {.notification = QStringLiteral("Exported %1 page(s), %2 failed.")
                            .arg(exportResult.completed)
                            .arg(exportResult.failed),
            .stateChanged = true};
  }
  return {.notification = QStringLiteral("Exported %1 page(s).")
                          .arg(exportResult.completed),
          .stateChanged = true};
}

} // namespace textfx
