#include "app/project/ProjectPersistenceWorkflow.h"

#include "infrastructure/persistence/ProjectStore.h"

namespace textfx {
namespace {
QString toQString(const std::string &value) { return QString::fromStdString(value); }
} // namespace

ProjectPersistenceResult ProjectPersistenceWorkflow::autosave(
    ProjectStore *store, const std::filesystem::path &pagePath,
    DocumentModel &document) {
  ProjectPersistenceResult result;
  if (!store)
    return result;

  std::string error;
  if (!store->autosave(pagePath, document, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

ProjectPersistenceResult ProjectPersistenceWorkflow::savePresets(
    ProjectStore *store, const std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!store)
    return result;

  std::string error;
  if (!store->savePresets(projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

ProjectPersistenceResult ProjectPersistenceWorkflow::loadPresets(
    ProjectStore *store, DocumentModel &document,
    std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!store)
    return result;

  std::string error;
  if (!store->loadPresets(document, projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

} // namespace textfx
