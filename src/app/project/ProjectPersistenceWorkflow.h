#pragma once

#include "domain/document/DocumentModel.h"

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class ProjectStore;

struct ProjectPersistenceResult {
  bool success = false;
  QString error;
};

class ProjectPersistenceWorkflow {
public:
  static ProjectPersistenceResult autosave(
      ProjectStore *store, const std::filesystem::path &pagePath,
      DocumentModel &document);

  static ProjectPersistenceResult
  savePresets(ProjectStore *store,
              const std::vector<TextPreset> &projectPresets);

  static ProjectPersistenceResult
  loadPresets(ProjectStore *store, DocumentModel &document,
              std::vector<TextPreset> &projectPresets);
};

} // namespace textfx
