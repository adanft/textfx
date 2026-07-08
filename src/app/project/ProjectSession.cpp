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

std::filesystem::path ProjectSession::pageExportPathFor(
    const std::filesystem::path &pagePath) const {
  return store_->pageExportPathFor(pagePath);
}

const IProjectDocumentStore &ProjectSession::documentStore() const {
  return *store_;
}

const IProjectExportStore &ProjectSession::exportStore() const { return *store_; }

const IProjectPresetStore &ProjectSession::presetStore() const { return *store_; }

} // namespace textfx
