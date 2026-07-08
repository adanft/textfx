#include "app/project/ProjectSession.h"

#include "infrastructure/persistence/ProjectStore.h"

#include <utility>

namespace textfx {

ProjectSession::ProjectSession(std::filesystem::path folder)
    : store_(std::make_unique<ProjectStore>(std::move(folder))) {}

ProjectSession::~ProjectSession() = default;

ProjectSession::ProjectSession(ProjectSession &&) noexcept = default;

ProjectSession &ProjectSession::operator=(ProjectSession &&) noexcept = default;

std::filesystem::path ProjectSession::pageExportPathFor(
    const std::filesystem::path &pagePath) const {
  return store_->pageExportPathFor(pagePath);
}

const IProjectPageSource &ProjectSession::pageSource() const { return *store_; }

const IProjectPageTextSource &ProjectSession::pageTextSource() const {
  return *store_;
}

const IProjectRawPageSource &ProjectSession::rawPageSource() const {
  return *store_;
}

const IProjectDocumentStore &ProjectSession::documentStore() const {
  return *store_;
}

const IProjectExportStore &ProjectSession::exportStore() const { return *store_; }

const IProjectPresetStore &ProjectSession::presetStore() const { return *store_; }

} // namespace textfx
