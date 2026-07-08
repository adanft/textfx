#include "app/project/ProjectPersistenceWorkflow.h"

#include "app/project/ProjectSession.h"
#include "application/ports/IProjectDocumentStore.h"

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
    ProjectSession *session, const std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!session)
    return result;

  std::string error;
  if (!session->savePresets(projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

ProjectPersistenceResult ProjectPersistenceWorkflow::loadPresets(
    ProjectSession *session, DocumentModel &document,
    std::vector<TextPreset> &projectPresets) {
  ProjectPersistenceResult result;
  if (!session)
    return result;

  std::string error;
  if (!session->loadPresets(document, projectPresets, &error)) {
    result.error = toQString(error);
    return result;
  }

  result.success = true;
  return result;
}

} // namespace textfx
