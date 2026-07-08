#include "app/project/ProjectPersistenceWorkflow.h"

#include "application/ports/IProjectDocumentStore.h"
#include "application/ports/IProjectPresetStore.h"

namespace textfx {
namespace {
QString toQString(const std::string &value) { return QString::fromStdString(value); }
} // namespace

ProjectPersistenceResult ProjectPersistenceWorkflow::autosave(
    const IProjectDocumentStore *documentStore,
    const std::filesystem::path &pagePath, DocumentModel &document) {
  ProjectPersistenceResult result;
  if (!documentStore)
    return result;

  std::string error;
  if (!documentStore->autosave(pagePath, document, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

ProjectPersistenceResult ProjectPersistenceWorkflow::savePresets(
    const IProjectPresetStore *presetStore,
    const std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!presetStore)
    return result;

  std::string error;
  if (!presetStore->savePresets(projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

ProjectPersistenceResult ProjectPersistenceWorkflow::loadPresets(
    const IProjectPresetStore *presetStore, DocumentModel &document,
    std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!presetStore)
    return result;

  std::string error;
  if (!presetStore->loadPresets(document, projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

} // namespace textfx
