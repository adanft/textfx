#pragma once

#include "application/ports/IPageExportRenderer.h"
#include "domain/document/DocumentModel.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace textfx {

class ProjectStore;

struct ExportPageResult {
  std::filesystem::path pagePath;
  std::filesystem::path exportPath;
  bool completed = false;
  std::string error;
  ExportTiming timing{};
};

struct ExportJobResult {
  int total = 0;
  int completed = 0;
  int failed = 0;
  std::vector<ExportPageResult> pages;
  std::chrono::steady_clock::duration elapsed{};
};

struct ExportJob {
  std::vector<std::filesystem::path> pagePaths;
  std::filesystem::path currentPage;
  const DocumentModel *currentDocument = nullptr;
};

class ProjectExportService {
public:
  ProjectExportService(const ProjectStore &store,
                       const IPageExportRenderer &renderer);

  ExportJobResult exportPages(const ExportJob &job) const;

private:
  const ProjectStore &store_;
  const IPageExportRenderer &renderer_;
};

} // namespace textfx
