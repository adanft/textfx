#include "app/project/ProjectPageLoadWorkflow.h"

#include "application/services/ProjectSessionService.h"
#include "app/project/ProjectSession.h"

namespace textfx {

int ProjectPageLoadWorkflow::normalizeRequestedIndex(
    int requestedIndex, const std::vector<std::filesystem::path> &pagePaths) {
  return ProjectSessionService::normalizePageIndex(requestedIndex,
                                                  pagePaths.size());
}

ProjectPageLoadResult ProjectPageLoadWorkflow::load(
    ProjectSession *session, int requestedIndex,
    const std::vector<std::filesystem::path> &pagePaths) {
  ProjectPageLoadResult result;
  if (!session)
    return result;

  const int normalizedIndex = normalizeRequestedIndex(requestedIndex, pagePaths);
  if (normalizedIndex < 0)
    return result;

  result.index = normalizedIndex;
  result.page = pagePaths.at(static_cast<std::size_t>(normalizedIndex));

  std::string error;
  if (!session->loadPage(result.page, result.document, &error)) {
    result.error = QString::fromStdString(error);
    return result;
  }

  result.success = true;
  return result;
}

} // namespace textfx
