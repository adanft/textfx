#include "app/project/ProjectSession.h"

#include "infrastructure/persistence/ProjectStore.h"

#include <utility>

namespace textfx {

ProjectSession::ProjectSession(std::filesystem::path folder)
    : store_(std::make_unique<ProjectStore>(std::move(folder))) {}

ProjectSession::~ProjectSession() = default;

ProjectSession::ProjectSession(ProjectSession &&) noexcept = default;

ProjectSession &ProjectSession::operator=(ProjectSession &&) noexcept = default;

PageTextMap ProjectSession::loadPageTexts(std::string *error) const {
  return store_->loadPageTexts(error);
}

ProjectPages ProjectSession::discoverPages() const {
  return ProjectSessionService::discoverPages(*store_);
}

std::filesystem::path ProjectSession::rawPagePathFor(
    const std::filesystem::path &cleanPagePath) const {
  return store_->rawPagePathFor(cleanPagePath);
}

bool ProjectSession::loadPage(const std::filesystem::path &pagePath,
                              DocumentModel &document,
                              std::string *error) const {
  return store_->loadPage(pagePath, document, error);
}

bool ProjectSession::autosave(const std::filesystem::path &pagePath,
                              DocumentModel &document,
                              std::string *error) const {
  return store_->autosave(pagePath, document, error);
}

bool ProjectSession::savePage(const std::filesystem::path &pagePath,
                              const DocumentModel &document,
                              std::string *error) const {
  return store_->savePage(pagePath, document, error);
}

bool ProjectSession::savePresets(const std::vector<TextPreset> &projectPresets,
                                 std::string *error) const {
  return store_->savePresets(projectPresets, error);
}

bool ProjectSession::loadPresets(DocumentModel &document,
                                 std::vector<TextPreset> &projectPresets,
                                 std::string *error) const {
  return store_->loadPresets(document, projectPresets, error);
}

std::filesystem::path ProjectSession::pageExportPathFor(
    const std::filesystem::path &pagePath) const {
  return store_->pageExportPathFor(pagePath);
}

const IProjectExportStore &ProjectSession::exportStore() const { return *store_; }

} // namespace textfx
