#pragma once

#include "application/services/PageTextService.h"
#include "infrastructure/persistence/ProjectStore.h"

#include <QString>
#include <QStringList>

#include <filesystem>
#include <memory>
#include <vector>

namespace textfx {

struct ProjectOpenResult {
  bool success = false;
  QString error;
  std::unique_ptr<ProjectStore> store;
  PageTextMap pageTexts;
  std::vector<std::filesystem::path> pagePaths;
  QStringList pageNames;
};

class ProjectOpenWorkflow {
public:
  static ProjectOpenResult open(const std::filesystem::path &folder);
};

} // namespace textfx
