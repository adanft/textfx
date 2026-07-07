#include "app/project/ProjectOpenWorkflow.h"

#include "application/services/ProjectSessionService.h"

namespace textfx {

ProjectOpenResult ProjectOpenWorkflow::open(const std::filesystem::path &folder) {
  ProjectOpenResult result;
  result.store = std::make_unique<ProjectStore>(folder);

  std::string error;
  result.pageTexts = result.store->loadPageTexts(&error);
  if (!error.empty()) {
    result.error = QString::fromStdString(error);
    result.store.reset();
    return result;
  }

  const auto pages = ProjectSessionService::discoverPages(*result.store);
  result.pagePaths = pages.paths;
  result.pageNames = pages.names;
  result.success = true;
  return result;
}

} // namespace textfx
