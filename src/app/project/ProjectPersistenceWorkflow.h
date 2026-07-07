#pragma once

#include "domain/document/DocumentModel.h"

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class ProjectSession;

struct ProjectPersistenceResult {
  bool success = false;
  QString error;
};

class ProjectPersistenceWorkflow {
public:
  static ProjectPersistenceResult autosave(
      ProjectSession *session, const std::filesystem::path &pagePath,
      DocumentModel &document);

  static ProjectPersistenceResult
  savePresets(ProjectSession *session,
              const std::vector<TextPreset> &projectPresets);

  static ProjectPersistenceResult
  loadPresets(ProjectSession *session, DocumentModel &document,
              std::vector<TextPreset> &projectPresets);
};

} // namespace textfx
