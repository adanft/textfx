#include "application/services/ProjectExportService.h"

#include "infrastructure/persistence/ProjectStore.h"

namespace textfx {

ProjectExportService::ProjectExportService(const ProjectStore &store)
    : store_(store) {}

ExportJobResult ProjectExportService::exportPages(const ExportJob &job) const {
  const auto startTime = std::chrono::steady_clock::now();

  ExportJobResult result;
  result.total = static_cast<int>(job.pagePaths.size());
  result.pages.reserve(job.pagePaths.size());

  const RenderGraph graph;
  for (const auto &pagePath : job.pagePaths) {
    ExportPageResult pageResult;
    pageResult.pagePath = pagePath;
    pageResult.exportPath = store_.pageExportPathFor(pagePath);

    DocumentModel loaded;
    const DocumentModel *document = job.currentDocument;
    if (pagePath != job.currentPage) {
      std::string error;
      if (!store_.loadPage(pagePath, loaded, &error)) {
        pageResult.error = error;
        ++result.failed;
        result.pages.push_back(std::move(pageResult));
        continue;
      }
      document = &loaded;
    }

    if (document == nullptr) {
      pageResult.error =
          "No document available for export: " + pagePath.string();
      ++result.failed;
      result.pages.push_back(std::move(pageResult));
      continue;
    }

    const auto exportResult =
        graph.exportPagePngTimed(*document, pagePath, pageResult.exportPath);
    if (exportResult) {
      pageResult.completed = true;
      pageResult.timing = exportResult->timing;
      ++result.completed;
    } else {
      pageResult.error = exportResult.error();
      ++result.failed;
    }
    result.pages.push_back(std::move(pageResult));
  }

  result.elapsed = std::chrono::steady_clock::now() - startTime;
  return result;
}

} // namespace textfx
