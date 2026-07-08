#pragma once

#include "domain/document/DocumentModel.h"

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class IProjectDocumentStore;
class IProjectPresetStore;

struct ProjectPersistenceResult {
  bool success = false;
  QString error;
};

class ProjectPersistenceWorkflow {
public:
  static ProjectPersistenceResult autosave(
      const IProjectDocumentStore *documentStore,
      const std::filesystem::path &pagePath, DocumentModel &document);

  static ProjectPersistenceResult
  savePresets(const IProjectPresetStore *presetStore,
              const std::vector<TextPreset> &projectPresets);

  static ProjectPersistenceResult
  loadPresets(const IProjectPresetStore *presetStore, DocumentModel &document,
              std::vector<TextPreset> &projectPresets);
};

} // namespace textfx
