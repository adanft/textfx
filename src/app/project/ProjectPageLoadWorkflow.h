#pragma once

#include "domain/document/DocumentModel.h"

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class ProjectStore;

struct ProjectPageLoadResult {
  bool success = false;
  QString error;
  int index = -1;
  std::filesystem::path page;
  DocumentModel document;
};

class ProjectPageLoadWorkflow {
public:
  static int normalizeRequestedIndex(
      int requestedIndex, const std::vector<std::filesystem::path> &pagePaths);

  static ProjectPageLoadResult
  load(ProjectStore *store, int requestedIndex,
       const std::vector<std::filesystem::path> &pagePaths);
};

} // namespace textfx
