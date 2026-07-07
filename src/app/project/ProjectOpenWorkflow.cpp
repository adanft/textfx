#include "app/project/ProjectOpenWorkflow.h"

namespace textfx {

ProjectOpenResult ProjectOpenWorkflow::open(const std::filesystem::path &folder) {
  ProjectOpenResult result;
  result.session = std::make_unique<ProjectSession>(folder);

  std::string error;
  result.pageTexts = result.session->loadPageTexts(&error);
  if (!error.empty()) {
    result.error = QString::fromStdString(error);
    result.session.reset();
    return result;
  }

  const auto pages = result.session->discoverPages();
  result.pagePaths = pages.paths;
  result.pageNames = pages.names;
  result.success = true;
  return result;
}

} // namespace textfx
