#pragma once

#include "application/ports/IProjectDocumentStore.h"
#include "application/ports/IProjectExportStore.h"
#include "application/ports/IProjectPageSource.h"
#include "application/ports/IProjectPageTextSource.h"
#include "application/ports/IProjectPresetStore.h"
#include "application/ports/IProjectRawPageSource.h"
#include "domain/document/DocumentModel.h"

#include <filesystem>
#include <memory>

namespace textfx {

class ProjectStore;

class ProjectSession {
public:
  explicit ProjectSession(std::filesystem::path folder);
  ~ProjectSession();

  ProjectSession(ProjectSession &&) noexcept;
  ProjectSession &operator=(ProjectSession &&) noexcept;

  ProjectSession(const ProjectSession &) = delete;
  ProjectSession &operator=(const ProjectSession &) = delete;

  const IProjectPageSource &pageSource() const;
  const IProjectPageTextSource &pageTextSource() const;
  const IProjectRawPageSource &rawPageSource() const;
  const IProjectDocumentStore &documentStore() const;
  const IProjectExportStore &exportStore() const;
  const IProjectPresetStore &presetStore() const;

private:
  std::unique_ptr<ProjectStore> store_;
};

} // namespace textfx
