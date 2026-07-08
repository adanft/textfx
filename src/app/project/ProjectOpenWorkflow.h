#pragma once

#include "app/project/ProjectSession.h"
#include "application/ports/ProjectPageTexts.h"

#include <QString>
#include <QStringList>

#include <filesystem>
#include <memory>
#include <vector>

namespace textfx {

struct ProjectOpenResult {
  bool success = false;
  QString error;
  std::unique_ptr<ProjectSession> session;
  PageTextMap pageTexts;
  std::vector<std::filesystem::path> pagePaths;
  QStringList pageNames;
};

class ProjectOpenWorkflow {
public:
  static ProjectOpenResult open(const std::filesystem::path &folder);
};

} // namespace textfx
