#pragma once

#include <QString>

#include <filesystem>

namespace textfx {

struct ProjectCreationResult {
  bool success = false;
  QString error;
};

class ProjectCreationWorkflow {
public:
  static ProjectCreationResult createStructure(const std::filesystem::path &root);
};

} // namespace textfx
