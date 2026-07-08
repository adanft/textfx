#include "app/project/ProjectSaveExportWorkflow.h"

#include "app/project/ProjectSession.h"
#include "application/ports/IPageExportRenderer.h"
#include "application/services/ProjectExportService.h"
#include "domain/document/DocumentModel.h"

namespace textfx {
namespace {
QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}
} // namespace

SaveCurrentProjectResult ProjectSaveExportWorkflow::saveCurrent(
    const SaveCurrentProjectRequest &request) {
  if (request.session == nullptr) {
    return {.notification = QStringLiteral("Open a project before saving")};
  }
  if (request.currentPage.empty()) {
    request.document.markSaved();
    return {.notification = QStringLiteral("Document marked saved"),
            .stateChanged = true,
            .notificationOrder = SaveNotificationOrder::AfterStateChanged};
  }

  std::string error;
  if (!request.session->documentStore().savePage(request.currentPage,
                                                 request.document, &error)) {
    return {.notification = QStringLiteral("Could not save boxes: %1")
                            .arg(toQString(error)),
            .stateChanged = true};
  }

  request.document.markSaved();
  const ProjectExportService exportService(request.session->exportStore(),
                                           request.renderer);
  const auto exportResult = exportService.exportPages(ExportJob{
      .pagePaths = {request.currentPage},
      .currentPage = request.currentPage,
      .currentDocument = &request.document,
  });
  const auto &pageResult = exportResult.pages.front();
  if (pageResult.completed) {
    return {.notification = QStringLiteral("Saved boxes and exported PNG to %1.")
                            .arg(QString::fromStdString(
                                pageResult.exportPath.string())),
            .stateChanged = true};
  }

  return {.notification = QStringLiteral("Saved boxes, but could not export PNG: %1")
                          .arg(toQString(pageResult.error)),
          .stateChanged = true};
}

SaveAllProjectResult
ProjectSaveExportWorkflow::saveAll(const SaveAllProjectRequest &request) {
  if (request.session == nullptr) {
    return {.notification = QStringLiteral("Open a project before saving")};
  }
  if (request.currentPage.empty()) {
    const auto saveResult = saveCurrent(SaveCurrentProjectRequest{
        .session = request.session,
        .renderer = request.renderer,
        .document = request.document,
        .currentPage = request.currentPage,
    });
    return saveResult;
  }

  std::string error;
  if (request.session->documentStore().savePage(request.currentPage,
                                                 request.document, &error)) {
    request.document.markSaved();
  } else {
    return {.notification = QStringLiteral(
                "Could not save current page; Save All stopped: %1")
                            .arg(toQString(error)),
            .stateChanged = true};
  }

  const ProjectExportService exportService(request.session->exportStore(),
                                           request.renderer);
  const auto exportResult = exportService.exportPages(ExportJob{
      .pagePaths = request.pagePaths,
      .currentPage = request.currentPage,
      .currentDocument = &request.document,
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
